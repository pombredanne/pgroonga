#include "pgroonga.h"

#include "pgrn_global.h"
#include "pgrn_groonga.h"

#include <utils/builtins.h>
#include <utils/timestamp.h>

static grn_ctx *ctx = &PGrnContext;
static struct PGrnBuffers *buffers = &PGrnBuffers;

PG_FUNCTION_INFO_V1(pgroonga_escape_query);
PG_FUNCTION_INFO_V1(pgroonga_escape_string);
PG_FUNCTION_INFO_V1(pgroonga_escape_boolean);
PG_FUNCTION_INFO_V1(pgroonga_escape_int2);
PG_FUNCTION_INFO_V1(pgroonga_escape_int4);
PG_FUNCTION_INFO_V1(pgroonga_escape_int8);
PG_FUNCTION_INFO_V1(pgroonga_escape_float4);
PG_FUNCTION_INFO_V1(pgroonga_escape_float8);
PG_FUNCTION_INFO_V1(pgroonga_escape_timestamptz);

/**
 * pgroonga.escape_query(query text) : text
 */
Datum
pgroonga_escape_query(PG_FUNCTION_ARGS)
{
	grn_rc rc = GRN_SUCCESS;
	text *query = PG_GETARG_TEXT_PP(0);
	text *escapedQuery;
	grn_obj *escapedQueryBuffer;

	escapedQueryBuffer = &(buffers->escape.escapedValue);
	GRN_BULK_REWIND(escapedQueryBuffer);
	rc = grn_expr_syntax_escape_query(ctx,
									  VARDATA_ANY(query),
									  VARSIZE_ANY_EXHDR(query),
									  escapedQueryBuffer);

	if (rc != GRN_SUCCESS) {
		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: escape_query: failed to escape")));
	}

	escapedQuery = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedQueryBuffer),
											GRN_TEXT_LEN(escapedQueryBuffer));
	PG_RETURN_TEXT_P(escapedQuery);
}

/**
 * pgroonga.escape(value text, special_characters text = '"\') : text
 */
Datum
pgroonga_escape_string(PG_FUNCTION_ARGS)
{
	grn_rc rc = GRN_SUCCESS;
	text *value = PG_GETARG_TEXT_PP(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;
	grn_obj *specialCharactersBuffer;

	escapedValueBuffer = &(buffers->escape.escapedValue);
	specialCharactersBuffer = &(buffers->escape.specialCharacters);

	GRN_BULK_REWIND(specialCharactersBuffer);
	GRN_TEXT_PUTC(ctx, escapedValueBuffer, '"');
	if (PG_NARGS() == 1)
	{
		GRN_TEXT_SETS(ctx, specialCharactersBuffer, "\"\\");
	}
	else
	{
		text *specialCharacters = PG_GETARG_TEXT_PP(1);

		GRN_TEXT_SET(ctx,
					 specialCharactersBuffer,
					 VARDATA_ANY(specialCharacters),
					 VARSIZE_ANY_EXHDR(specialCharacters));
		GRN_TEXT_PUTC(ctx, specialCharactersBuffer, '\0');
	}

	rc = grn_expr_syntax_escape(ctx,
								VARDATA_ANY(value),
								VARSIZE_ANY_EXHDR(value),
								GRN_TEXT_VALUE(specialCharactersBuffer),
								'\\',
								escapedValueBuffer);
	if (rc != GRN_SUCCESS) {
		ereport(ERROR,
				(errcode(PGrnRCToPgErrorCode(rc)),
				 errmsg("pgroonga: escape: value: failed to escape")));
	}
	GRN_TEXT_PUTC(ctx, escapedValueBuffer, '"');

	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}

/**
 * pgroonga.escape(value boolean) : text
 */
Datum
pgroonga_escape_boolean(PG_FUNCTION_ARGS)
{
	bool value = PG_GETARG_BOOL(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;

	escapedValueBuffer = &(buffers->escape.escapedValue);
	if (value)
	{
		GRN_TEXT_SETS(ctx, escapedValueBuffer, "true");
	}
	else
	{
		GRN_TEXT_SETS(ctx, escapedValueBuffer, "false");
	}

	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}

/**
 * pgroonga.escape(value int2) : text
 */
Datum
pgroonga_escape_int2(PG_FUNCTION_ARGS)
{
	int16 value = PG_GETARG_INT16(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;

	escapedValueBuffer = &(buffers->escape.escapedValue);
	GRN_BULK_REWIND(escapedValueBuffer);
	grn_text_itoa(ctx, escapedValueBuffer, value);

	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}

/**
 * pgroonga.escape(value int4) : text
 */
Datum
pgroonga_escape_int4(PG_FUNCTION_ARGS)
{
	int32 value = PG_GETARG_INT32(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;

	escapedValueBuffer = &(buffers->escape.escapedValue);
	GRN_BULK_REWIND(escapedValueBuffer);
	grn_text_itoa(ctx, escapedValueBuffer, value);

	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}

/**
 * pgroonga.escape(value int8) : text
 */
Datum
pgroonga_escape_int8(PG_FUNCTION_ARGS)
{
	int64 value = PG_GETARG_INT64(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;

	escapedValueBuffer = &(buffers->escape.escapedValue);
	GRN_BULK_REWIND(escapedValueBuffer);
	grn_text_lltoa(ctx, escapedValueBuffer, value);

	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}

/**
 * pgroonga.escape(value float4) : text
 */
Datum
pgroonga_escape_float4(PG_FUNCTION_ARGS)
{
	float value = PG_GETARG_FLOAT4(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;

	escapedValueBuffer = &(buffers->escape.escapedValue);
	GRN_BULK_REWIND(escapedValueBuffer);
	grn_text_ftoa(ctx, escapedValueBuffer, value);

	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}

/**
 * pgroonga.escape(value float8) : text
 */
Datum
pgroonga_escape_float8(PG_FUNCTION_ARGS)
{
	double value = PG_GETARG_FLOAT8(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;

	escapedValueBuffer = &(buffers->escape.escapedValue);
	GRN_BULK_REWIND(escapedValueBuffer);
	grn_text_ftoa(ctx, escapedValueBuffer, value);

	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}

/**
 * pgroonga.escape(value timestamptz) : text
 */
Datum
pgroonga_escape_timestamptz(PG_FUNCTION_ARGS)
{
	TimestampTz value = PG_GETARG_TIMESTAMPTZ(0);
	text *escapedValue;
	grn_obj *escapedValueBuffer;

	escapedValueBuffer = &(buffers->escape.escapedValue);
	GRN_BULK_REWIND(escapedValueBuffer);
#ifdef HAVE_INT64_TIMESTAMP
	grn_text_lltoa(ctx, escapedValueBuffer, timestamptz_to_time_t(value));
#else
	grn_text_ftoa(ctx, escapedValueBuffer, timestamptz_to_time_t(value));
#endif

	escapedValue = cstring_to_text_with_len(GRN_TEXT_VALUE(escapedValueBuffer),
											GRN_TEXT_LEN(escapedValueBuffer));
	PG_RETURN_TEXT_P(escapedValue);
}