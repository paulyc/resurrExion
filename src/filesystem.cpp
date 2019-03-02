//
//  filesystem.cpp
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

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <variant>
#include <memory>

#include "filesystem.hpp"
#include "exception.hpp"
#include "recoverylog.hpp"

using io::github::paulyc::ExFATRestore::ExFATFilesystem;

io::github::paulyc::ExFATRestore::ExFATFilesystem::ExFATFilesystem(const char *devname, size_t devsize) :
    _fd(-1),
    _mmap((uint8_t*)MAP_FAILED),
    _devsize(devsize)
{
    _fd = open(devname, O_RDONLY);
    if (_fd == -1) {
        throw io::github::paulyc::ExFATRestore::posix_exception(errno);
    }

    _mmap = (uint8_t*)mmap(0, _devsize, PROT_READ, MAP_PRIVATE, _fd, 0);
    if (_mmap == (uint8_t*)MAP_FAILED) {
        throw io::github::paulyc::ExFATRestore::posix_exception(errno);
    }
}

io::github::paulyc::ExFATRestore::ExFATFilesystem::~ExFATFilesystem()
{
    if (_mmap != MAP_FAILED) {
        munmap(_mmap, _devsize);
    }

    if (_fd != -1) {
        close(_fd);
    }
}

std::shared_ptr<ExFATFilesystem::BaseEntry> io::github::paulyc::ExFATRestore::ExFATFilesystem::loadEntry(size_t entry_offset)
{
    uint8_t *buf = (uint8_t *)(_mmap + entry_offset);
    struct fs_file_directory_entry *fde = (struct fs_file_directory_entry*)(buf);
    struct fs_stream_extension_entry *streamext = (struct fs_stream_extension_entry*)(buf+32);

    if (fde->type != FILE_INFO1 || streamext->type != FILE_INFO2) {
        return std::shared_ptr<ExFATFilesystem::BaseEntry>();
    }

    const int continuations = fde->continuations;
    if (continuations < 2 || continuations > 18) {
        return std::shared_ptr<ExFATFilesystem::BaseEntry>();
    }

    int i;
    uint16_t chksum = 0;

    for (i = 0; i < 32; ++i) {
        if (i != 2 && i != 3) {
            chksum = ((chksum << 15) | (chksum >> 1)) + buf[i];
        }
    }

    for (; i < continuations * sizeof(struct fs_entry); ++i) {
        chksum = ((chksum << 15) | (chksum >> 1)) + buf[i];
    }

    if (chksum != fde->checksum) {
        return std::shared_ptr<ExFATFilesystem::BaseEntry>();
    }

    if (fde->attrib & DIR) {
        return std::make_shared<ExFATFilesystem::DirectoryEntry>(buf, continuations + 1);
    } else {
        return std::make_shared<ExFATFilesystem::FileEntry>(buf, continuations + 1);
    }
}

io::github::paulyc::ExFATRestore::ExFATFilesystem::BaseEntry::BaseEntry(void *entry_start, int num_entries)
{
    _fs_entries = (struct fs_entry *)entry_start;
    _num_entries = num_entries;
}

void io::github::paulyc::ExFATRestore::ExFATFilesystem::restore_all_files(const std::string &restore_dir_name, const std::string &textlogfilename)
{
    io::github::paulyc::ExFATRestore::RecoveryLogReader reader(textlogfilename);

    reader.parseTextLog([this, &restore_dir_name](size_t offset, std::variant<std::string, std::exception, bool> entry_info){
        if (std::holds_alternative<std::string>(entry_info)) {
            // File entry
            const std::string filename = std::get<std::string>(entry_info);
            std::shared_ptr<ExFATFilesystem::FileEntry> ent = std::dynamic_pointer_cast<ExFATFilesystem::FileEntry>(loadEntry(offset));
                //std::shared_ptr<ExFATFilesystem::FileEntry>(dynamic_cast<ExFATFilesystem::FileEntry>(loadEntry(offset)));
            if (!ent) {
                std::cerr << "Invalid file entry at offset " << offset << std::endl;
                return;
            }
            if (ent->is_contiguous()) {
                std::ofstream restored_file(restore_dir_name + std::string("/") + filename, std::ios_base::binary);
                const uint32_t start_cluster = ent->get_start_cluster();
                const size_t file_offset = start_cluster * cluster_size_bytes + cluster_heap_disk_start_sector * sector_size_bytes;
                const size_t file_size = ent->get_size();
                std::cout << "recovering file " << filename <<
                    " at cluster offset " << start_cluster <<
                    " disk offset " << file_offset <<
                    " size " << file_size << std::endl;
                restored_file.write((const char *)(_mmap + file_offset), file_size);
                restored_file.close();
            } else {
                std::cerr << "Non-contiguous file: " << filename << std::endl;
            }
        } else if (std::holds_alternative<bool>(entry_info)) {
            // Bad sector, don't care
        } else {
            // Exception
            std::exception &ex = std::get<std::exception>(entry_info);
            std::cerr << "entry_info had " << typeid(ex).name() << " with message: " << ex.what() << std::endl;
        }
    });
}
