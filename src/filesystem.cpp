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

ExFATFilesystem::ExFATFilesystem(const char *devname, size_t devsize) :
    _fd(-1),
    _mmap((uint8_t*)MAP_FAILED),
    _devsize(devsize)
{
    _fd = open(devname, O_RDONLY);
    if (_fd == -1) {
        throw io::github::paulyc::posix_exception(errno);
    }

    _mmap = (uint8_t*)mmap(0, _devsize, PROT_READ, MAP_PRIVATE, _fd, 0);
    if (_mmap == (uint8_t*)MAP_FAILED) {
        throw io::github::paulyc::posix_exception(errno);
    }
}

ExFATFilesystem::~ExFATFilesystem()
{
    if (_mmap != MAP_FAILED) {
        munmap(_mmap, _devsize);
    }

    if (_fd != -1) {
        close(_fd);
    }
}

std::shared_ptr<ExFATFilesystem::BaseEntity> ExFATFilesystem::loadEntity(size_t entry_offset)
{
    uint8_t *buf = _mmap + entry_offset;
    struct fs_file_directory_entry *fde = (struct fs_file_directory_entry*)(buf);
    struct fs_stream_extension_entry *streamext = (struct fs_stream_extension_entry*)(buf+32);

    if (fde->type != FILE_INFO1 || streamext->type != FILE_INFO2) {
        std::cerr << "bad file entry types at offset " << std::hex << entry_offset << std::endl;
        return std::shared_ptr<ExFATFilesystem::BaseEntity>();
    }

    const int continuations = fde->continuations;
    if (continuations < 2 || continuations > 18) {
        std::cerr << "bad number of continuations at offset " << std::hex << entry_offset << std::endl;
        return std::shared_ptr<ExFATFilesystem::BaseEntity>();
    }

    int i;
    uint16_t chksum = 0;

    for (i = 0; i < 32; ++i) {
        if (i != 2 && i != 3) {
            chksum = ((chksum << 15) | (chksum >> 1)) + buf[i];
        }
    }

    for (; i < (continuations+1) * 32; ++i) {
        chksum = ((chksum << 15) | (chksum >> 1)) + buf[i];
    }

    if (chksum != fde->checksum) {
        std::cerr << "bad file entry checksum at offset " << std::hex << entry_offset << std::endl;
        return std::shared_ptr<ExFATFilesystem::BaseEntity>();
    }

    if (fde->attrib & DIR) {
        std::shared_ptr<ExFATFilesystem::DirectoryEntity> de = std::make_shared<ExFATFilesystem::DirectoryEntity>(buf, continuations + 1);
        _offset_to_entity_mapping[buf] = de;
        return de;
    } else {
        std::shared_ptr<ExFATFilesystem::FileEntity> fe = std::make_shared<ExFATFilesystem::FileEntity>(buf, continuations + 1);
        _offset_to_entity_mapping[buf] = fe;
        return fe;
    }
}

ExFATFilesystem::BaseEntity::BaseEntity(void *entry_start, int num_entries)
{
    _fs_entries = (struct fs_entry *)entry_start;
    _num_entries = num_entries;
}

std::string ExFATFilesystem::BaseEntity::get_name() const
{
    return std::string();
}

void ExFATFilesystem::restore_all_files(const std::string &restore_dir_name, const std::string &textlogfilename)
{
    RecoveryLogTextReader reader(textlogfilename);

    reader.parseTextLog(*this, [this, &restore_dir_name](size_t offset, std::variant<std::string, std::exception, bool> entry_info){
        if (std::holds_alternative<std::string>(entry_info)) {
            // File entry
            const std::string filename = std::get<std::string>(entry_info);
            std::shared_ptr<ExFATFilesystem::FileEntity> ent = std::dynamic_pointer_cast<ExFATFilesystem::FileEntity>(loadEntity(offset));
                //std::shared_ptr<ExFATFilesystem::FileEntity>(dynamic_cast<ExFATFilesystem::FileEntity>(loadEntity(offset)));
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

// todo split out the binlog logic
/**
 * binlog format:
 * 64-bit unsigned int, disk offset of byte in record or sector
 * 32-bit int, number of bytes in record, OR -1 if bad sector
 * variable bytes equal to number of bytes in record
 */
void io::github::paulyc::ExFATRestore::ExFATFilesystem::textLogToBinLog(
    const std::string &textlogfilename,
    const std::string &binlogfilename)
{
    //std::ofstream binlog(binlogfilename, std::ios::binary | std::ios::trunc);
    io::github::paulyc::ExFATRestore::RecoveryLogTextReader reader(textlogfilename);
    io::github::paulyc::ExFATRestore::RecoveryLogBinaryWriter writer(binlogfilename);

#if 0
    BP_ASSERT(false, "Don't call this");
#else
    reader.parseTextLog(*this, [this, &writer](size_t offset, std::variant<std::string, std::exception, bool> entry_info) {
        if (std::holds_alternative<std::string>(entry_info)) {
            // File entry
            std::shared_ptr<ExFATFilesystem::BaseEntity> entry = loadEntity(offset);
            if (entry) {
                const int entries_size_bytes = entry->get_file_info_size();
                writer.writeToBinLog((const char *)&offset, sizeof(size_t));
                writer.writeToBinLog((const char *)&entries_size_bytes, sizeof(int32_t));
                writer.writeToBinLog((const char*)(_mmap + offset), entries_size_bytes);
            } else {
                std::cerr << "failed to loadEntity at " << std::hex << offset << std::endl;
            }
        } else if (std::holds_alternative<bool>(entry_info)) {
            // Bad sector
            writer.writeToBinLog((const char *)&offset, sizeof(size_t));
            writer.writeToBinLog((const char *)&io::github::paulyc::ExFATRestore::RecoveryLogBase::BadSectorFlag, sizeof(int32_t));
        } else {
            // Exception
            std::exception &ex = std::get<std::exception>(entry_info);
            std::cerr << "entry_info had " << typeid(ex).name() << " with message: " << ex.what() << std::endl;
        }
    });
#endif

#if 0
    std::regex fde("FDE ([0-9a-fA-F]{16})(?: (.*))?");
    std::regex badsector("BAD_SECTOR ([0-9a-fA-F]{16})");

    for (std::string line; std::getline(textlog, line); ) {
        std::smatch sm;
        if (std::regex_match(line, sm, fde)) {
            size_t offset;
            std::string filename;
            filename = sm[2];
            try {
                offset = std::stol(sm[1], nullptr, 16);
                _writeBinlogFileEntity(dev, offset, binlog);
            } catch (std::exception &ex) {
                std::cerr << "Writing file entry to binlog, got exception: " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                std::cerr << "Invalid textlog FDE line, skipping: " << line << std::endl;
            }
        } else if (std::regex_match(line, sm, badsector)) {
            int32_t bad_sector_flag;
            size_t offset;
            try {
                offset = std::stol(sm[1], nullptr, 16);
                binlog.write((const char *)&offset, sizeof(size_t));
                binlog.write((const char *)&BadSectorFlag, sizeof(int32_t));
            } catch (std::exception &ex) {
                std::cerr << "Writing bad sector to binlog, got exception " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                std::cerr << "Invalid textlog BAD_SECTOR line, skipping: " << line << std::endl;
            }
        } else {
            std::cerr << "Unknown textlog line format: " << line << std::endl;
        }
    }
#endif
}
