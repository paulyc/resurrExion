//
//  database.cpp
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

#include "database.hpp"
#include <unordered_set>
#include <sstream>

// "root", "root", "resurrex", 0, "/run/mysqld/mysqld.sock"
Database::Database(DatabaseConfig &config) : _config(config) {
}

Database::~Database() {
}

sql::Connection* Database::getConnection() {
    sql::Driver* driver= sql::mariadb::get_driver_instance();
    sql::Connection *conn = driver->connect(_config.url, _config.props);
    return conn;
}

void Database::fill_allocated_clusters() {
    sql::Connection *conn = this->getConnection();
    sql::Statement *s = conn->createStatement();
    //sql::PreparedStatement *ps = _conn->prepareStatement("insert into cluster(cluster, allocated, file) values(?, ?, ?)");
    sql::ResultSet *rs = s->executeQuery(
                "select entry_offset, name, data_offset, data_len, is_contiguous from file where entry_offset >= 2562102657504 order by entry_offset asc"
                );
    while (rs->next()) {
        std::ostringstream q;
        q << "insert into cluster(cluster, allocated, file) values ";
        byteofs_t entry_offset = rs->getUInt64("entry_offset");
        sql::SQLString name = rs->getString("name");
        byteofs_t data_offset = rs->getUInt64("data_offset");
        ssize_t data_len = rs->getUInt64("data_len");
        int is_contiguous = rs->getInt("is_contiguous");
        clusterofs_t c = (data_offset - ClusterHeapStartOffset) / ClusterSize;
        do {
            /*ps->setUInt64(1, c);
            ps->setInt(2, 1);
            ps->setUInt64(3, entry_offset);
            ps->execute();*/
            data_len -= ClusterSize;
            ++c;
            q << '(' << c << ",1," << entry_offset << ')';
            //std::cout << "insert into cluster(cluster, allocated, file) values(" << c << ", 1, " << entry_offset << ")" << std::endl;
            if (is_contiguous && data_len > 0) {
                q << ',';
            } else {
                break;
            }
        } while (1);
        s->executeQuery(q.str());
        std::cout << q.str() << std::endl;
    }
    rs->close();
    //ps->close();
    s->close();
}

void Database::init_cluster_table() {
    sql::Connection *conn = this->getConnection();
    sql::Statement *s = conn->createStatement();
    s->executeQuery("alter table cluster disable keys");
    sql::PreparedStatement *ps = conn->prepareStatement("insert into cluster() values()");
    for (clusterofs_t c = 0; c < NumClusters; ++c) {
        //ps->setUInt64(0, c);
        ps->execute();
        if (c % 1000) {
            std::cout << c << std::endl;
        }
    }
    s->executeQuery("alter table cluster enable keys");
}

void Database::consolidate_unaccounted_clusters() {
}
