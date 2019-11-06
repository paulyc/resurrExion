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

#include "exfat_structs.hpp"

using namespace github::paulyc;
using namespace github::paulyc::resurrExion;

typedef uint8_t my_cluster[512*512];

namespace {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cvt;
}

std::string get_utf8_filename(exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde)
{
    const int continuations = fde->continuations;
    const int namelen = sde->name_length;
    std::basic_string<char16_t> u16s;
    for (int c = 2; c <= continuations; ++c) {
        exfat::file_name_entry_t *n = (exfat::file_name_entry_t *)(((uint8_t*)fde) + c*32);
        if (n->type == exfat::FILE_NAME) {
            for (int i = 0; i < sizeof(n->name); ++i) {
                if (u16s.length() == namelen) {
                    return cvt.to_bytes(u16s);
                } else {
                    u16s.push_back((char16_t)n->name[i]);
                }
            }
        }
    }
    return cvt.to_bytes(u16s);
}

class Entity;

Entity * make_entity(std::streamoff offset, exfat::file_directory_entry_t *fde);

class Entity{
public:
    Entity(std::streamoff offset, exfat::file_directory_entry_t *fde) :
        _offset(offset), _fde(fde), _parent(nullptr) {
            _streamext = (exfat::stream_extension_entry_t*)(fde+1);
            _nameent = (exfat::file_name_entry_t*)(fde+2);
        }

    virtual ~Entity() {}
    virtual std::streamoff get_offset() const { return _offset; }
    virtual std::string get_name() const { return _name; }
    virtual Entity * get_parent() const { return _parent; }
    virtual void set_parent(Entity *parent) { _parent = parent; }

    virtual Entity * load_name() {
        _name = get_utf8_filename(_fde, _streamext);
        return this; //convenience
    }

public: // can't deal
    std::string _name;
    std::streamoff _offset;
    exfat::file_directory_entry_t *_fde;
    exfat::stream_extension_entry_t *_streamext;
    exfat::file_name_entry_t *_nameent;
    Entity *_parent;
};

class File:public Entity{
public:
    File(std::streamoff offset, exfat::file_directory_entry_t *fde) : Entity(offset, fde) {}
    virtual ~File() {}
    bool is_contiguous() const {
        return _streamext->flags & exfat::CONTIGUOUS;
    }
};

class Directory:public Entity{
public:
    Directory(std::streamoff offset, exfat::file_directory_entry_t *fde) : Entity(offset, fde) {
    }
    virtual ~Directory() {}

    virtual void add_child(Entity *child) {
        child->set_parent(this);
        this->children.push_back(child);
    }
    virtual const std::vector<Entity*>& get_children() const { return children; }
private:
    std::vector<Entity*> children;
};

Entity * make_entity(std::streamoff offset, exfat::file_directory_entry_t *fde) {
    if (fde->attributes & exfat::DIRECTORY) {
        return (new Directory(offset, fde))->load_name();
    } else {
        return (new File(offset, fde))->load_name();
    }
}

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
        _mmap((uint8_t*)MAP_FAILED),
        _partition_start(nullptr),
        _partition_end(nullptr)
    {
        _invalid_file_name_characters = {'"', '*', '/', ':', '<', '>', '?', '\\', '|'};
        for (char16_t ch = 0; ch <= 0x001F; ++ch) {
            _invalid_file_name_characters.push_back(ch);
        }
    }

    void open(std::string devpath) {
        const bool write_changes = false;
        const int oflags = write_changes ? O_RDWR | O_DSYNC | O_RSYNC : O_RDONLY;
        _fd = ::open(devpath.c_str(), oflags);
        if (_fd == -1) {
            std::cerr << "failed to open device " << devpath << std::endl;
            throw std::system_error(std::error_code(errno, std::system_category()));
        }

        const int mprot = write_changes ? PROT_READ | PROT_WRITE : PROT_READ;
        const int mflags = write_changes ? MAP_SHARED : MAP_PRIVATE;
        _mmap = (uint8_t*)mmap(0, DiskSize, mprot, mflags, _fd, 0);
        if (_mmap == (uint8_t*)MAP_FAILED) {
            std::cerr << "error opening mmap" << std::endl;
            throw std::system_error(std::error_code(errno, std::system_category()));
        }

         _partition_start = _mmap + PartitionStartSector * SectorSize;
         _partition_end = _partition_start + (NumSectors + 1) * SectorSize;
    }

    void close() {
        _partition_end = nullptr;
        _partition_start = nullptr;

        if (_mmap != MAP_FAILED) {
            munmap(_mmap, DiskSize);
            _mmap = (uint8_t*)MAP_FAILED;
        }

        if (_fd != -1) {
            ::close(_fd);
            _fd = -1;
        }
    }

    ~FilesystemStub()
    {
        close();
    }

    void parseTextLog(const std::string &filename)
    {
        std::unordered_map<Entity*, Entity*> child_to_parent;
        std::unordered_map<Entity*, std::streamoff> entity_to_offset;

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
                    Entity * ent = loadEntityOffset(offset);
                    _entities_to_offsets[ent] = offset;
                    _offsets_to_entities[offset] = ent;
                } catch (std::exception &ex) {
                    std::cerr << "Writing file entry to binlog, got exception: " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                    std::cerr << "Invalid textlog FDE line, skipping: " << line << std::endl;
                }
            } else if (std::regex_match(line, sm, badsector)) {
                std::streamoff offset;
                try {
                    offset = std::stol(sm[1], nullptr, 16);
                    std::cerr << "Noted bad sector at offset " << offset << std::endl;
                } catch (std::exception &ex) {
                    std::cerr << "Writing bad sector to binlog, got exception " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                    std::cerr << "Invalid textlog BAD_SECTOR line, skipping: " << line << std::endl;
                }
            } else {
                std::cerr << "Unknown textlog line format: " << line << std::endl;
            }
        }
    }

    void find_orphans() {
        std::cerr << "find_orphans" << std::endl;
        FILE *orphanlog = fopen("/Users/paulyc/Development/resurrExion/orphan.log", "w");
        for (auto it = _offsets_to_entities.begin(); it != _offsets_to_entities.end(); ++it) {
            const std::streamoff offset = it->first;
            const Entity *ent = it->second;
            if (ent->get_parent() == nullptr) {
                fprintf(orphanlog, "ORPHAN %016lld %s", offset, ent->get_name().c_str());
            }
        }
        fclose(orphanlog);
    }

    Entity * loadEntityOffset(std::streamoff entity_offset) {
        //fprintf(stderr, "loadEntityOffset[%016lld]\n", entity_offset);
        return loadEntity(entity_offset, _mmap + entity_offset);
    }

    Entity * loadEntity(std::streamoff offset, uint8_t *entity_start)
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
            fprintf(stderr, "bad file entry checksum at offset %016llx\n", offset);
            return nullptr;
        }

        return make_entity(offset, fde);
    }

    void load_directory(std::streamoff offset, Directory *d) {
        const uint32_t start_cluster = d->_streamext->first_cluster;
        const uint8_t num_entries = d->_fde->continuations + 1;
        exfat::metadata_entry_u *dirent = nullptr;
        if (start_cluster == 0) {
            // directory children immediately follow
            dirent = (exfat::metadata_entry_u *)(d->_fde + num_entries);
            offset += sizeof(d->_fde) * num_entries;
        } else {
            fprintf(stderr, "SKETCH!\n");
            //throw std::exception();
            const size_t cluster_heap_start_byte_offset = 0x283D8 * 512;
            my_cluster *c = (my_cluster*)(_mmap + cluster_heap_start_byte_offset);
            dirent = (exfat::metadata_entry_u*)(c+start_cluster);
            offset = (uint8_t*)dirent - _mmap;
        }

        const uint8_t num_secondary_entries = dirent->primary_directory_entry.secondary_count;
        for (uint8_t secondary_entry = 0; secondary_entry < num_secondary_entries; ++secondary_entry) {
            Entity *child = make_entity(offset, &dirent->file_directory_entry);
            child->set_parent(d);
            d->add_child(child);
            dirent += child->_fde->continuations + 1;
            offset += sizeof(exfat::metadata_entry_u) * (child->_fde->continuations + 1);
        }
    }

    int      _fd;
    uint8_t *_mmap;
    uint8_t *_partition_start;
    uint8_t *_partition_end;
    std::vector<char16_t> _invalid_file_name_characters;
    std::unordered_map<Entity*, std::streamoff> _entities_to_offsets;
    std::unordered_map<std::streamoff, Entity*> _offsets_to_entities;
};

int main(int argc, char *argv[]) {
    FilesystemStub stub;
    stub.open("/dev/disk2");
    stub.parseTextLog("/Users/paulyc/Development/resurrExion/recovery.log");
    stub.find_orphans();
    stub.close();
    return 0;
}
