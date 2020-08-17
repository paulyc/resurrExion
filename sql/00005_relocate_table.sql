ALTER TABLE `cluster` DROP CONSTRAINT `consolidated_cluster_idx`;
ALTER TABLE `cluster` DROP KEY `cluster_big_indx`;
ALTER TABLE `cluster` DROP `consolidated_cluster`;

CREATE TABLE `relocate` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `from_cluster` bigint(20) unsigned NOT NULL,
  `to_cluster` bigint(20) unsigned NOT NULL,
  `status` int DEFAULT 0 NOT NULL,
  PRIMARY KEY (`id`),
  KEY `relocate_from_cluster_idx` (`from_cluster`),
  KEY `relocate_to_cluster_idx` (`to_cluster`),
  CONSTRAINT `fk_relocate_from_cluster` FOREIGN KEY (`from_cluster`) REFERENCES `cluster` (`cluster`),
  CONSTRAINT `fk_relocate_to_cluster` FOREIGN KEY (`to_cluster`) REFERENCES `cluster` (`cluster`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
