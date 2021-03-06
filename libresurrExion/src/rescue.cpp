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
#include "types.hpp"

#include <cstdio>

typedef uint8_t cluster[512*512];
cluster * cluster_heap = reinterpret_cast<cluster*>(ClusterHeapStartDiskOffset);
cluster * heap_end = cluster_heap + NumClusters;

inline void debug_memcpy(uint8_t *to, uint8_t *from, size_t bytes, bool dry_run=false) {
    if (!dry_run) {
        for (;bytes > 0;--bytes) {
	    // prevent CPU, or at least mine, from throwing around interrupts that are going unhandled
	    // because someone's got a bug causing memcpy and memmove to execute SIMD instructions with
	    // wildly misaligned addresses.
	    // Seems like it only happens when the mmaped device is read-only though, i.e., this
	    // SHOULD fail, but not, I think, with an equally wildly misleading HW interrupt.
	    // So, this has fulfilled its purpose of making sure only to do cache-aligned memcpys,
	    // but still not a bad sanity check to have around particularly with the utter lack of unit testing going on here.
	    // No, this is not exactly the most important data I'm trying to recover here :D
	    assert((reinterpret_cast<std::uintptr_t>(to) % 512 == 0));
	    assert((reinterpret_cast<std::uintptr_t>(from) % 512 == 0));
	    if ((reinterpret_cast<std::uintptr_t>(to) % 64) != 0 || (reinterpret_cast<std::uintptr_t>(from) % 64) != 0) {
                *to++ = *from++;
            } else {
                break;
            }
        }
        memcpy(to, from, bytes);
    }
}

void github::paulyc::SalvatorDatorum::consolidate_fragments(FilesystemStub &fs, Database &db) {
    cluster * cluster_heap = reinterpret_cast<cluster*>(fs._mmap + ClusterHeapStartDiskOffset);
    cluster * heap_end = cluster_heap + NumClusters;
    std::cout << "ClusterHeapStartDiskOffset = " << ClusterHeapStartDiskOffset << " % 512 == " << ClusterHeapStartDiskOffset % 512 << " % 512*512 == " << ClusterHeapStartDiskOffset % (512*512) << std::endl;
    std::cout <<
        "cluster_heap = " << reinterpret_cast<std::uintptr_t>(cluster_heap) <<
        " % 512 == " << reinterpret_cast<std::uintptr_t>(cluster_heap) % 512 <<
	" % 512*512 == " << reinterpret_cast<std::uintptr_t>(cluster_heap) % (512*512) << 
        std::endl;

    sql::Connection *conn = db.getConnection();
    //sql::Connection *updateConn = db.getConnection();
    sql::Statement *s = conn->createStatement();
    sql::ResultSet *rs = s->executeQuery("SELECT r.id, r.from_cluster, r.to_cluster FROM relocate r WHERE r.status = 0 ORDER BY r.from_cluster ASC");
    //sql::PreparedStatement *ps = updateConn->prepareStatement("UPDATE relocate SET status = 1 WHERE id = ?");
    while (rs->next()) {
        uint64_t id = rs->getUInt64("id");
        clusterofs_t from = rs->getUInt64("from_cluster");
        clusterofs_t to   = rs->getUInt64("to_cluster");
        printf("0x%08x -> 0x%08x memmove(%p, %p)\n", from, to, &cluster_heap[to], &cluster_heap[from]);
//        continue;
        //std::cin >> dummy;
//	memcpy(&cluster_heap[to], &cluster_heap[from], sizeof(cluster));
	debug_memcpy((uint8_t*)&cluster_heap[to], (uint8_t*)&cluster_heap[from], sizeof(cluster));
        // was debugging, stupid bug mentioned in this function!
	//memmove_cluster((uint8_t*)(heap+to), (uint8_t*)(heap+from), (unsigned int)ClusterSize);
        //ps->setUInt64(1, id);
        //ps->executeUpdate();
    }
    //ps->close();
    rs->close();
    s->close();
    conn->close();
}

void github::paulyc::SalvatorDatorum::rescue_music(FilesystemStub &fs, Database &db) {
    sql::Connection *conn = db.getConnection();
    //dir = "/home/paulyc/elements";
    sql::PreparedStatement *stmt = conn->prepareStatement("update file set is_copied_off = 1 where file.entity_offset = ?");
    std::function<void(File*)> onFileCopied = [&stmt](File *f){
        stmt->setUInt64(1, f->_offset);
        stmt->executeUpdate();
    };
    //stub.parseTextLog("recovery.log");
    sql::Statement *s = conn->createStatement();
    sql::ResultSet *rs = s->executeQuery("select distinct(parent_directory_offset) from file where name like '%flac' or name like '%mp3' or name like '%wav' or name like '%m4a' or name like '%aiff' or name like '%aif'");
    while (rs->next()) {
        //mariadb::transaction_ref txref = conn->create_transaction();
        uint64_t pdo = rs->getUInt64(1);
        Directory * d = reinterpret_cast<Directory*>(fs.loadEntityOffset(pdo, "temp"));
        if (d == nullptr) {
            continue;
        }
        std::string name = std::string(d->_name.c_str());
        fs.dump_directory(d, name, onFileCopied, false);
        //txref->commit();
    }

    rs->close();
    stmt->close();
    conn->close();
}

void github::paulyc::SalvatorDatorum::rescue_orphan_dirs(FilesystemStub &fs, Database &db) {
    sql::Connection *conn = db.getConnection();
    sql::PreparedStatement *stmt = conn->prepareStatement("update file set is_copied_off = 1 where file.entry_offset = ?");
    sql::PreparedStatement *frag_stmt = conn->prepareStatement("update file set is_contiguous = ? where file.entry_offset = ?");
    sql::PreparedStatement *dir_stmt = conn->prepareStatement("select entry_offset, parent_directory_offset from directory where entry_offset = ?");
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

    sql::Statement *s = conn->createStatement();
    sql::ResultSet *orphan_dirs = s->executeQuery(
                "select entry_offset from directory where parent_directory_offset = 0 order by entry_offset asc"
                );

    while (orphan_dirs->next()) {
        byteofs_t pdo = orphan_dirs->getUInt64("entry_offset");
        Directory * d = reinterpret_cast<Directory*>(fs.loadEntityOffset(pdo, "temp"));
        if (d == nullptr) {
            std::cerr << "failed to load directory pdo = " << pdo << std::endl;
        }
        fs.dump_directory(d, std::string(d->_name.c_str()), onFileCopied, false);
        delete d;
    }
    orphan_dirs->close();
    s->close();
    dir_stmt->close();
    frag_stmt->close();
    stmt->close();
    conn->close();
}

static void copy_files(std::list<File*> &files, uint8_t *mmap, const char *dir) {
    for (File *f: files) {
        f->copy_to_dir(mmap, dir);
        std::cout << "wrote " << f->_name << " offset " << f->_offset << std::endl;
    }
}

void github::paulyc::SalvatorDatorum::rescue_dupe_orphan_files(FilesystemStub &fs, Database &db, const char *dir) {
    sql::Connection *conn = db.getConnection();
    sql::Statement *s = conn->createStatement();
    sql::ResultSet *orphan_files = s->executeQuery(
                "select count(*) c, name from file where parent_directory_offset = 0 group by name order by c asc");
    sql::PreparedStatement *dupes_stmt = conn->prepareStatement("select entry_offset from file where name = ?");
    while (orphan_files->next()) {
        uint64_t count = orphan_files->getUInt64("c");
        if (count > 1) {
            sql::SQLString name = orphan_files->getString("name");
            dupes_stmt->setString(1, name);
            sql::ResultSet *dupes = dupes_stmt->executeQuery();
            std::list<File*> files;
            while (dupes->next()) {
                byteofs_t pdo = dupes->getUInt64("entry_offset");
                File * f = reinterpret_cast<File*>(fs.loadEntityOffset(pdo, "temp"));
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
                    copy_files(files, fs._mmap, dir);
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
    conn->close();
}

void github::paulyc::SalvatorDatorum::rescue_orphan_files(FilesystemStub &fs, Database &db, const char *dir) {
    sql::Connection *conn = db.getConnection();
    sql::Statement *s = conn->createStatement();
    sql::PreparedStatement *stmt = conn->prepareStatement(
                "update file set is_copied_off = 1 where file.entry_offset = ?");
    sql::PreparedStatement *dir_stmt = conn->prepareStatement(
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
        File * f = reinterpret_cast<File*>(fs.loadEntityOffset(pdo, "temp"));
        if (f == nullptr) {
            std::cerr << "failed to load file pdo = " << pdo << std::endl;
        }
        f->copy_to_dir(fs._mmap, dir);
        onFileCopied(f);
        std::cout << "wrote " << f->_offset << std::endl;
       // stub.dump_directory(d, std::string(d->_name.c_str()), onFileCopied, false);
        delete f;
    }
    orphan_files->close();
    dir_stmt->close();
    stmt->close();
    s->close();
    conn->close();
}
