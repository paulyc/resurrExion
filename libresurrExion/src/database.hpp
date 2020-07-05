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

#include <mariadb++/connection.hpp>

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

}

class Database
{
public:
    Database(const std::string &user, const std::string &pass, const std::string &sock, const std::string &db);
    ~Database();
    //void migrate_to_sql_ids();
    void rescue_directories(const std::string &rescuedir);
    void rescue_music(const std::string &rescuedir);
    //void rescue_photos();
    //void rescue_x();
//private:
    mariadb::connection_ref _conn;
};

#endif /* _github_paulyc_database_hpp_ */
