//
//  entity.cpp - File/Directory Entity on Disk
//  resurrExion
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

#include "entity.hpp"

namespace github {
namespace paulyc {
namespace resurrExion {

BaseEntity::BaseEntity(void *entry_start, uint8_t num_entries, std::shared_ptr<BaseEntity> parent, const std::string &name) :
    _fs_entries((exfat::metadata_entry_u*)entry_start),
    _num_entries(num_entries),
    _parent(parent),
    _name(name)
{
}

DirectoryEntity::DirectoryEntity(
    void *entry_start,
    int num_entries,
    std::shared_ptr<BaseEntity> parent,
    const std::string &name) :
    BaseEntity(entry_start, num_entries, parent, name)
{

}

} /* namespace resurrExion */
} /* namespace paulyc */
} /* namespace github */