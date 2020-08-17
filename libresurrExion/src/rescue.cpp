//
//  rescue.cpp
//  resurrExion
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

#include "rescue.hpp"

void github::paulyc::SalvatorDatorum::rescue_music(Database &db) {
    //dir = "/home/paulyc/elements";
    sql::PreparedStatement *stmt = db._conn->prepareStatement("update file set is_copied_off = 1 where file.entity_offset = ?");
    std::function<void(File*)> onFileCopied = [&stmt](File *f){
        stmt->setUInt64(1, f->_offset);
        stmt->executeUpdate();
    };
    //stub.parseTextLog("recovery.log");
    sql::Statement *s = db._conn->createStatement();
    sql::ResultSet *rs = s->executeQuery("select distinct(parent_directory_offset) from file where name like '%flac' or name like '%mp3' or name like '%wav' or name like '%m4a' or name like '%aiff' or name like '%aif'");
    while (rs->next()) {
        //mariadb::transaction_ref txref = db._conn->create_transaction();
        uint64_t pdo = rs->getUInt64(1);
        Directory * d = reinterpret_cast<Directory*>(db._stub.loadEntityOffset(pdo, "temp"));
        if (d == nullptr) {
            continue;
        }
        std::string name = std::string(d->_name.c_str());
        db._stub.dump_directory(d, name, onFileCopied, false);
        //txref->commit();
    }

    rs->close();
    stmt->close();
}

void github::paulyc::SalvatorDatorum::rescue_orphan_dirs(Database &db) {
    sql::PreparedStatement *stmt = db._conn->prepareStatement("update file set is_copied_off = 1 where file.entry_offset = ?");
    sql::PreparedStatement *frag_stmt = db._conn->prepareStatement("update file set is_contiguous = ? where file.entry_offset = ?");
    sql::PreparedStatement *dir_stmt = db._conn->prepareStatement("select entry_offset, parent_directory_offset from directory where entry_offset = ?");
    uint64_t count = 0;
    std::function<void(File*)> onFileCopied = [&stmt, &frag_stmt, &count](File *f){
        stmt->setUInt64(1, f->_offset);
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

    sql::Statement *s = db._conn->createStatement();
    sql::ResultSet *orphan_dirs = s->executeQuery(
                "select entry_offset from directory where parent_directory_offset = 0 order by entry_offset asc"
                );

    while (orphan_dirs->next()) {
        byteofs_t pdo = orphan_dirs->getUInt64("entry_offset");
        Directory * d = reinterpret_cast<Directory*>(db._stub.loadEntityOffset(pdo, "temp"));
        if (d == nullptr) {
            std::cerr << "failed to load directory pdo = " << pdo << std::endl;
        }
        db._stub.dump_directory(d, std::string(d->_name.c_str()), onFileCopied, false);
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

void github::paulyc::SalvatorDatorum::rescue_dupe_orphan_files(Database &db, const char *dir) {
    sql::Statement *s = db._conn->createStatement();
    sql::ResultSet *orphan_files = s->executeQuery(
                "select count(*) c, name from file where parent_directory_offset = 0 group by name order by c asc");
    sql::PreparedStatement *dupes_stmt = db._conn->prepareStatement("select entry_offset from file where name = ?");
    while (orphan_files->next()) {
        uint64_t count = orphan_files->getUInt64("c");
        if (count > 1) {
            sql::SQLString name = orphan_files->getString("name");
            dupes_stmt->setString(1, name);
            sql::ResultSet *dupes = dupes_stmt->executeQuery();
            std::list<File*> files;
            while (dupes->next()) {
                byteofs_t pdo = dupes->getUInt64("entry_offset");
                File * f = reinterpret_cast<File*>(db._stub.loadEntityOffset(pdo, "temp"));
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
                    copy_files(files, db._stub._mmap, dir);
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

void github::paulyc::SalvatorDatorum::rescue_orphan_files(Database &db, const char *dir) {
    sql::Statement *s = db._conn->createStatement();
    sql::PreparedStatement *stmt = db._conn->prepareStatement(
                "update file set is_copied_off = 1 where file.entry_offset = ?");
    sql::PreparedStatement *dir_stmt = db._conn->prepareStatement(
                "select entry_offset, parent_directory_offset from directory where entry_offset = ?");
    uint64_t count = 0;
    std::function<void(File*)> onFileCopied = [&stmt, &count](File *f){
        stmt->setUInt64(1, f->_offset);
        stmt->executeUpdate();
        if (++count % 1000 == 0) {
            std::cout << "counted " << count << std::endl;
        }
    };
    sql::ResultSet *orphan_files = s->executeQuery(
                "select entry_offset from file where is_contiguous = 1 and is_copied_off = 0 and entry_offset > 237329123584 order by entry_offset asc ");

    while (orphan_files->next()) {
        byteofs_t pdo = orphan_files->getUInt64("entry_offset");
        File * f = reinterpret_cast<File*>(db._stub.loadEntityOffset(pdo, "temp"));
        if (f == nullptr) {
            std::cerr << "failed to load file pdo = " << pdo << std::endl;
        }
        f->copy_to_dir(db._stub._mmap, dir);
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
