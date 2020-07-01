//
//  quick.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 11/5/19.
//
//  Copyright (C) 2020 Paul Ciarlo <paul.ciarlo@gmail.com>.
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
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
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
#include <list>

#include "exfat_structs.hpp"

using namespace github::paulyc;
using namespace github::paulyc::resurrExion;

typedef uint8_t my_cluster[512*512];

constexpr static size_t SectorSize             = 512;
constexpr static size_t SectorsPerCluster      = 512;
constexpr static size_t NumSectors             = 7813560247;
constexpr static size_t ClustersInFat          = (NumSectors - 0x283D8) / 512;
constexpr static size_t PartitionStartSector   = 0x64028;
constexpr static size_t ClusterHeapStartSector = 0x283D8; // relative to partition start
constexpr static size_t ClusterHeapStartSectorRelWholeDisk = PartitionStartSector + ClusterHeapStartSector;
constexpr static size_t ClusterHeapStartOffset = ClusterHeapStartSectorRelWholeDisk * SectorSize;
constexpr static size_t DiskSize               = (NumSectors + PartitionStartSector) * SectorSize;

namespace {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cvt;
}

bool is_valid_filename_char(char16_t ch) {
    switch (ch) {
    case '\'':
    case '*':
    case '/':
    case ':':
    case '<':
    case '>':
    case '?':
    case '\\':
    case '|':
        return false;
    default:
        return ch >= 0x0020;
    }
}

std::string get_utf8_filename(exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde)
{
    const size_t entry_count = fde->continuations + 1;
    // i dont know that this means anything without decoding the string because UTF-16 is so dumb re: the "emoji problem"
    const size_t namelen = sde->name_length;
    struct exfat::file_name_entry_t *n = reinterpret_cast<struct exfat::file_name_entry_t*>(fde);
    std::basic_string<char16_t> u16s;
    for (size_t c = 2; c < entry_count && u16s.length() <= namelen; ++c) {
        if (n[c].type == exfat::FILE_NAME) {
            for (size_t i = 0; i < exfat::file_name_entry_t::FS_FILE_NAME_ENTRY_SIZE && u16s.length() <= namelen; ++i) {
                u16s.push_back((char16_t)n[c].name[i]);
            }
        }
    }
    return cvt.to_bytes(u16s);
}

class Entity;

Entity * make_entity(std::streamoff offset, exfat::file_directory_entry_t *fde, const std::string &suggested_name);

std::streamoff cluster_number_to_offset(uint32_t cluster) {
    // have to subtract 2 because the first cluster in the cluster heap has index 2 and
    // by my definition the cluster heap simply starts there
    return ((cluster) * SectorsPerCluster + ClusterHeapStartSectorRelWholeDisk) * SectorSize;
}

class Entity{
public:
    Entity(std::streamoff offset, exfat::file_directory_entry_t *fde) :
        _offset(offset), _fde(fde), _parent(nullptr) {
            _streamext = reinterpret_cast<exfat::stream_extension_entry_t*>(fde+1);
            _nameent = reinterpret_cast<exfat::file_name_entry_t*>(fde+2);
            _alloc_possible = _streamext->flags & exfat::ALLOCATION_POSSIBLE;
            _contiguous = _streamext->flags & exfat::NO_FAT_CHAIN;
            if (_alloc_possible) {
                _first_cluster = _streamext->first_cluster;
                _data_length = _streamext->valid_size;
                if (_first_cluster == 0) {
                    _data_offset = offset + static_cast<std::streamoff>((_fde->continuations + 2)*sizeof(exfat::file_directory_entry_t));
                } else {
                    _data_offset = cluster_number_to_offset(_first_cluster);
                }
            } else {
                // FirstCluster and DataLength are undefined
                _first_cluster = 0;
                _data_length = 0;
            }
        }

    virtual ~Entity();
    virtual std::streamoff get_offset() const { return _offset; }
    virtual std::string get_name() const { return _name; }
    virtual Entity * get_parent() const { return _parent; }
    virtual void set_parent(Entity *parent) { _parent = parent; }

    // defname is given if this fails
    virtual Entity * load_name(const std::string &suggested_name) {
        try {
            _name = get_utf8_filename(_fde, _streamext);
        } catch (const std::exception &ex) {
            fprintf(stderr, "get_utf8_filename failed with [%s]: %s\n", typeid(ex).name(), ex.what());
            fprintf(stderr, "using suggested name %s for entity %016lx\n", suggested_name.c_str(), get_offset());
            _name = suggested_name;
        }
        return this; //convenience
    }

    virtual uint32_t get_first_cluster() const {
        return _first_cluster;
    }

    virtual uint64_t get_data_length() const {
        return _data_length;
    }

    virtual std::streamoff get_data_offset() const {
        return _data_offset;
    }

    virtual uint8_t* get_data_ptr(uint8_t *mmap) const {
        return _data_offset + mmap;
    }

    virtual bool is_contiguous() const {
        return _contiguous;
    }

    virtual bool is_alloc_possible() const {
        return _alloc_possible;
    }

    virtual uint8_t get_num_continuations() const {
        return _fde->continuations;
    }

public:
    std::string _name;
    std::streamoff _offset;
    exfat::file_directory_entry_t *_fde;
    exfat::stream_extension_entry_t *_streamext;
    exfat::file_name_entry_t *_nameent;
    Entity *_parent;
    std::streamoff _data_offset;
    uint64_t _data_length;
    uint32_t _first_cluster;
    bool _alloc_possible;
    bool _contiguous;
    uint8_t padding[2];
};

class File:public Entity{
public:
    File(std::streamoff offset, exfat::file_directory_entry_t *fde) :
        Entity(offset, fde) {}
    virtual ~File();
};

class Directory:public Entity{
public:
    Directory(std::streamoff offset, exfat::file_directory_entry_t *fde) : Entity(offset, fde), _dirty(false) {
        //_sde = reinterpret_cast<struct exfat::secondary_directory_entry_t*>(fde+1);
        //printf("Dir %016lx continuation count %d sde first cluster %08x data length %016lx\n", offset, get_num_continuations(), _sde->first_cluster, _sde->data_length);
    }
    virtual ~Directory();

    /*virtual uint32_t get_first_cluster() const {
        return _sde->first_cluster;
    }
    virtual uint64_t get_data_length() const {
        return _sde->data_length;
    }*/
    virtual void add_child(Entity *child, bool dirty=false) {
        child->set_parent(this);
        this->_children.push_back(child);
        _dirty |= dirty;
    }
    virtual const std::vector<Entity*>& get_children() const { return _children; }
    bool is_full() const { return _children.size() >= 254; } // ?
private:
    //struct exfat::primary_directory_entry_t *_pde;
    //struct exfat::secondary_directory_entry_t *_sde;
    //uint8_t _num_secondary_entries;
    //std::vector<struct exfat::secondary_directory_entry_t> _child_entries;
    std::vector<Entity*> _children;
    bool _dirty;
};

Entity::~Entity() {}
File::~File() {}
Directory::~Directory() {}

Entity * make_entity(std::streamoff offset, exfat::file_directory_entry_t *fde, const std::string &suggested_name) {
    if (fde->attribute_flags & exfat::DIRECTORY) {
        return (new Directory(offset, fde))->load_name(suggested_name);
    } else {
        return (new File(offset, fde))->load_name(suggested_name);
    }
}

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
        //const bool write_changes = false;
        //const int oflags = write_changes ? O_RDWR | O_DSYNC | O_RSYNC : O_RDONLY;
        _fd = ::open(devpath.c_str(), O_RDONLY | O_DSYNC | O_RSYNC);
        if (_fd == -1) {
            std::cerr << "failed to open device " << devpath << std::endl;
            throw std::system_error(std::error_code(errno, std::system_category()));
        }

        //const int mprot = write_changes ? PROT_READ | PROT_WRITE : PROT_READ;
        //const int mflags = write_changes ? MAP_SHARED : MAP_PRIVATE;
        // Darwin doesn't like this with DiskSize == 3.6TB. Not sure if that's why,
        // but only about 1G ends up actually getting mapped for my disk with ~270000 entities....
        _mmap = (uint8_t*)mmap(nullptr, DiskSize, PROT_READ, MAP_PRIVATE, _fd, 0);
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
        //std::unordered_map<Entity*, Entity*> child_to_parent;
        //std::unordered_map<Entity*, std::streamoff> entity_to_offset;

        std::regex fde("FDE ([0-9a-fA-F]{16}) (.*)");
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
                    Entity * ent = loadEntityOffset(offset, filename);
                    if (ent != nullptr) {
                        //printf("%016lx %s\n", ent->get_offset(), ent->get_name().c_str());
                    }
                } catch (std::exception &ex) {
                    std::cerr << "Writing file entry to binlog, got exception: [" << typeid(ex).name() << "] with msg: " << ex.what() << std::endl;
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

    void log_sql(const char *sqlfilename) {
        std::cerr << "log_sql" << std::endl;
        FILE *sqllog = fopen(sqlfilename, "w");
        for (auto it = _offsets_to_entities.begin(); it != _offsets_to_entities.end(); ++it) {
            const std::streamoff offset = it->first;
            const Entity *ent = it->second;
            if (ent == nullptr) {
                //fprintf(orphanlog, "NOENT %016lx\n", offset);
            } else {
                const Entity *parent = ent->get_parent();
                if (parent == nullptr) {
                    if (typeid(*ent) == typeid(File)) {
                        const File *f = dynamic_cast<const File*>(ent);
                        fprintf(sqllog, 
                            "INSERT INTO file(entry_offset, parent_directory_id, name, data_offset, data_len, is_contiguous, is_copied_off) VALUES "
                            "                (%016llx, NULL, '%s', %016llx, %016llx, %d, 0);\n", 
                            f->get_offset(), ent->get_name().c_str(), f->get_data_offset(), f->get_data_length(), f->is_contiguous() ? 1 : 0
                        );
                    } else {
                        const Directory *d = dynamic_cast<const Directory*>(ent);
                        fprintf(sqllog, 
                            "INSERT INTO directory(name, entry_offset, parent_directory_id) VALUES "
                            "                ('%s', %016llx, NULL);\n", 
                            d->get_offset(), ent->get_name().c_str()
                        );
                    }
                } else {
                    if (typeid(*ent) == typeid(File)) {
                        const File *f = dynamic_cast<const File*>(ent);
                        fprintf(sqllog, 
                            "INSERT INTO file(entry_offset, parent_directory_offset, name, data_offset, data_len, is_contiguous, is_copied_off) VALUES "
                            "                (%016llx, %016llx, '%s', %016llx, %016llx, %d, 0);\n", 
                            f->get_offset(), parent->get_offset(), ent->get_name().c_str(), f->get_data_offset(), f->get_data_length(), f->is_contiguous() ? 1 : 0
                        );
                        
                    } else {
                        const Directory *d = dynamic_cast<const Directory*>(ent);
                        fprintf(sqllog, 
                            "INSERT INTO directory(name, entry_offset, parent_directory_id) VALUES "
                            "                ('%s', %016llx, %016llx);\n", 
                            d->get_offset(), ent->get_name().c_str(), parent->get_offset()
                        );
                    }
                }
            }
        }
        fclose(sqllog);
    }

    void log_results(const char *orphanlogfilename) {
        std::cerr << "find_orphans" << std::endl;
        FILE *orphanlog = fopen(orphanlogfilename, "w");
        for (auto it = _offsets_to_entities.begin(); it != _offsets_to_entities.end(); ++it) {
            const std::streamoff offset = it->first;
            const Entity *ent = it->second;
            if (ent == nullptr) {
                fprintf(orphanlog, "NOENT %016lx\n", offset);
            } else {
                const Entity *parent = ent->get_parent();
                if (parent == nullptr) {
                    if (typeid(*ent) == typeid(File)) {
                        fprintf(orphanlog, "ORPHANFILE %016lx %s\n", offset, ent->get_name().c_str());
                    } else {
                        fprintf(orphanlog, "ORPHANDIR %016lx %s\n", offset, ent->get_name().c_str());
                    }
                } else {
                    if (typeid(*ent) == typeid(File)) {
                        const File *f = dynamic_cast<const File*>(ent);
                        if (f->is_contiguous()) {
                            fprintf(orphanlog, "FILE %016lx %016lx ", f->get_offset(), f->get_data_offset());
                        } else {
                            fprintf(orphanlog, "FRAGMENT %016lx ", f->get_offset());
                        }
                    } else {
                        const Directory *d = dynamic_cast<const Directory*>(ent);
                        fprintf(orphanlog, "DIRECTORY %016lx ", d->get_offset());
                    }
                    fprintf(orphanlog, " %016lx %s %s\n", parent->get_offset(), ent->get_name().c_str(), parent->get_name().c_str());
                }
            }
        }
        fclose(orphanlog);
    }

    std::shared_ptr<Directory> allocate_directory() {
        return std::make_shared<Directory>(0, nullptr);
    }

    std::list<std::shared_ptr<Directory>> adopt_orphans() {
        std::shared_ptr<Directory> adoptive_directory;
        std::list<std::shared_ptr<Directory>> new_directories;
        for (auto it = _offsets_to_entities.begin(); it != _offsets_to_entities.end(); ++it) {
            const std::streamoff offset = it->first;
            Entity *ent = it->second;
            if (ent != nullptr) {
                const Entity *parent = ent->get_parent();
                if (parent == nullptr) {
                    if (!adoptive_directory) {
                        adoptive_directory = allocate_directory();
                    } else if (adoptive_directory->is_full()) {
                        new_directories.push_back(adoptive_directory);
                        adoptive_directory = allocate_directory();
                    }
                    ent->set_parent(adoptive_directory.get());
                    adoptive_directory->add_child(ent, true);
                }
            }
        }
        if (adoptive_directory) {
            new_directories.push_back(adoptive_directory);
        }
        return new_directories;
    }

    void dirty_writeback() {

    }

    // confirmed working by checksumming a known file...
    std::vector<uint8_t> read_file_contents(File *f) {
        std::vector<uint8_t> contents(f->get_data_length());
        uint8_t *data = f->get_data_offset() + _mmap;
        memcpy(contents.data(), data, f->get_data_length());
        return contents;
    }

    Entity * loadEntityOffset(std::streamoff entity_offset, const std::string &suggested_name) {
        //fprintf(stderr, "loadEntityOffset[%016lld]\n", entity_offset);
        auto loaded_entity = _offsets_to_entities.find(entity_offset);
        if (loaded_entity != _offsets_to_entities.end()) {
            return loaded_entity->second;
        }

        uint8_t *entity_start = _mmap + entity_offset;

        exfat::file_directory_entry_t *fde = (exfat::file_directory_entry_t*)(entity_start);
        exfat::stream_extension_entry_t *streamext = (exfat::stream_extension_entry_t*)(entity_start+32);
        exfat::file_name_entry_t *n = (exfat::file_name_entry_t*)(entity_start+64);

        if (fde->type != exfat::FILE_DIR_ENTRY || streamext->type != exfat::STREAM_EXTENSION || n->type != exfat::FILE_NAME) {
            fprintf(stderr, "bad number of continuations at offset %016lx\n", entity_start - _mmap);
            return nullptr;
        }

        const uint8_t continuations = fde->continuations;
        if (continuations < 2 || continuations > 18) {
            fprintf(stderr, "bad number of continuations at offset %016lx\n", entity_start - _mmap);
            return nullptr;
        }

        size_t i;
        uint16_t chksum = 0;

        for (i = 0; i < sizeof(exfat::raw_entry_t); ++i) {
            if (i != 2 && i != 3) {
                chksum = uint16_t((chksum << 15) | (chksum >> 1)) + entity_start[i];
            }
        }

        for (; i < (continuations+1) * sizeof(exfat::raw_entry_t); ++i) {
            chksum = uint16_t((chksum << 15) | (chksum >> 1)) + entity_start[i];
        }

        if (chksum != fde->checksum) {
            fprintf(stderr, "bad file entry checksum at offset %016lx\n", entity_offset);
            return nullptr;
        }

        Entity *ent = make_entity(entity_offset, fde, suggested_name);
        _entities_to_offsets[ent] = entity_offset;
        _offsets_to_entities[entity_offset] = ent;
        if (typeid(*ent) == typeid(Directory)) {
            Directory *d = dynamic_cast<Directory*>(ent);
            this->load_directory(d);
        } else if (typeid(*ent) == typeid(File)) {
            File *f = dynamic_cast<File*>(ent);
            if (this->should_write_file_data(f)) {
                this->write_file_data(f, "wt.dts");
            }
        }
        return ent;
    }

    bool should_write_file_data(File *) const {
        return false;
    }

    void write_file_data(File *f, const std::string &filename) {
        std::vector<uint8_t> content = this->read_file_contents(f);
        FILE *ff = fopen(filename.c_str(), "wb");
        fwrite(content.data(), 1, content.size(), ff);
        fclose(ff);
    }

    void load_directory(Directory *d) {
        uint32_t first_cluster = d->get_first_cluster();
        fprintf(stderr, "load_directory @cluster %08x name %s\n", first_cluster, d->_name.c_str());
        if (first_cluster == 0) {
            fprintf(stderr, "  first cluster is zero! cant handle, return\n");
            return;
        }
        std::streamoff dataoffset = cluster_number_to_offset(first_cluster);
        uint64_t datalen = d->get_data_length();
        uint8_t *data = _mmap + dataoffset;
        uint8_t *enddata = data + datalen;
        struct exfat::secondary_directory_entry_t *dirent = (struct exfat::secondary_directory_entry_t *)(data);
        struct exfat::raw_entry_t *ent = (struct exfat::raw_entry_t*)(data);
        struct exfat::raw_entry_t *end = (struct exfat::raw_entry_t*)(enddata);
        for (;ent < end; ++ent) {
            if (ent->type == exfat::FILE_DIR_ENTRY) {
                Entity *e = this->loadEntityOffset(reinterpret_cast<uint8_t*>(ent) - _mmap, "noname");
                if (e != nullptr) {
                    d->add_child(e);
                    fprintf(stderr, "  child %s\n", e->get_name().c_str());
                    ent += e->get_num_continuations();
                }
            } else if (ent->type == 0) {
                //printf("  end directory listing\n");
                break;
            } else {
                switch (ent->type) {
                case exfat::STREAM_EXTENSION:
                case exfat::FILE_NAME:
                case exfat::FILE_DIR_DELETED:
                case exfat::STREAM_EXT_DELETED:
                case exfat::FILE_NAME_DELETED:
                    break;
                default:
                    fprintf(stderr, "  unknown raw ent type %02x\n", ent->type);
                    break;
                }
            }
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
    stub.open("/dev/sdb");
    stub.parseTextLog("recoverylog.log");
    stub.log_sql_results("logfile.sql");
    stub.close();
    return 0;
}

#if 0
int fix_orphans_method(const std::vector<std::string> &args) {
    if (args.size() < 2) {
        return -1;
    }
    FilesystemStub stub;
    stub.open(args[0]);
    stub.parseTextLog(args[1]);
    stub.adopt_orphans();
    stub.dirty_writeback();
    stub.close();
    return 0;
}

int orphans_method(const std::vector<std::string> &args) {
    if (args.size() < 3) {
        return -1;
    }
    FilesystemStub stub;
    stub.open(args[0]);
    RecoveryLog<> log;
    //stub.parseTextLog(args[1]);
    //stub.log_results(args[2].c_str());
    stub.log_sql_results("logfile.sql");
    stub.close();
    return 0;
}


int main(int argc, char *argv[]) {
    return orphans_method

    if (argc == 1) {
        fprintf(stderr, "forcing defaults\n");
        return orphans_method({"/dev/sdb", "recovery.log", "orphan.log"});
    } else if (argc == 2) {
        return orphans_method({argv[1], "recovery.log", "orphan.log"});
    } else if (argc == 3) {
        return orphans_method({argv[1], argv[2], "orphan.log"});
    } else if (argc == 4) {
        return orphans_method({argv[1], argv[2], argv[3]});
    } else if (argc != 4) {
        fprintf(stderr, "usage: %s <device> <recovery_log> <orphan_log>\n\n", argv[0]);
        return -1;
    }
}
#endif
