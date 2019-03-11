//
//  filesystem.hpp
//  ExFATRestore
//
//  Created by Paul Ciarlo on 2/11/19.
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

#ifndef _io_github_paulyc_filesystem_hpp_
#define _io_github_paulyc_filesystem_hpp_

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <variant>
#include <memory>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <locale>
#include <codecvt>
#include <list>
#include <vector>

#include "exception.hpp"
#include "logger.hpp"
#include "entity.hpp"

namespace io {
namespace github {
namespace paulyc {
namespace ExFATRestore {

class BaseEntity;

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
class ExFATFilesystem : public Loggable
{
public:
    ExFATFilesystem(const char *devname, size_t devsize, size_t partition_first_sector);
    virtual ~ExFATFilesystem();

    std::shared_ptr<BaseEntity> loadEntity(uint8_t *entry_offset, std::shared_ptr<BaseEntity> parent);

    void init_metadata();
    void write_metadata();

    void restore_all_files(const std::string &restore_dir_name, const std::string &textlogfilename);

    void textLogToBinLog(const std::string &textlogfilename, const std::string &binlogfilename);

private:
    int _fd;
    uint8_t *_mmap;
    size_t _devsize;
    uint8_t *_partition_start;
    uint8_t *_partition_end;

    // not part of actual partition, to be copied over later after being initialized
    fs_boot_region<SectorSize> _boot_region;
    fs_root_directory<SectorSize, SectorsPerCluster> _root_directory;

    // pointer to the start of the actual mmap()ed partition
    fs_filesystem<SectorSize, SectorsPerCluster, NumSectors> *_fs; // actual mmaped filesystem

    std::unordered_map<fs_entry*, std::shared_ptr<BaseEntity>> _offset_to_entity_mapping;
    std::unique_ptr<RootDirectoryEntity> _root_directory_entity;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> _cvt;

    std::vector<char16_t> _invalid_file_name_characters;
};

}
}
}
}

#include "filesystem.cpp"

#endif /* _io_github_paulyc_filesystem_hpp_ */
