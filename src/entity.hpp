//
//  entity.hpp - File/Directory Entity on Disk
//  ExFATRestore
//
//  Created by Paul Ciarlo on 9 March 2019.
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

#ifndef _io_github_paulyc_entity_hpp_
#define _io_github_paulyc_entity_hpp_

#include "exfat_structs.hpp"

#include <list>
#include <string>
#include <memory>

namespace io {
namespace github {
namespace paulyc {
namespace ExFATRestore {

class BaseEntity
{
public:
    BaseEntity(void *entry_start, int num_entries, std::shared_ptr<BaseEntity> parent, const std::string &name);
    virtual ~BaseEntity() {}

    fs_entry *get_entity_start() const {
        return _fs_entries;
    }
    int get_file_info_size() const {
        return _num_entries * sizeof(struct fs_entry);
    }
    uint32_t get_start_cluster() const {
        return ((struct fs_stream_extension_entry *)(_fs_entries + 1))->first_cluster;
    }
    uint64_t get_size() const {
        return ((struct fs_stream_extension_entry *)(_fs_entries + 1))->size;
    }
    std::string get_name() const {
        return _name;
    }
protected:
    struct fs_entry *_fs_entries;
    int _num_entries;
    std::shared_ptr<BaseEntity> _parent;
    std::string _name;
};

class FileEntity : public BaseEntity
{
public:
    FileEntity(void *entry_start, int num_entries, std::shared_ptr<BaseEntity> parent, const std::string &name) :
        BaseEntity(entry_start, num_entries, parent, name) {}

    bool is_contiguous() const {
        return ((struct fs_stream_extension_entry *)(this->_fs_entries + 1))->flags & CONTIGUOUS;
    }
};

class DirectoryEntity : public BaseEntity
{
public:
    DirectoryEntity(void *entry_start, int num_entries, std::shared_ptr<BaseEntity> parent, const std::string &name);

    void add_child(std::shared_ptr<BaseEntity> child) { _children.push_back(child); }
    const std::list<std::shared_ptr<BaseEntity>> &get_children() const { return _children; }

protected:
    std::list<std::shared_ptr<BaseEntity>> _children;
};

class RootDirectoryEntity : public DirectoryEntity
{
public:
    RootDirectoryEntity(void *entry_start) : DirectoryEntity(entry_start, 0, nullptr, "ROOT") {}
};

}
}
}
}

#endif /* _io_github_paulyc_entity_hpp_ */
