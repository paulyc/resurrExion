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

// "root", "root", "resurrex", 0, "/run/mysqld/mysqld.sock"
Database::Database(const std::string &dev, const std::string &user, const std::string &pass, const std::string &sock, const std::string &db) {
    sql::Driver* driver= sql::mariadb::get_driver_instance();
    sql::SQLString url("jdbc:mariadb://localhost:3306/" + db);
    sql::Properties props({{"user", user}, {"password", pass}});
    _conn = driver->connect(url, props);
    _conn->setAutoCommit(true);
    //mariadb::account_ref a = mariadb::account::create("", user.c_str(), pass.c_str(), db.c_str(), 0, sock.c_str());
    //a->set_auto_commit(true);
    //_conn = mariadb::connection::create(a);
    _stub.open(dev);
}
Database::~Database() {
    _conn->close();
    _conn = nullptr;
    _stub.close();
}

void Database::rescue_music() {
    //dir = "/home/paulyc/elements";
    sql::PreparedStatement *stmt = _conn->prepareStatement("update file set is_copied_off = 1 where file.entity_offset = ?");
    std::function<void(File*)> onFileCopied = [&stmt](File *f){
        stmt->setUInt64(0, f->_offset);
        stmt->executeUpdate();
    };
    //stub.parseTextLog("recovery.log");
    sql::Statement *s = _conn->createStatement();
    sql::ResultSet *rs = s->executeQuery("select distinct(parent_directory_offset) from file where name like '%flac' or name like '%mp3' or name like '%wav' or name like '%m4a' or name like '%aiff' or name like '%aif'");
    while (rs->next()) {
        //mariadb::transaction_ref txref = _conn->create_transaction();
        mariadb::u64 pdo = rs->getUInt64(0);
        Directory * d = reinterpret_cast<Directory*>(_stub.loadEntityOffset(pdo, "temp"));
        if (d == nullptr) {
            continue;
        }
        std::string name = std::string(d->_name.c_str());
        _stub.dump_directory(d, name, onFileCopied, false);
        //txref->commit();
    }

    rs->close();
    stmt->close();
}

void Database::rescue_orphan_dirs() {
    sql::PreparedStatement *stmt = _conn->prepareStatement("update file set is_copied_off = 1 where file.entry_offset = ?");
    sql::PreparedStatement *frag_stmt = _conn->prepareStatement("update file set is_contiguous = ? where file.entry_offset = ?");
    sql::PreparedStatement *dir_stmt = _conn->prepareStatement("select entry_offset, parent_directory_offset from directory where entry_offset = ?");
    uint64_t count = 0;
    std::function<void(File*)> onFileCopied = [&stmt, &frag_stmt, &count](File *f){
        stmt->setUInt64(0, f->_offset);
        stmt->executeUpdate();
        //frag_stmt->set_unsigned8(0, f->_contiguous);
        //frag_stmt->set_unsigned64(1, f->_offset);
        //frag_stmt->execute();
        if (++count % 1000 == 0) {
            std::cout << "counted " << count << std::endl;
        }
    };
    //stub.parseTextLog("recovery.log");
    //mariadb::result_set_ref rs = _conn->query(
    //"select distinct(parent_directory_offset) from file where name like '%jpg' or name like '%jpeg' or name like '%png' or name like '%mov' or name like '%mp4' or name like '%avi' or name like '%wmv' or name like '%mkv'");

    sql::Statement *s = _conn->createStatement();
    sql::ResultSet *orphan_dirs = s->executeQuery(
                "select entry_offset from directory where parent_directory_offset = 0 order by entry_offset asc"
                );

    while (orphan_dirs->next()) {
        mariadb::u64 pdo = orphan_dirs->getUInt64(0);
        Directory * d = reinterpret_cast<Directory*>(_stub.loadEntityOffset(pdo, "temp"));
        if (d == nullptr) {
            std::cerr << "failed to load directory pdo = " << pdo << std::endl;
        }
        _stub.dump_directory(d, std::string(d->_name.c_str()), onFileCopied, false);
        delete d;
    }
    orphan_dirs->close();
    s->close();
    dir_stmt->close();
    frag_stmt->close();
    stmt->close();
}

static void copy_files(std::list<File*> &files, uint8_t *mmap, const char *dir) {
    for (File *f: files) {
        f->copy_to_dir(mmap, dir);
        std::cout << "wrote " << f->_name << " offset " << f->_offset << std::endl;
    }
}

void Database::rescue_dupe_orphan_files(const char *dir) {
    sql::Statement *s = _conn->createStatement();
    sql::ResultSet *orphan_files = s->executeQuery(
                "select count(*) c, name from file where parent_directory_offset = 0 group by name order by c asc");
    sql::PreparedStatement *dupes_stmt = _conn->prepareStatement("select entry_offset from file where name = ?");
    while (orphan_files->next()) {
        mariadb::u64 count = orphan_files->getUInt64(0);
        if (count > 1) {
            sql::SQLString name = orphan_files->getString(1);
            dupes_stmt->setString(0, name);
            sql::ResultSet *dupes = dupes_stmt->executeQuery();
            std::list<File*> files;
            while (dupes->next()) {
                mariadb::u64 pdo = dupes->getUInt64(0);
                File * f = reinterpret_cast<File*>(_stub.loadEntityOffset(pdo, "temp"));
                if (f == nullptr) {
                    std::cerr << "failed to load file pdo = " << pdo << std::endl;
                }
                //f->_name += std::to_string(count++);
                f->_name = std::to_string(pdo) + std::string("-") + f->_name;
                files.push_back(f);
            }
            std::list<File*>::iterator it = files.begin();
            //uint32_t crc = (*it)->data_crc32(stub._mmap);
            uint32_t crc = (*it)->_data_length;
            for (;it != files.end(); ++it) {
                //uint32_t c = (*it)->data_crc32(stub._mmap);
                uint32_t c = (*it)->_data_length;
                if (c != crc) {
                    std::cout << "no match len " << crc << " and " << c << std::endl;
                    copy_files(files, _stub._mmap, dir);
                    break;
                }
            }
            for (File *f: files) {
                delete f;
            }
        }
    }
    orphan_files->close();
    dupes_stmt->close();
    s->close();
}

void Database::rescue_orphan_files(const char *dir) {
    sql::Statement *s = _conn->createStatement();
    sql::PreparedStatement *stmt = _conn->prepareStatement(
                "update file set is_copied_off = 1 where file.entry_offset = ?");
    sql::PreparedStatement *dir_stmt = _conn->prepareStatement(
                "select entry_offset, parent_directory_offset from directory where entry_offset = ?");
    uint64_t count = 0;
    std::function<void(File*)> onFileCopied = [&stmt, &count](File *f){
        stmt->setUInt64(0, f->_offset);
        stmt->executeUpdate();
        if (++count % 1000 == 0) {
            std::cout << "counted " << count << std::endl;
        }
    };
    sql::ResultSet *orphan_files = s->executeQuery(
                "select entry_offset from file where is_contiguous = 1 and is_copied_off = 0 and entry_offset > 237329123584 order by entry_offset asc ");

    while (orphan_files->next()) {
        mariadb::u64 pdo = orphan_files->getUInt64(0);
        File * f = reinterpret_cast<File*>(_stub.loadEntityOffset(pdo, "temp"));
        if (f == nullptr) {
            std::cerr << "failed to load file pdo = " << pdo << std::endl;
        }
        f->copy_to_dir(_stub._mmap, dir);
        onFileCopied(f);
        std::cout << "wrote " << f->_offset << std::endl;
       // stub.dump_directory(d, std::string(d->_name.c_str()), onFileCopied, false);
        delete f;
    }
    orphan_files->close();
    dir_stmt->close();
    stmt->close();
    s->close();
}

static constexpr clusterofs_t NumClusters = 15260537;

void Database::fill_allocated_clusters() {
    sql::Statement *s = _conn->createStatement();
    sql::PreparedStatement *ps = _conn->prepareStatement("insert into cluster(cluster, allocated, file) values(?, ?, ?)");
    sql::ResultSet *rs = s->executeQuery(
                "select entry_offset, name, data_offset, data_len, is_contiguous from file order by entry_offset asc"
                );
    while (rs->next()) {
        byteofs_t entry_offset = rs->getUInt64("entry_offset");
        sql::SQLString name = rs->getString("name");
        byteofs_t data_offset = rs->getUInt64("data_offset");
        ssize_t data_len = rs->getUInt64("data_len");
        int is_contiguous = rs->getInt("is_contiguous");
        clusterofs_t c = (data_offset - ClusterHeapStartOffset) / ClusterSize;
        do {
            ps->setUInt64(1, c);
            ps->setInt(2, 1);
            ps->setUInt64(3, entry_offset);
            ps->execute();
            data_len -= ClusterSize;
            ++c;
            std::cout << "insert into cluster(cluster, allocated, file) values(" << c << ", 1, " << entry_offset << ")" << std::endl;
        } while (is_contiguous && data_len > 0);
    }
    rs->close();
    ps->close();
    s->close();
}

void Database::init_cluster_table() {
    sql::Statement *s = _conn->createStatement();
    s->executeQuery("alter table cluster disable keys");
    sql::PreparedStatement *ps = _conn->prepareStatement("insert into cluster() values()");
    for (clusterofs_t c = 0; c < NumClusters; ++c) {
        //ps->setUInt64(0, c);
        ps->execute();
        if (c % 1000) {
            std::cout << c << std::endl;
        }
    }
    s->executeQuery("alter table cluster enable keys");
}

/*
void Database::migrate_to_sql_ids() {
    mariadb::result_set_ref rs = _conn->query("SELECT id, parent_directory_offset FROM file");
    while (rs->next()) {

    }
}
*/
