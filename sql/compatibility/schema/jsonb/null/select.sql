CREATE TABLE logs (
  record jsonb
);

CREATE INDEX pgroonga_index ON logs
  USING pgroonga (record pgroonga.jsonb_ops_v2);

INSERT INTO logs VALUES ('{}');

SELECT *
  FROM logs
 WHERE record &@ null;

DROP TABLE logs;
