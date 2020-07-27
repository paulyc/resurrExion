//
//  database.hpp
//  resurrExion
//
//  Created by Paul Ciarlo on 2 July 2020
//
//  Copyright (C) 2020 Paul Ciarlo <paul.ciarlo@gmail.com>
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//

#ifndef _github_paulyc_database_hpp_
#define _github_paulyc_database_hpp_

#include <string>
#include <memory>

#include <mariadbcpp/ConnCpp.h>

#include "quick.hpp"

namespace model {

struct Entity
{
};

/*
CREATE TABLE `directory` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(255) DEFAULT NULL,
  `entry_offset` bigint(20) unsigned NOT NULL,
  `parent_directory_offset` bigint(20) unsigned DEFAULT NULL,
  `parent_directory_id` bigint(20) unsigned DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `fk_directory_1_idx` (`parent_directory_offset`),
  KEY `index3` (`entry_offset`),
  CONSTRAINT `fk_directory_1` FOREIGN KEY (`parent_directory_offset`) REFERENCES `directory` (`entry_offset`)
) ENGINE=InnoDB AUTO_INCREMENT=20683 DEFAULT CHARSET=utf8mb
*/
struct Directory
{
};

/*
 CREATE TABLE `file` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `entry_offset` bigint(20) unsigned NOT NULL,
  `parent_directory_offset` bigint(20) unsigned DEFAULT NULL,
  `name` varchar(255) DEFAULT NULL,
  `data_offset` bigint(20) unsigned DEFAULT NULL,
  `data_len` bigint(20) unsigned DEFAULT NULL,
  `is_contiguous` tinyint(1) DEFAULT NULL,
  `is_copied_off` tinyint(1) DEFAULT NULL,
  `parent_directory_id` bigint(20) unsigned DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `fk_file_1_idx` (`parent_directory_offset`),
  KEY `index3` (`entry_offset`),
  CONSTRAINT `fk_file_1` FOREIGN KEY (`parent_directory_offset`) REFERENCES `directory` (`entry_offset`)
) ENGINE=InnoDB AUTO_INCREMENT=220283 DEFAULT CHARSET=utf8mb4
*/
struct File
{
};

/*
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
*/
struct Cluster
{
};

}

class Database
{
public:
    Database(const std::string &dev, const std::string &user, const std::string &pass, const std::string &sock, const std::string &db);
    ~Database();
    //void migrate_to_sql_ids();
    void rescue_music();
    void rescue_orphan_dirs();
    void rescue_orphan_files(const char *dir);
    void rescue_dupe_orphan_files(const char *dir);
    void fill_allocated_clusters();
//private:
    sql::Connection *_conn;
    FilesystemStub _stub;
};

#endif /* _github_paulyc_database_hpp_ */
