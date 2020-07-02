ALTER TABLE `file` ADD `parent_directory_id` BIGINT(20) UNSIGNED NULL;
ALTER TABLE `directory` ADD `parent_directory_id` BIGINT(20) UNSIGNED NULL;
