#include "pgroonga.h"

#include "pgrn-compatible.h"

#include "pgrn-global.h"
#include "pgrn-query-expand.h"

#include <access/heapam.h>
#include <access/relscan.h>
#include <catalog/pg_operator.h>
#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>
#include <utils/rel.h>
#include <utils/snapmgr.h>

#include <groonga/plugin.h>

#define PGRN_EXPANDER_NAME			"QueryExpanderPostgreSQL"
#define PGRN_EXPANDER_NAME_LENGTH	(sizeof(PGRN_EXPANDER_NAME) - 1)

typedef struct {
	Relation table;
	AttrNumber termAttributeNumber;
	Form_pg_attribute synonymsAttribute;
	Snapshot snapshot;
	IndexScanDesc scan;
	StrategyNumber scanStrategy;
	RegProcedure scanProcedure;
} PGrnQueryExpandData;

static grn_ctx *ctx = &PGrnContext;
static PGrnQueryExpandData currentData;

PGRN_FUNCTION_INFO_V1(pgroonga_query_expand);

static grn_obj *
func_query_expander_postgresql(grn_ctx *ctx,
							   int nargs,
							   grn_obj **args,
							   grn_user_data *user_data)
{
	grn_rc rc = GRN_END_OF_DATA;
	grn_obj *term;
	grn_obj *expandedTerm;
	text *termText;
	ScanKeyData scanKeys[1];
	int nKeys = 1;
	HeapScanDesc heapScan = NULL;
	HeapTuple tuple;
	int ith_synonyms = 0;

	term = args[0];
	expandedTerm = args[1];

	termText = cstring_to_text_with_len(GRN_TEXT_VALUE(term),
										GRN_TEXT_LEN(term));
	if (currentData.scan)
	{
		ScanKeyInit(&(scanKeys[0]),
					currentData.termAttributeNumber,
					currentData.scanStrategy,
					currentData.scanProcedure,
					PointerGetDatum(termText));
		index_rescan(currentData.scan, scanKeys, nKeys, NULL, 0);
	}
	else
	{
		ScanKeyInit(&(scanKeys[0]),
					currentData.termAttributeNumber,
					InvalidStrategy,
					currentData.scanProcedure,
					PointerGetDatum(termText));
		heapScan = heap_beginscan(currentData.table,
								  currentData.snapshot,
								  nKeys,
								  scanKeys);
	}

	while (true)
	{
		Datum synonymsDatum;
		bool isNULL;

		if (currentData.scan)
			tuple = index_getnext(currentData.scan, ForwardScanDirection);
		else
			tuple = heap_getnext(heapScan, ForwardScanDirection);

		if (!tuple)
			break;

		synonymsDatum = heap_getattr(tuple,
									 currentData.synonymsAttribute->attnum,
									 RelationGetDescr(currentData.table),
									 &isNULL);
		if (isNULL)
			continue;

		if (currentData.synonymsAttribute->atttypid == TEXTOID)
		{
			text *synonym;
			synonym = DatumGetTextP(synonymsDatum);

			if (ith_synonyms == 0)
				GRN_TEXT_PUTC(ctx, expandedTerm, '(');
			else
				GRN_TEXT_PUTS(ctx, expandedTerm, " OR ");

			GRN_TEXT_PUTC(ctx, expandedTerm, '(');
			GRN_TEXT_PUT(ctx, expandedTerm,
						 VARDATA_ANY(synonym),
						 VARSIZE_ANY_EXHDR(synonym));
			GRN_TEXT_PUTC(ctx, expandedTerm, ')');
		}
		else
		{
			ArrayType *synonymsArray;
			int i, n;

			synonymsArray = DatumGetArrayTypeP(synonymsDatum);
			if (ARR_NDIM(synonymsArray) == 0)
				continue;

			n = ARR_DIMS(synonymsArray)[0];
			if (n == 0)
				continue;

			if (ith_synonyms == 0)
				GRN_TEXT_PUTC(ctx, expandedTerm, '(');
			else
				GRN_TEXT_PUTS(ctx, expandedTerm, " OR ");

			for (i = 1; i <= n; i++)
			{
				Datum synonymDatum;
				bool isNULL;
				text *synonym;

				synonymDatum = array_ref(synonymsArray, 1, &i, -1,
										 currentData.synonymsAttribute->attlen,
										 currentData.synonymsAttribute->attbyval,
										 currentData.synonymsAttribute->attalign,
										 &isNULL);
				synonym = DatumGetTextP(synonymDatum);
				if (i > 1)
					GRN_TEXT_PUTS(ctx, expandedTerm, " OR ");
				GRN_TEXT_PUTC(ctx, expandedTerm, '(');
				GRN_TEXT_PUT(ctx, expandedTerm,
							 VARDATA_ANY(synonym),
							 VARSIZE_ANY_EXHDR(synonym));
				GRN_TEXT_PUTC(ctx, expandedTerm, ')');
			}
		}

		ith_synonyms++;

		rc = GRN_SUCCESS;
	}
	if (ith_synonyms > 0)
		GRN_TEXT_PUTC(ctx, expandedTerm, ')');

	if (heapScan)
		heap_endscan(heapScan);

	{
		grn_obj *rc_object;

		rc_object = grn_plugin_proc_alloc(ctx, user_data, GRN_DB_INT32, 0);
		if (rc_object)
			GRN_INT32_SET(ctx, rc_object, rc);

		return rc_object;
	}
}

void
PGrnInitializeQueryExpand(void)
{
	grn_proc_create(ctx,
					PGRN_EXPANDER_NAME,
					PGRN_EXPANDER_NAME_LENGTH,
					GRN_PROC_FUNCTION,
					func_query_expander_postgresql, NULL, NULL,
					0, NULL);
}

static Form_pg_attribute
PGrnFindSynonymsAttribute(const char *tableName,
						  Relation table,
						  const char *columnName,
						  size_t columnNameSize)
{
	TupleDesc desc;
	int i;

	desc = RelationGetDescr(table);
	for (i = 1; i <= desc->natts; i++)
	{
		Form_pg_attribute attribute = desc->attrs[i - 1];

		if (strlen(attribute->attname.data) != columnNameSize)
			continue;
		if (strncmp(attribute->attname.data, columnName, columnNameSize) != 0)
			continue;

		if (!(attribute->atttypid == TEXTOID ||
			  attribute->atttypid == TEXTARRAYOID))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_NAME),
					 errmsg("pgroonga: query_expand: "
							"synonyms column isn't text type nor text[] type: "
							"<%s>.<%.*s>",
							tableName,
							(int)columnNameSize, columnName)));
		}

		return attribute;
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_NAME),
			 errmsg("pgroonga: query_expand: "
					"synonyms column doesn't exist: <%s>.<%.*s>",
					tableName,
					(int)columnNameSize, columnName)));

	return NULL;
}

static Relation
PGrnFindTermIndex(Relation table,
				  const char *columnName,
				  size_t columnNameSize,
				  Oid opNo,
				  AttrNumber *indexAttributeNumber,
				  StrategyNumber *indexStrategy)
{
	Relation termIndex = InvalidRelation;
	Relation preferedIndex = InvalidRelation;
	List *indexOIDList;
	ListCell *cell;

	indexOIDList = RelationGetIndexList(table);
	foreach(cell, indexOIDList)
	{
		Relation index = InvalidRelation;
		Oid indexOID = lfirst_oid(cell);
		int i;

		index = index_open(indexOID, NoLock);
		for (i = 1; i <= index->rd_att->natts; i++)
		{
			const char *name = index->rd_att->attrs[i - 1]->attname.data;
			Oid opFamily;
			StrategyNumber strategy;

			if (strlen(name) != columnNameSize)
				continue;

			if (memcmp(name, columnName, columnNameSize) != 0)
				continue;

			opFamily = index->rd_opfamily[i - 1];
			strategy = get_op_opfamily_strategy(opNo, opFamily);
			if (strategy == InvalidStrategy)
				continue;

			if (PGrnIndexIsPGroonga(index))
			{
				preferedIndex = index;
				*indexStrategy = strategy;
				*indexAttributeNumber = i;
				break;
			}

			if (!RelationIsValid(termIndex))
			{
				termIndex = index;
				*indexStrategy = strategy;
				*indexAttributeNumber = i;
			}

			break;
		}

		if (RelationIsValid(preferedIndex))
			break;

		if (termIndex == index)
			continue;

		index_close(index, NoLock);
		index = InvalidRelation;
	}
	list_free(indexOIDList);

	if (RelationIsValid(preferedIndex))
	{
		if (RelationIsValid(termIndex) && termIndex != preferedIndex)
			index_close(termIndex, NoLock);
		return preferedIndex;
	}
	else
	{
		return termIndex;
	}
}

static AttrNumber
PGrnFindTermAttributeNumber(const char *tableName,
							Relation table,
							const char *columnName,
							size_t columnNameSize)
{
	TupleDesc desc;
	int i;

	desc = RelationGetDescr(table);
	for (i = 1; i <= desc->natts; i++)
	{
		Form_pg_attribute attribute = desc->attrs[i - 1];

		if (strlen(attribute->attname.data) != columnNameSize)
			continue;
		if (strncmp(attribute->attname.data, columnName, columnNameSize) != 0)
			continue;

		if (attribute->atttypid != TEXTOID)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_NAME),
					 errmsg("pgroonga: query_expand: "
							"term column isn't text type: <%s>.<%.*s>",
							tableName,
							(int)columnNameSize, columnName)));
		}

		return attribute->attnum;
	}

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_NAME),
			 errmsg("pgroonga: query_expand: "
					"term column doesn't exist: <%s>.<%.*s>",
					tableName,
					(int)columnNameSize, columnName)));

	return InvalidAttrNumber;
}

/**
 * pgroonga.query_expand(tableName cstring,
 *                       termColumnName text,
 *                       synonymsColumnName text,
 *                       query text) : text
 */
Datum
pgroonga_query_expand(PG_FUNCTION_ARGS)
{
	Datum tableNameDatum = PG_GETARG_DATUM(0);
	text *termColumnName = PG_GETARG_TEXT_PP(1);
	text *synonymsColumnName = PG_GETARG_TEXT_PP(2);
	text *query = PG_GETARG_TEXT_PP(3);
	Datum tableOIDDatum;
	Oid tableOID;
	Relation index;
	Oid opNo = TextEqualOperator;
	grn_obj expandedQuery;

	tableOIDDatum = DirectFunctionCall1(regclassin, tableNameDatum);
	if (!OidIsValid(tableOIDDatum))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_NAME),
				 errmsg("pgroonga: query_expand: unknown table name: <%s>",
						DatumGetCString(tableNameDatum))));
	}
	tableOID = DatumGetObjectId(tableOIDDatum);
	currentData.table = RelationIdGetRelation(tableOID);

	currentData.synonymsAttribute =
		PGrnFindSynonymsAttribute(DatumGetCString(tableNameDatum),
								  currentData.table,
								  VARDATA_ANY(synonymsColumnName),
								  VARSIZE_ANY_EXHDR(synonymsColumnName));

	index = PGrnFindTermIndex(currentData.table,
							  VARDATA_ANY(termColumnName),
							  VARSIZE_ANY_EXHDR(termColumnName),
							  opNo,
							  &(currentData.termAttributeNumber),
							  &(currentData.scanStrategy));
	if (!index)
		currentData.termAttributeNumber =
			PGrnFindTermAttributeNumber(DatumGetCString(tableNameDatum),
										currentData.table,
										VARDATA_ANY(termColumnName),
										VARSIZE_ANY_EXHDR(termColumnName));

	currentData.snapshot = GetActiveSnapshot();
	if (index)
	{
		int nKeys = 1;
		int nOrderBys = 0;
		currentData.scan = index_beginscan(currentData.table,
										   index,
										   currentData.snapshot,
										   nKeys,
										   nOrderBys);
	}
	else
	{
		currentData.scan = NULL;
	}

	currentData.scanProcedure = get_opcode(opNo);

	GRN_TEXT_INIT(&expandedQuery, 0);
	grn_expr_syntax_expand_query(ctx,
								 VARDATA_ANY(query),
								 VARSIZE_ANY_EXHDR(query),
								 GRN_EXPR_SYNTAX_QUERY,
								 grn_ctx_get(ctx,
											 PGRN_EXPANDER_NAME,
											 PGRN_EXPANDER_NAME_LENGTH),
								 &expandedQuery);
	if (currentData.scan)
	{
		index_endscan(currentData.scan);
		index_close(index, NoLock);
	}

	RelationClose(currentData.table);

	{
		text *expandedQueryText;

		expandedQueryText =
			cstring_to_text_with_len(GRN_TEXT_VALUE(&expandedQuery),
									 GRN_TEXT_LEN(&expandedQuery));
		GRN_OBJ_FIN(ctx, &expandedQuery);

		PG_RETURN_TEXT_P(expandedQueryText);
	}
}

