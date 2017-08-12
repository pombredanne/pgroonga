CREATE TABLE tags (
  name text PRIMARY KEY
);

CREATE TABLE tag_readings (
  tag_name text
    REFERENCES tags ON DELETE CASCADE ON UPDATE CASCADE,
  katakana text,
  PRIMARY KEY (tag_name, katakana)
);

INSERT INTO tags VALUES ('PostgreSQL');
INSERT INTO tags VALUES ('Groonga');
INSERT INTO tags VALUES ('PGroonga');
INSERT INTO tags VALUES ('pglogical');

INSERT INTO tag_readings VALUES ('PostgreSQL', 'ポストグレスキューエル');
INSERT INTO tag_readings VALUES ('PostgreSQL', 'ポスグレ');
INSERT INTO tag_readings VALUES ('Groonga', 'グルンガ');
INSERT INTO tag_readings VALUES ('PGroonga', 'ピージールンガ');
INSERT INTO tag_readings VALUES ('pglogical', 'ピージーロジカル');

CREATE INDEX pgrn_tags_index ON tags
  USING pgroonga (name pgroonga_text_term_search_ops_v2);
CREATE INDEX pgrn_tag_readings_index ON tag_readings
  USING pgroonga (tag_name, katakana pgroonga_text_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SELECT * FROM (
  SELECT name, pgroonga_score(tags) AS score
  FROM tags
  WHERE name &^ 'Groon'
  UNION
  SELECT tag_name AS name, pgroonga_score(tag_readings) AS score
  FROM tag_readings
  WHERE katakana &^~ 'posu'
) AS candidates
ORDER BY name;

DROP TABLE tag_readings;
DROP TABLE tags;
