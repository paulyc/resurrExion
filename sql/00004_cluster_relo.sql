ALTER TABLE `cluster`
  ADD COLUMN `consolidated_cluster` BIGINT(20) UNSIGNED NULL;

ALTER TABLE `cluster`
  ADD INDEX `consolidated_cluster_idx`(`consolidated_cluster` ASC);

ALTER TABLE `cluster`
  ADD CONSTRAINT
    FOREIGN KEY `consolidated_cluster_idx` (`consolidated_cluster`)
    REFERENCES `cluster` (`cluster`)
  ON DELETE RESTRICT
  ON UPDATE RESTRICT;
