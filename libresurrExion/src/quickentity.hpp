//
//  quickentity.hpp
//  resurrExion
//
//  Created by Paul Ciarlo on 20 July 2020.
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

#ifndef RESURREX_QUICKENTITY_HPP
#define RESURREX_QUICKENTITY_HPP

#include "quick.hpp"

Entity * make_entity(byteofs_t offset, exfat::file_directory_entry_t *fde, const std::string &suggested_name);

inline byteofs_t cluster_number_to_offset(clusterofs_t cluster) {
    // have to subtract 2 because the first cluster in the cluster heap has index 2 and
    // by my definition the cluster heap simply starts there
    return ((cluster) * SectorsPerCluster + ClusterHeapStartSectorRelWholeDisk) * SectorSize;
}

class Entity{
public:
    Entity() : _name("ROOT"), _offset(0), _fde(nullptr), _parent(nullptr) {}

    Entity(byteofs_t offset, exfat::file_directory_entry_t *fde) :
        _offset(offset), _fde(fde), _parent(nullptr) {
            _streamext = reinterpret_cast<exfat::stream_extension_entry_t*>(fde+1);
            _nameent = reinterpret_cast<exfat::file_name_entry_t*>(fde+2);
            _alloc_possible = _streamext->flags & exfat::ALLOCATION_POSSIBLE;
            _contiguous = _streamext->flags & exfat::NO_FAT_CHAIN;
            if (_alloc_possible) {
                _first_cluster = _streamext->first_cluster;
                _data_length = _streamext->size;
                std::cout << "size: " << _streamext->size << " valid_size: " << _streamext->valid_size << " fde: " << fde << std::endl;
                if (_first_cluster == 0) {
                    _data_offset = offset + static_cast<uint64_t>((_fde->continuations + 2)*sizeof(exfat::file_directory_entry_t));
                } else {
                    _data_offset = cluster_number_to_offset(_first_cluster);
                }
            } else {
                // FirstCluster and DataLength are undefined
                _first_cluster = 0;
                _data_length = 0;
            }
        }

    virtual ~Entity() {}
    virtual byteofs_t get_offset() const { return _offset; }
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

    virtual clusterofs_t get_first_cluster() const {
        return _first_cluster;
    }

    virtual byteofs_t get_data_length() const {
        return _data_length;
    }

    virtual byteofs_t get_data_offset() const {
        return _data_offset;
    }

    virtual byteofs_t get_parent_offset() const {
        if (_parent != nullptr) {
            return _parent->get_offset();
        } else {
            return 0;
        }
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

    virtual std::string to_string() const { return "ENTITY"; }

public:
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

class File:public Entity{
public:
    File(byteofs_t offset, exfat::file_directory_entry_t *fde) :
        Entity(offset, fde) {}
    virtual ~File() {}
    virtual std::string to_string() const { return "FILE"; }
    void copy_to_dir(uint8_t *mmap, const std::string &abs_dir);
    uint32_t data_crc32(uint8_t *mmap);
};

class Directory:public Entity{
public:
    Directory() : Entity() {} //special case for root

    Directory(byteofs_t offset, exfat::file_directory_entry_t *fde) : Entity(offset, fde), _dirty(false) {
        //_sde = reinterpret_cast<struct exfat::secondary_directory_entry_t*>(fde+1);
        //printf("Dir %016lx continuation count %d sde first cluster %08x data length %016lx\n", offset, get_num_continuations(), _sde->first_cluster, _sde->data_length);
    }
    virtual ~Directory() {
        for (const auto &child: _children) {
            delete child.second;
        }
    }

    /*virtual uint32_t get_first_cluster() const {
        return _sde->first_cluster;
    }
    virtual uint64_t get_data_length() const {
        return _sde->data_length;
    }*/
    virtual void add_child(Entity *child, bool dirty=false) {
        if (child->get_parent() != nullptr) {
            Directory *olddir = reinterpret_cast<Directory*>(child->get_parent());
            olddir->remove_child(child);
        }
        child->set_parent(this);
        this->_children[child->get_offset()] = child;
        _dirty |= dirty;
    }
    void remove_child(Entity *child) {
        delete this->_children[child->get_offset()];
    }
    bool is_full() const { return _children.size() >= 254; } // ?

    virtual std::string to_string() const { return "DIRECTORY"; }

    void dump_files(uint8_t *mmap, const std::string &abs_dir, std::function<void(File*)> yield, bool actually_copy) {
        for (auto [ofs, ent]: _children) {
            if (typeid(*ent) == typeid(File)) {
                File *f = reinterpret_cast<File*>(ent);
                if (f->is_contiguous()) {
                    if (actually_copy) { //hacky restart midway through
                        f->copy_to_dir(mmap, abs_dir);
                    }
                } else {
                    std::cerr << "Non-contiguous file: " << f->get_name() << std::endl;
                }
                yield(f);
            } else if (typeid(*ent) == typeid(Directory)) {
                Directory *d = reinterpret_cast<Directory*>(ent);
                std::cout << "writing dir " << d->_name << std::endl;
                std::string new_abs_dir = abs_dir + std::string("/") + std::string(d->_name.c_str());
                mkdir(new_abs_dir.c_str(), 0777);
                d->dump_files(mmap, new_abs_dir, yield, actually_copy);
                std::cout << "done writing dir " << d->_name << std::endl;
            }
        }
    }

private:
    //struct exfat::primary_directory_entry_t *_pde;
    //struct exfat::secondary_directory_entry_t *_sde;
    //uint8_t _num_secondary_entries;
    //std::vector<struct exfat::secondary_directory_entry_t> _child_entries;
    std::map<byteofs_t, Entity*> _children;
    bool _dirty;
};

#endif /* RESURREX_QUICKENTITY_HPP */
