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


// "root", "root", "resurrex", 0, "/run/mysqld/mysqld.sock"
Database::Database(const std::string &user, const std::string &pass, const std::string &sock, const std::string &db) {
    mariadb::account_ref a = mariadb::account::create("", user.c_str(), pass.c_str(), db.c_str(), 0, sock.c_str());
    a->set_auto_commit(false);
    _conn = mariadb::connection::create(a);
    if (!_conn->connect()) {
        _conn = nullptr;
        throw new std::runtime_error("failed to connect");
    }
}
Database::~Database() {
    if (_conn != nullptr) {
        _conn->disconnect();
    }
}

void Database::rescue_directories(const std::string &rescuedir) {
    mariadb::result_set_ref rs = _conn->query("SELECT * FROM directory");
    while (rs->next()) {
        std::string name = rs->get_string("name");
    }
}

void Database::rescue_music(const std::string &dev, const std::string &dir) {
    //dir = "/home/paulyc/elements";
    FilesystemStub stub;
    stub.open("/dev/sdb");
    //stub.parseTextLog("recovery.log");
    mariadb::result_set_ref rs = _conn->query("select distinct(parent_directory_offset) from file where name like '%flac' or name like '%mp3' or name like '%wav' or name like '%m4a' or name like '%aiff' or name like '%aif'");
    while (rs->next()) {
        mariadb::u64 pdo = rs->get_unsigned64(0);
        Directory * d = reinterpret_cast<Directory*>(stub.loadEntityOffset(pdo, "temp"));
        if (d == nullptr) {
            continue;
        }
        //d->resolve_children(_conn);
        std::string name = std::string(d->_name.c_str());
        stub.dump_directory(d, name);
    }
    //rs = _conn->query("SELECT id, parent_directory_offset FROM file");
}
/*
void Database::migrate_to_sql_ids() {
    mariadb::result_set_ref rs = _conn->query("SELECT id, parent_directory_offset FROM file");
    while (rs->next()) {

    }
}
void Database::gather_music() {
    _conn->query("SELECT * FROM file WHERE name like '%flac' and parent_directory_offset = 0");
}
void Database::gather_photos() {

}
void Database::gather_x() {

}
*/
#if 0
void Database::connect(const std::string &user, const std::string &pass, const std::string &sock, const std::string &db) {
    try {
        _conn = std::make_unique<mysqlpp::Connection>(db.c_str(), sock.c_str(), user.c_str(), pass.c_str());
    } catch (const mysqlpp::BadQuery &ex) {
        std::cerr << "Bad query mysql++ exception: " << ex.what() << std::endl;
        throw ex;
    } catch (const mysqlpp::Exception &ex) {
        std::cerr << "General mysql++ exception: " << ex.what() << std::endl;
        throw ex;
    }
}

class fix_missing_parents
{
public:
    fix_missing_parents() :
};

void Database::fix_parents() {
    mysqlpp::Query q = _conn->query();
}
#endif
