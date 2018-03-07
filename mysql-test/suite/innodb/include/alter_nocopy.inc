CREATE TABLE t1(f1 INT NOT NULL, f2 INT NOT NULL,
                f3 INT AS (f2 * f2) VIRTUAL,
		f4 INT NOT NULL,
                PRIMARY KEY(f1),
		INDEX`idx`(f2),
		UNIQUE INDEX `uidx` (f4))ENGINE=INNODB;

CREATE TABLE t2(f1 INT NOT NULL, f2 INT NOT NULL,
                INDEX(f1))ENGINE=INNODB;

INSERT INTO t1(f1, f2, f4) VALUES(1, 2, 3);

--enable_info
--echo # Add secondary index
--error $error_code
--eval ALTER TABLE t1 ADD INDEX `idx1`(f4), ALGORITHM=$algorithm_type

--echo # Drop secondary index
--error $error_code
--eval ALTER TABLE t1 DROP INDEX `idx`, ALGORITHM=$algorithm_type

--echo # Add Unique Index
--error $error_code
--eval ALTER TABLE t1 ADD UNIQUE INDEX `u1`(f2), ALGORITHM=$algorithm_type

--echo # Drop Unique Index
--error $error_code
--eval ALTER TABLE t1 DROP INDEX `uidx`, ALGORITHM=$algorithm_type

--echo # Add foreign & Drop foreign key
SET foreign_key_checks = 0;
--error $error_code
--eval ALTER TABLE t1 ADD FOREIGN KEY(f1) REFERENCES t2(f1), ALGORITHM=$algorithm_type
SET foreign_key_checks = 1;

DROP TABLE t1, t2;
--disable_info
