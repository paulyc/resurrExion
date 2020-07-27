//
//  quickfs.hpp
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

#ifndef RESURREX_QUICKFS_HPP
#define RESURREX_QUICKFS_HPP

#include "quick.hpp"

class FilesystemStub
{
public:
    FilesystemStub();

    ~FilesystemStub();

    void open(const std::string &devpath);

    void close();

#ifdef HAVE_ENTITY_OFFSET_MAPS
    void log_results(const char *orphanlogfilename);

    // this can break them down into exfat-size chunks eventually
    /*std::list<std::shared_ptr<Directory>>*/
    void adopt_orphans();
#endif

    bool parseTextLog(const std::string &filename);

    // confirmed working by checksumming a known file...
    std::vector<uint8_t> read_file_contents(File *f);

    void dump_directory(Directory *d, const std::string &dirname, std::function<void(File*)> yield, bool actually_write);

    Entity * loadEntityOffset(byteofs_t entity_offset, const std::string &suggested_name);

    void write_file_data(File *f, const std::string &filename);

    void load_directory(Directory *d);

    int      _fd;
    uint8_t *_mmap;
    uint8_t *_partition_start;
    uint8_t *_partition_end;
    std::vector<char16_t> _invalid_file_name_characters;
#ifdef HAVE_ENTITY_OFFSET_MAPS
    std::unordered_map<Entity*, byteofs_t> _entities_to_offsets;
    std::unordered_map<byteofs_t, Entity*> _offsets_to_entities;
#endif
};

#endif /* RESURREX_QUICKFS_HPP */
