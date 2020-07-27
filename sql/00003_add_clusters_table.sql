-- -----------------------------------------------------
-- Table `cluster`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `cluster` (
  `id` BIGINT(20) UNSIGNED NOT NULL AUTO_INCREMENT,
  `cluster` BIGINT(20) UNSIGNED NOT NULL,
  `allocated` TINYINT(1) NULL,
  `type` VARCHAR(255) NULL,
  `next` BIGINT(20) UNSIGNED NULL,
  `file` BIGINT(20) UNSIGNED NULL,
  PRIMARY KEY (`id`),
  INDEX `index2` (`cluster` ASC),
  INDEX `index3` (`type` ASC),
  INDEX `fk_cluster_1_idx` (`next` ASC),
  INDEX `fk_cluster_2_idx` (`file` ASC),
  CONSTRAINT `fk_cluster_1`
    FOREIGN KEY (`next`)
    REFERENCES `cluster` (`cluster`)
    ON DELETE RESTRICT
    ON UPDATE RESTRICT,
  CONSTRAINT `fk_cluster_2`
    FOREIGN KEY (`file`)
    REFERENCES `file` (`entry_offset`)
    ON DELETE RESTRICT
    ON UPDATE RESTRICT)
ENGINE = InnoDB
DEFAULT CHARACTER SET = utf8mb4;
