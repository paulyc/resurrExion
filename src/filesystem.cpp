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
    _devsize(devsize)
{
    _fd = open(devname, O_RDONLY);
    if (_fd == -1) {
        throw posix_exception(errno);
    }

    _mmap = (uint8_t*)mmap(0, _devsize, PROT_READ, MAP_PRIVATE, _fd, 0);
    if (_mmap == (uint8_t*)MAP_FAILED) {
        throw posix_exception(errno);
    }

    _partition_start = _mmap + partition_first_sector * SectorSize;
    _partition_end = _partition_start + (NumSectors + 1) * SectorSize;
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
std::shared_ptr<BaseEntity>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::loadEntity(uint8_t *entity_start, std::shared_ptr<BaseEntity> parent)
{
    struct fs_file_directory_entry *fde = (struct fs_file_directory_entry*)(entity_start);
    struct fs_stream_extension_entry *streamext = (struct fs_stream_extension_entry*)(entity_start+32);
    struct fs_file_name_entry *n = (struct fs_file_name_entry*)(entity_start+64);

    if (fde->type != FILE_ENTRY || streamext->type != STREAM_EXTENSION || n->type != FILE_NAME) {
        logf(WARN, "bad number of continuations at offset %016xull\n", entity_start - _mmap);
        return nullptr;
    }

    const int continuations = fde->continuations;
    if (continuations < 2 || continuations > 18) {
        logf(WARN, "bad number of continuations at offset %016xull\n", entity_start - _mmap);
        return nullptr;
    }

    int i;
    uint16_t chksum = 0;

    for (i = 0; i < 32; ++i) {
        if (i != 2 && i != 3) {
            chksum = ((chksum << 15) | (chksum >> 1)) + entity_start[i];
        }
    }

    for (; i < (continuations+1) * 32; ++i) {
        chksum = ((chksum << 15) | (chksum >> 1)) + entity_start[i];
    }

    if (chksum != fde->checksum) {
        logf(WARN, "bad file entry checksum at offset %016xull\n", entity_start - _mmap);
        return nullptr;
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
        logf(WARN, "u16s.length() != namelen for entity at offset %016xull\n", entity_start - _mmap);
    }
    utf8_name = _cvt.to_bytes(u16s);

    // load parents and children here.....
    // orphan entities we will just stick in the root eventually ....
    if (fde->attributes & DIRECTORY) {
        std::shared_ptr<DirectoryEntity> de = std::make_shared<DirectoryEntity>(entity_start, continuations + 1, parent, utf8_name);
        _offset_to_entity_mapping[de->get_entity_start()] = de;
        if (parent == nullptr) {
            _orphan_entities.push_back(de);
        }
        uint8_t *child_entry = _fs->cluster_heap.storage[de->get_start_cluster()].sectors[0].data;
        const uint8_t *const end_children = child_entry + de->get_size();
        // Not tail recursive but I'm gonna go head and assume that ExFAT sets a limit on some number of
        // directory levels before we run out of memory, at least until I'm not half-cocked and in a hurry and
        // can figure out a more reasonable solution
        while (child_entry < end_children) {
            std::shared_ptr<BaseEntity> child = loadEntity(child_entry, de);
            child_entry += child->get_file_info_size();
        }
        return de;
    } else {
        std::shared_ptr<FileEntity> fe = std::make_shared<FileEntity>(entity_start, continuations + 1, parent, utf8_name);
        _offset_to_entity_mapping[fe->get_entity_start()] = fe;
        if (parent == nullptr) {
            _orphan_entities.push_back(fe);
        }
        return fe;
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::init_metadata()
{
    // pretty sure this is 0 because it's the first sector in the partition, not the whole disk
    // but i should verify
    _metadata.main_boot_region.vbr.partition_offset_sectors = 0;
    _metadata.main_boot_region.vbr.volume_length_sectors = NumSectors;
    _metadata.main_boot_region.vbr.fat_offset_sectors =
        2 * sizeof(fs_boot_region<SectorSize>) / SectorSize;
    _metadata.main_boot_region.vbr.fat_length_sectors =
        sizeof(fs_file_allocation_table<SectorSize, SectorsPerCluster, NumSectors>) / SectorSize;
    _metadata.main_boot_region.vbr.cluster_heap_offset_sectors =
        (sizeof(fs_volume_metadata <SectorSize, SectorsPerCluster, NumSectors>) + sizeof(fs_root_directory)) / SectorSize;
    _metadata.main_boot_region.vbr.cluster_count = NumSectors / SectorsPerCluster;
    _metadata.main_boot_region.vbr.root_directory_cluster = 0; // ??
       // sizeof(fs_volume_metadata <SectorSize, SectorsPerCluster, NumSectors>) / (SectorSize * ClustersPerSector);// ???
    _metadata.main_boot_region.vbr.volume_serial_number = 0; // ???
    _metadata.main_boot_region.vbr.volume_flags = 0; // ??

    // calculate log base 2 of sector size and sectors per cluster
    size_t i = SectorSize;
    _metadata.main_boot_region.vbr.bytes_per_sector = 0;
    while (i >>= 1) { ++_metadata.main_boot_region.vbr.bytes_per_sector; }

    _metadata.main_boot_region.vbr.sectors_per_cluster = 0;
    i = SectorsPerCluster;
    while (i >>= 1) { ++_metadata.main_boot_region.vbr.sectors_per_cluster; }

    _metadata.main_boot_region.vbr.drive_select = 0; // ??
    _metadata.main_boot_region.vbr.percent_used = 100;
    _metadata.main_boot_region.checksum.calculate_checksum((uint8_t*)&_metadata.main_boot_region.vbr, sizeof(_metadata.main_boot_region.vbr));

    memcpy(&_metadata.backup_boot_region, &_metadata.main_boot_region, sizeof(fs_boot_region<SectorSize>));
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
                logf(WARN, "Invalid file entry at offset %016xull\n", offset);
                return;
            }
            if (ent->is_contiguous()) {
                std::ofstream restored_file(restore_dir_name + std::string("/") + filename, std::ios_base::binary);
                const uint32_t start_cluster = ent->get_start_cluster();
                const size_t file_offset = start_cluster * cluster_size_bytes + cluster_heap_disk_start_sector * sector_size_bytes;
                const size_t file_size = ent->get_size();
                logf(INFO, "recovering file %s at cluster offset %08xu disk offset %016xull size %d\n",
                     filename, start_cluster, file_offset, file_size);
                restored_file.write((const char *)(_mmap + file_offset), file_size);
                restored_file.close();
            } else {
                logf(WARN, "Non-contiguous file: %s\n", filename);
            }
        } else if (std::holds_alternative<bool>(entry_info)) {
            // Bad sector, don't care
        } else {
            // Exception
            std::exception &ex = std::get<std::exception>(entry_info);
            logf(WARN, "entry_info had exception type %s with message %s\n", typeid(ex).name(), ex.what());
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
    RecoveryLogTextReader<ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>> reader(textlogfilename);
    RecoveryLogBinaryWriter<ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>> writer(binlogfilename);

    reader.parseTextLog(*this, [this, &writer](size_t offset, std::variant<std::string, std::exception, bool> entry_info) {
        if (std::holds_alternative<std::string>(entry_info)) {
            // File entry
            std::shared_ptr<BaseEntity> entity = loadEntity(_mmap + offset, nullptr);
            if (entity) {
                writer.writeEntityToBinLog(offset, _mmap + offset, entity);
            } else {
                logf(WARN, "failed to loadEntity at offset %016ull\n", offset);
            }
        } else if (std::holds_alternative<bool>(entry_info)) {
            // Bad sector
            writer.writeBadSectorToBinLog(offset);
        } else {
            // Exception
            std::exception &ex = std::get<std::exception>(entry_info);
            logf(WARN, "entry_info had exception type %s with message %s\n", typeid(ex).name(), ex.what());
        }
    });
}

}
}
}
}
