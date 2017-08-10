CREATE TABLE fruits (
  id int,
  items jsonb
);

INSERT INTO fruits VALUES (1, '["apple"]');
INSERT INTO fruits VALUES (2, '["banana", "apple"]');
INSERT INTO fruits VALUES (3, '["peach"]');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id, items
  FROM fruits
 WHERE items &? 'banana OR peach'
 ORDER BY id;

DROP TABLE fruits;
