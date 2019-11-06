//
//  quick.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 11/5/19.
//
//  Copyright (C) 2019 Paul Ciarlo <paul.ciarlo@gmail.com>.
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

#include <stdlib.h>
#include <xlocale.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <unistd.h>

#ifndef O_RSYNC
#define O_RSYNC O_SYNC
#endif

#include <unordered_map>
#include <typeinfo>
#include <exception>
#include <system_error>
#include <locale>
#include <codecvt>
#include <vector>
#include <fstream>
#include <iostream>
#include <functional>
#include <regex>
#include <unordered_set>
#include <optional>

#include "exfat_structs.hpp"

class Entity{
public:
    virtual ~Entity() {}
    virtual std::string get_name() const { return "Entity"; }
    virtual Entity * get_parent() const { return nullptr; }
    virtual void set_parent(Entity *parent) {}
    virtual void add_child(Entity *child) {}
    virtual std::optional<std::unordered_set<Entity*>> get_children() const { return std::nullopt; }
};
class File:public Entity{
public:
    std::string name;
    Entity *parent;

    virtual ~File() {}
    virtual std::string get_name() const { return name; }
    virtual Entity * get_parent() const { return parent; }
    virtual void set_parent(Entity *parent) {this->parent = parent;}
    virtual void add_child(Entity *child) {}
    virtual std::optional<std::unordered_set<Entity*>> get_children() const { return std::nullopt; }
};
class Directory:public Entity{
public:
    std::string name;
    Entity *parent;
    std::unordered_set<Entity*> children;

    virtual ~Directory() {}
    virtual std::string get_name() const { return name; }
    virtual Entity * get_parent() const { return parent; }
    virtual void set_parent(Entity *parent) {this->parent = parent;}
    virtual void add_child(Entity *child) {child->set_parent(this); this->children.insert(child);}
    virtual std::optional<std::unordered_set<Entity*>> get_children() const { return std::make_optional(children); }
};

using namespace github::paulyc::resurrExion;

constexpr static size_t SectorSize = 512;
constexpr static size_t SectorsPerCluster = 512;
constexpr static size_t NumSectors = 7813560247;
constexpr static size_t ClustersInFat = (NumSectors - 0x283D8) / 512;
constexpr static size_t PartitionStartSector = 0x64028;
constexpr static size_t ClusterHeapStartSector = 0x283D8; // relative to partition start
constexpr static size_t DiskSize = (NumSectors + PartitionStartSector) * SectorSize;

class FilesystemStub
{
public:
    FilesystemStub():
        _fd(-1),
        _mmap((uint8_t*)MAP_FAILED)
    {
        const bool write_changes = false;
        const int oflags = write_changes ? O_RDWR | O_DSYNC | O_RSYNC : O_RDONLY;
        _fd = open("/dev/disk2", oflags);
        if (_fd == -1) {
            throw std::system_error(std::error_code(errno, std::system_category()));
        }

        const int mprot = write_changes ? PROT_READ | PROT_WRITE : PROT_READ;
        const int mflags = write_changes ? MAP_SHARED : MAP_PRIVATE;
        _mmap = (uint8_t*)mmap(0, DiskSize, mprot, mflags, _fd, 0);
        if (_mmap == (uint8_t*)MAP_FAILED) {
            //throw std::system_error(std::error_code(errno, std::system_category()));
            std::cerr << "error opening mmap" << std::endl;
            return;
        }

        _partition_start = _mmap + PartitionStartSector * SectorSize;
        _partition_end = _partition_start + (NumSectors + 1) * SectorSize;
       // _fs = (exfat::filesystem_t<SectorSize, SectorsPerCluster, NumSectors>*)_partition_start;

        _invalid_file_name_characters = {'"', '*', '/', ':', '<', '>', '?', '\\', '|'};
        for (char16_t ch = 0; ch <= 0x001F; ++ch) {
            _invalid_file_name_characters.push_back(ch);
        }
    }

    ~FilesystemStub()
    {
        if (_mmap != MAP_FAILED) {
            munmap(_mmap, DiskSize);
        }

        if (_fd != -1) {
            close(_fd);
        }
    }
    void cb(std::streamoff offset, Entity *ent, std::optional<std::exception> except) {
        
    }

    void parseTextLog(const std::string &filename)
    {
        std::regex fde("FDE ([0-9a-fA-F]{16})(?: (.*))?");
        std::regex badsector("BAD_SECTOR ([0-9a-fA-F]{16})");
        std::ifstream logfile(filename);
        size_t count = 0;
        for (std::string line; std::getline(logfile, line); ++count) {
            std::smatch sm;
            if (count % 1000 == 0) {
                printf("count: %zu\n", count);
            }
            if (std::regex_match(line, sm, fde)) {
                std::streamoff offset;
                std::string filename;
                filename = sm[2];
                try {
                    offset = std::stol(sm[1], nullptr, 16);
                    Entity * ent = loadEntityOffset(offset, nullptr);
                    cb(offset, ent, std::nullopt);
                } catch (std::exception &ex) {
                    std::cerr << "Writing file entry to binlog, got exception: " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                    std::cerr << "Invalid textlog FDE line, skipping: " << line << std::endl;
                    cb(0, nullptr, std::make_optional(ex));
                }
            } else if (std::regex_match(line, sm, badsector)) {
                std::streamoff offset;
                try {
                    offset = std::stol(sm[1], nullptr, 16);
                    cb(offset, nullptr, std::nullopt);
                } catch (std::exception &ex) {
                    std::cerr << "Writing bad sector to binlog, got exception " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                    std::cerr << "Invalid textlog BAD_SECTOR line, skipping: " << line << std::endl;
                    cb(0, nullptr, std::make_optional(ex));
                }
            } else {
                std::cerr << "Unknown textlog line format: " << line << std::endl;
                cb(0, nullptr, std::make_optional(std::exception()));
            }
        }
    }

    void find_orphans() {
        FILE *orphanlog = fopen("/Users/paulyc/Development/resurrExion/orphan.log", "w");
        for (auto & [ entry_ptr, entity ] : _offset_to_entity_mapping) {
            if (entity->get_parent() == nullptr) {
                fprintf(orphanlog, "ORPHAN %016lld %s", entry_ptr, entity->get_name().c_str());
            }
        }
        fclose(orphanlog);
    }

    Entity * loadEntityOffset(std::streamoff entity_offset, Entity * parent) {
        return loadEntity(_mmap + entity_offset, parent);
    }

    Entity * loadEntity(uint8_t *entity_start, Entity * parent)
    {
        exfat::file_directory_entry_t *fde = (exfat::file_directory_entry_t*)(entity_start);
        exfat::stream_extension_entry_t *streamext = (exfat::stream_extension_entry_t*)(entity_start+32);
        exfat::file_name_entry_t *n = (exfat::file_name_entry_t*)(entity_start+64);

        if (fde->type != exfat::FILE_DIR_ENTRY || streamext->type != exfat::STREAM_EXTENSION || n->type != exfat::FILE_NAME) {
            fprintf(stderr, "bad number of continuations at offset %016lx\n", entity_start - _mmap);
            return nullptr;
        }

        const int continuations = fde->continuations;
        if (continuations < 2 || continuations > 18) {
            fprintf(stderr, "bad number of continuations at offset %016lx\n", entity_start - _mmap);
            return nullptr;
        }

        int i;
        uint16_t chksum = 0;

        for (i = 0; i < 32; ++i) {
            if (i != 2 && i != 3) {
                chksum = ((chksum << 15) | (chksum >> 1)) + entity_start[i];
            }
        }

        for (; i < (continuations+1) * 32; ++i) {
            chksum = ((chksum << 15) | (chksum >> 1)) + entity_start[i];
        }

        if (chksum != fde->checksum) {
            fprintf(stderr, "bad file entry checksum at offset %016llx\n", entity_start - _mmap);
            return nullptr;
        }

        std::basic_string<char16_t> u16s;
        std::string utf8_name;
        int namelen = streamext->name_length;
        for (int c = 0; c <= continuations - 2; ++c) {
            if (n[c].type == exfat::FILE_NAME) {
                for (int i = 0; i < sizeof(n[c].name); ++i) {
                    if (u16s.length() == namelen) {
                        break;
                    } else {
                        u16s.push_back(n[c].name[i]);
                    }
                }
            }
        }
        if (u16s.length() != namelen) {
            fprintf(stderr, "u16s.length() != namelen for entity at offset %016llx\n", entity_start - _mmap);
        }
        utf8_name = _cvt.to_bytes(u16s);

        // load parents and children here.....
        // orphan entities we will just stick in the root eventually ....
        //std::shared_ptr<BaseEntity> entity;
        // first see if we already loaded this entity
        std::unordered_map<std::streamoff, Entity*>::iterator entityit =
            _offset_to_entity_mapping.find(entity_start - _mmap);
        if (entityit != _offset_to_entity_mapping.end()) {
            if (entityit->second->get_parent() == nullptr) {
                if (parent != nullptr) {
                    entityit->second->set_parent(parent);// = parent;
                }
            } else {
                if (parent != nullptr) {
                    fprintf(stderr, "entity at offset %016llx already has a parent\n", entity_start - _mmap);
                }
            }
            return entityit->second;
        }

        if (fde->attributes & exfat::DIRECTORY) {
            exfat::file_directory_entry_t *fde = (exfat::file_directory_entry_t *)entity_start;
            Directory *d = new Directory;
            d->name = utf8_name;
            d->parent = parent;
            loadDirectory(d, fde, continuations + 1);

            //std::make_shared<DirectoryEntity>(entity_start, continuations + 1, parent, utf8_name);
            _offset_to_entity_mapping[entity_start - _mmap] = d;
            return d;
        } else {
            File *f = new File;
            f->name = utf8_name;
            f->parent = parent;
            _offset_to_entity_mapping[entity_start - _mmap] = f;
            return f;
        }
    }

    std::unordered_map<std::streamoff, Entity*> _offset_to_entity_mapping;

    void loadDirectory(Directory *d, exfat::file_directory_entry_t *fde, int num_entries)
    {
        const uint32_t start_cluster = ((exfat::stream_extension_entry_t*)(fde + 1))->first_cluster;
        exfat::metadata_entry_u *dirent;
        if (start_cluster == 0) {
            // directory children immediately follow
            dirent = (exfat::metadata_entry_u *)(fde + num_entries);
        } else {
            fprintf(stderr, "PANIC!\n");
            throw std::exception();
            //dirent = (exfat::metadata_entry_u *)_fs->data_region.cluster_heap.storage[start_cluster].sectors[0].data;
        }

        const uint8_t num_secondary_entries = ((exfat::primary_directory_entry_t*)dirent)->secondary_count;
        for (uint8_t secondary_entry = 0; secondary_entry < num_secondary_entries; ++secondary_entry) {
            Entity *child = this->loadEntity((uint8_t*)dirent, d);
            d->add_child(child);
            child->set_parent(d);
#warning not done
            dirent += 0;//child->get_num_entries();
        }
    }

    std::string _get_utf8_filename(exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde)
    {
        const int continuations = fde->continuations;
        const int namelen = sde->name_length;
        std::basic_string<char16_t> u16s;
        for (int c = 2; c <= continuations; ++c) {
            exfat::file_name_entry_t *n = (exfat::file_name_entry_t *)(((uint8_t*)fde) + c*32);
            if (n->type == exfat::FILE_NAME) {
                for (int i = 0; i < sizeof(n->name); ++i) {
                    if (u16s.length() == namelen) {
                        return _cvt.to_bytes(u16s);
                    } else {
                        u16s.push_back((char16_t)n->name[i]);
                    }
                }
            }
        }
        return _cvt.to_bytes(u16s);
    }

    int      _fd;
    uint8_t *_mmap;
    uint8_t *_partition_start;
    uint8_t *_partition_end;
    std::vector<char16_t> _invalid_file_name_characters;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> _cvt;
};

int main(int argc, char *argv[]) {
    std::unordered_map<Entity*, Entity*> child_to_parent;
    std::unordered_map<Entity*, std::streamoff> entity_to_offset;

    //if (argc != 3) {
    //    fprintf(stderr, "wrong args\n");
    //    return 1;
    //}

    //filesystem_t fs("/dev/disk2", DiskSize, PartitionStartSector, false);
    //fs.init_metadata();

    FilesystemStub log;
    log.parseTextLog("/Users/paulyc/Development/resurrExion/recovery.log");
#if 0
    , [](std::streamoff offset, entity_t ent, std::optional<std::exception> except){
        // go through the log, add every file, update directory children,
        // anything left with no parent goes in lost+found
        if (except.has_value()) {
            const std::exception &ex = except.value();
            fprintf(stderr, "[WARN] entry_info had exception type %s with message %s\n", typeid(ex).name(), ex.what());
        } else if (ent != nullptr) {
            switch (ent->get_type()) {
            case BaseEntity::File:
            {
                std::shared_ptr<FileEntity> f = std::dynamic_pointer_cast<FileEntity>(ent);
                if (!f) {
                    fprintf(stderr, "[WARN] Invalid file entry at offset %016llx\n", offset);
                    return;
                }
                /*if (entity_to_offset.find(ent) != entity_to_offset.end()) {
                    fprintf(stderr, "[WARN] %016lx [%s] already in offset_to_entity\n", offset, ent->get_name().c_str());
                    return;
                }
                entity_to_offset[ent] = offset;
                if (child_to_parent.find(ent) == child_to_parent.end()) {
                    child_to_parent[ent] = nullptr;
                    return;
                }*/
                break;
            }
            case BaseEntity::Directory:
            {
                std::shared_ptr<DirectoryEntity> d = std::dynamic_pointer_cast<DirectoryEntity>(ent);
                if (!d) {
                    fprintf(stderr, "[WARN] Invalid directory entry at offset %016llx\n", offset);
                    return;
                }
                /*for (auto &child: d->get_children()) {
                    child_to_parent[child] = ent;
                }*/
                break;
            }
            default:
                fprintf(stderr, "[WARN] Unknown entity type\n");
                break;
            }
        } else {
            // Bad sector, mark it in the fat
        }
    });
#endif


    return 0;
}
