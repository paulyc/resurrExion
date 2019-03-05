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

#include "recoverylog.hpp"

namespace io {
namespace github {
namespace paulyc {
namespace ExFATRestore {

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::ExFATFilesystem(const char *devname, size_t devsize, size_t partition_first_sector) :
    _fd(-1),
    _mmap((uint8_t*)MAP_FAILED),
    _devsize()
{
    _fd = open(devname, O_RDONLY);
    if (_fd == -1) {
        throw posix_exception(errno);
    }

    _mmap = (uint8_t*)mmap(0, _devsize, PROT_READ, MAP_PRIVATE, _fd, 0);
    if (_mmap == (uint8_t*)MAP_FAILED) {
        throw posix_exception(errno);
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::~ExFATFilesystem()
{
    if (_mmap != MAP_FAILED) {
        munmap(_mmap, _devsize);
    }

    if (_fd != -1) {
        close(_fd);
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
std::shared_ptr<typename ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::BaseEntity>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::loadEntity(size_t entry_offset)
{
    uint8_t *buf = _mmap + entry_offset;
    struct fs_file_directory_entry *fde = (struct fs_file_directory_entry*)(buf);
    struct fs_stream_extension_entry *streamext = (struct fs_stream_extension_entry*)(buf+32);
    struct fs_file_name_entry *n = (struct fs_file_name_entry*)(buf+64);

    if (fde->type != FILE_ENTRY || streamext->type != STREAM_EXTENSION || n->type != FILE_NAME) {
        std::cerr << "bad file entry types at offset " << std::hex << entry_offset << std::endl;
        return nullptr;
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

    std::basic_string<char16_t> u16s;
    std::string utf8_name;
    int namelen = streamext->name_length;
    for (int c = 0; c <= continuations - 2; ++c) {
        if (n[c].type == FILE_NAME) {
            for (int i = 0; i < sizeof(n[c].name); ++i) {
                if (u16s.length() == namelen) {
                    break;
                } else {
                    u16s.push_back(n[c].name[i]);
                }
            }
        }
    }
    if (u16s.length() != namelen) {
        std::cerr << "Warning: u16s.length() != namelen for entity at offset " << std::hex << entry_offset << std::endl;
    }
    utf8_name = _cvt.to_bytes(u16s);

    // TODO load parents and children here.....
    if (fde->attributes & DIRECTORY) {
        std::shared_ptr<DirectoryEntity> de = std::make_shared<DirectoryEntity>(buf, continuations + 1, nullptr, utf8_name);
        _offset_to_entity_mapping[buf] = de;
        return de;
    } else {
        std::shared_ptr<FileEntity> fe = std::make_shared<FileEntity>(buf, continuations + 1, nullptr, utf8_name);
        _offset_to_entity_mapping[buf] = fe;
        return fe;
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::BaseEntity::BaseEntity(void *entry_start, int num_entries, std::shared_ptr<BaseEntity> parent, const std::string &name) :
    _fs_entries((struct fs_entry *)entry_start),
    _num_entries(num_entries),
    _parent(parent),
    _name(name)
{
}

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::DirectoryEntity::DirectoryEntity(
    void *entry_start,
    int num_entries,
    std::shared_ptr<BaseEntity> parent,
    const std::string &name) :
    BaseEntity(entry_start, num_entries, parent, name)
{

}

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::restore_all_files(const std::string &restore_dir_name, const std::string &textlogfilename)
{
    RecoveryLogTextReader<ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>> reader(textlogfilename);

    reader.parseTextLog(*this, [this, &restore_dir_name](size_t offset, std::variant<std::string, std::exception, bool> entry_info){
        if (std::holds_alternative<std::string>(entry_info)) {
            // File entry
            const std::string filename = std::get<std::string>(entry_info);
            std::shared_ptr<FileEntity> ent = std::dynamic_pointer_cast<FileEntity>(loadEntity(offset));
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
template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::textLogToBinLog(
    const std::string &textlogfilename,
    const std::string &binlogfilename)
{
    RecoveryLogTextReader reader(textlogfilename);
    RecoveryLogBinaryWriter writer(binlogfilename);

    reader.parseTextLog(*this, [this, &writer](size_t offset, std::variant<std::string, std::exception, bool> entry_info) {
        if (std::holds_alternative<std::string>(entry_info)) {
            // File entry
            std::shared_ptr<BaseEntity> entity = loadEntity(offset);
            if (entity) {
                writer.writeEntityToBinLog(offset, _mmap + offset, entity);
            } else {
                std::cerr << "failed to loadEntity at " << std::hex << offset << std::endl;
            }
        } else if (std::holds_alternative<bool>(entry_info)) {
            // Bad sector
            writer.writeBadSectorToBinLog(offset);
        } else {
            // Exception
            std::exception &ex = std::get<std::exception>(entry_info);
            std::cerr << "entry_info had " << typeid(ex).name() << " with message: " << ex.what() << std::endl;
        }
    });
}

}
}
}
}
