CREATE TABLE memos (
  title text,
  tags varchar(1023)[]
);

INSERT INTO memos VALUES ('PostgreSQL', ARRAY['PostgreSQL']);
INSERT INTO memos VALUES ('Groonga', ARRAY['Groonga']);
INSERT INTO memos VALUES ('PGroonga', ARRAY['PostgreSQL', 'Groonga']);

CREATE INDEX pgroonga_memos_index ON memos
  USING pgroonga (tags pgroonga_varchar_array_term_search_ops_v2);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT title, tags
  FROM memos
 WHERE tags %% 'Groonga';

DROP TABLE memos;
