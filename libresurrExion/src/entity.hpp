//
//  entity.hpp - File/Directory Entity on Disk
//  resurrExion
//
//  Created by Paul Ciarlo on 9 March 2019.
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

#ifndef _github_paulyc_entity_hpp_
#define _github_paulyc_entity_hpp_

#include <list>
#include <memory>
#include <iostream>
#include <map>

#include "types.hpp"

namespace github {
namespace paulyc {
namespace resurrExion {

struct Entity;
struct File;
struct Directory;

Directory *GetRootDirectory();

struct Entity{
    Entity() : _name("ROOT"), _offset(INVALID_BYTEOFS), _fde(nullptr), _parent(nullptr) {}

    Entity(byteofs_t offset, exfat::file_directory_entry_t *fde) :
        _offset(offset), _fde(fde), _parent(nullptr) {
            _streamext = reinterpret_cast<exfat::stream_extension_entry_t*>(fde+1);
            _nameent = reinterpret_cast<exfat::file_name_entry_t*>(fde+2);
            _alloc_possible = _streamext->flags & exfat::ALLOCATION_POSSIBLE;
            _contiguous = _streamext->flags & exfat::NO_FAT_CHAIN;
            if (_alloc_possible) {
                _first_cluster = _streamext->first_cluster;
                _data_length = _streamext->size;
                std::cout << "size: " << _streamext->size << " valid_size: " << _streamext->valid_size << std::endl;
                if (_first_cluster == 0) {
                    _data_offset = offset + static_cast<uint64_t>((_fde->continuations + 2)*sizeof(exfat::file_directory_entry_t));
                } else {
                    _data_offset = cluster_number_to_offset(_first_cluster);
                }
            } else {
                // FirstCluster and DataLength are undefined
                _first_cluster = INVALID_CLUSTEROFS;
                _data_length = INVALID_BYTEOFS;
            }
        }

    virtual ~Entity() = default;

    static Entity * make_entity(byteofs_t offset, exfat::file_directory_entry_t *fde, const std::string &suggested_name);

    // defname is given if this fails
    virtual Entity * load_name(const std::string &suggested_name) {
        try {
            _name = get_utf8_filename(_fde, _streamext);
        } catch (const std::exception &ex) {
            fprintf(stderr, "get_utf8_filename failed with [%s]: %s\n", typeid(ex).name(), ex.what());
            fprintf(stderr, "using suggested name %s for entity %016lx\n", suggested_name.c_str(), this->_offset);
            _name = suggested_name;
        }
        return this; //convenience
    }


    virtual byteofs_t get_parent_offset() const {
        if (_parent != nullptr) {
            return _parent->_offset;
        } else {
            return INVALID_BYTEOFS;
        }
    }

    virtual uint8_t* get_data_ptr(uint8_t *mmap) const {
        return _data_offset + mmap;
    }

    virtual uint8_t get_num_continuations() const {
        return _fde->continuations;
    }

#ifdef NOBRAIN
    virtual size_t count_nodes() const = 0;
    virtual void dump_sql(Entity *parent, FILE *sqllog, size_t &count) = 0;
    virtual std::string to_string() const { return "ENTITY"; }
#endif

    std::string _name;
    byteofs_t _offset;
    exfat::file_directory_entry_t *_fde;
    exfat::stream_extension_entry_t *_streamext;
    exfat::file_name_entry_t *_nameent;
    Entity *_parent;
    byteofs_t _data_offset;
    byteofs_t _data_length;
    clusterofs_t _first_cluster;
    bool _alloc_possible;
    bool _contiguous;
    uint8_t padding[2];
};

struct File:public Entity{
    File(byteofs_t offset, exfat::file_directory_entry_t *fde) :
        Entity(offset, fde) {}
    virtual ~File() = default;
#ifdef NOBRAIN
    virtual void dump_sql(Entity *parent, FILE *sqllog, size_t &count) {
        ++count;
        fprintf(sqllog,
            "INSERT INTO file(entry_offset, parent_directory_offset, name, data_offset, data_len, is_contiguous, is_copied_off) VALUES "
            "                (0x%016lx, 0x%016lx, \"%s\", 0x%016lx, 0x%016lx, %d, 0);\n",
            this->_offset, parent->get_offset(), this->get_name().c_str(), this->get_data_offset(), this->get_data_length(), this->is_contiguous() ? 1 : 0
        );
    }
#endif
};

struct Directory:public Entity{
public:
    Directory() : Entity() {} //special case for root

    Directory(byteofs_t offset, exfat::file_directory_entry_t *fde) : Entity(offset, fde), _dirty(false) {
        //_sde = reinterpret_cast<struct exfat::secondary_directory_entry_t*>(fde+1);
        //printf("Dir %016lx continuation count %d sde first cluster %08x data length %016lx\n", offset, get_num_continuations(), _sde->first_cluster, _sde->data_length);
    }
    virtual ~Directory();

    virtual void add_child(Entity *child, bool dirty=false) {
        if (child->_parent != nullptr) {
            Directory *olddir = reinterpret_cast<Directory*>(child->_parent);
            olddir->remove_child(child);
        }
        child->_parent = this;
        this->_children[child->_offset] = child;
    }

    void remove_child(Entity *child) {
        delete this->_children[child->_offset];
        child->_parent = nullptr;
    }

    bool is_full() const { return _children.size() >= 254; } // ?

    void dump_files(uint8_t *mmap, const std::string &dirname) {
        for (auto [ofs, ent]: _children) {
            if (typeid(*ent) == typeid(File)) {
                File *f = reinterpret_cast<File*>(ent);
                if (f->_contiguous) {
                    std::string path = dirname + "/" + f->_name;
                    std::cout << "writing " << path << std::endl;
                    uint8_t *data = f->get_data_ptr(mmap);
                    size_t sz = f->_data_length;
                   // if (sz < 1000000) {
                   //     sz = 1000000;
                   // }
                    FILE *output = fopen(path.c_str(), "wb");
                    while (sz > 0) {
                        size_t write = sz < 0x10000 ? sz : 0x10000;
                        fwrite(data, write, 1, output);

                        data += write;
                        sz -= write;

                    }
                    fclose(output);
                    std::cout << "wrote " << path << std::endl;
                } else {
                    std::cerr << "Non-contiguous file: " << f->_name << std::endl;
                }
            } else if (typeid(*ent) == typeid(Directory)) {
                // recurse later
            }
        }
    }

private:
    //struct exfat::primary_directory_entry_t *_pde;
    //struct exfat::secondary_directory_entry_t *_sde;
    //uint8_t _num_secondary_entries;
    //std::vector<struct exfat::secondary_directory_entry_t> _child_entries;
    std::map<byteofs_t, Entity*> _children;
};

} /* namespace resurrExion */
} /* namespace paulyc */
} /* namespace github */

#endif /* _github_paulyc_entity_hpp_ */
