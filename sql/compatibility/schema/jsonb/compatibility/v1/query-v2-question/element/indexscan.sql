CREATE TABLE fruits (
  id int,
  items jsonb
);

INSERT INTO fruits VALUES (1, '["apple"]');
INSERT INTO fruits VALUES (2, '["banana", "apple"]');
INSERT INTO fruits VALUES (3, '["peach"]');

CREATE INDEX pgroonga_index ON fruits
  USING pgroonga (items pgroonga.jsonb_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id, items
  FROM fruits
 WHERE items &? 'banana OR peach'
 ORDER BY id;

SELECT id, items
  FROM fruits
 WHERE items &? 'banana OR peach'
 ORDER BY id;

DROP TABLE fruits;
