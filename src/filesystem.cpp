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

#include "filesystem.hpp"
#include "recoverylog.hpp"

#include <stddef.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace io {
namespace github {
namespace paulyc {
namespace ExFATRestore {

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::ExFATFilesystem(const char *devname, size_t devsize, size_t partition_first_sector, bool write_changes) :
    _fd(-1),
    _mmap((uint8_t*)MAP_FAILED),
    _devsize(devsize),
    _write_changes(write_changes)
{
    const int oflags = write_changes ? O_RDWR | O_DSYNC | O_RSYNC : O_RDONLY;
    _fd = open(devname, oflags);
    if (_fd == -1) {
        throw posix_exception(errno);
    }

    const int mprot = write_changes ? PROT_READ | PROT_WRITE : PROT_READ;
    const int mflags = write_changes ? MAP_SHARED : MAP_PRIVATE;
    _mmap = (uint8_t*)mmap(0, _devsize, mprot, mflags, _fd, 0);
    if (_mmap == (uint8_t*)MAP_FAILED) {
        throw posix_exception(errno);
    }

    _partition_start = _mmap + partition_first_sector * SectorSize;
    _partition_end = _partition_start + (NumSectors + 1) * SectorSize;
    _fs = (fs_filesystem<SectorSize, SectorsPerCluster, NumSectors>*)_partition_start;

    _invalid_file_name_characters = {'"', '*', '/', ':', '<', '>', '?', '\\', '|'};
    for (char16_t ch = 0; ch <= 0x001F; ++ch) {
        _invalid_file_name_characters.push_back(ch);
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
void
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::loadDirectory(std::shared_ptr<DirectoryEntity> de)
{
    const uint32_t start_cluster = de->get_start_cluster();
    fs_entry *dirent;
    if (start_cluster == 0) {
        // directory children immediately follow
        dirent = de->get_entity_start() + de->get_num_entries();
    } else {
        dirent = (fs_entry*)_fs->cluster_heap.storage[start_cluster].sectors[0].data;
    }

    const uint8_t num_secondary_entries = ((fs_primary_directory_entry*)dirent)->secondary_count;
    for (uint8_t secondary_entry = 0; secondary_entry < num_secondary_entries; ++secondary_entry) {
        std::shared_ptr<BaseEntity> child = this->loadEntity((uint8_t*)dirent, de);
        de->add_child(child);
        dirent += child->get_num_entries();
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
std::shared_ptr<BaseEntity>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::loadEntity(uint8_t *entity_start, std::shared_ptr<BaseEntity> parent)
{
    struct fs_file_directory_entry *fde = (struct fs_file_directory_entry*)(entity_start);
    struct fs_stream_extension_entry *streamext = (struct fs_stream_extension_entry*)(entity_start+32);
    struct fs_file_name_entry *n = (struct fs_file_name_entry*)(entity_start+64);

    if (fde->type != FILE_DIR_ENTRY || streamext->type != STREAM_EXTENSION || n->type != FILE_NAME) {
        logf(WARN, "bad number of continuations at offset %016llx\n", entity_start - _mmap);
        return nullptr;
    }

    const int continuations = fde->continuations;
    if (continuations < 2 || continuations > 18) {
        logf(WARN, "bad number of continuations at offset %016llx\n", entity_start - _mmap);
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
        logf(WARN, "bad file entry checksum at offset %016llx\n", entity_start - _mmap);
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
        logf(WARN, "u16s.length() != namelen for entity at offset %016llx\n", entity_start - _mmap);
    }
    utf8_name = _cvt.to_bytes(u16s);

    // load parents and children here.....
    // orphan entities we will just stick in the root eventually ....
    //std::shared_ptr<BaseEntity> entity;
    // first see if we already loaded this entity
    std::unordered_map<fs_entry*, std::shared_ptr<BaseEntity>>::iterator entityit =
        _offset_to_entity_mapping.find((fs_entry*)entity_start);
    if (entityit != _offset_to_entity_mapping.end()) {
        if (entityit->second->get_parent() == nullptr) {
            if (parent != nullptr) {
				entityit->second->set_parent(parent);
            }
        } else {
            if (parent != nullptr) {
                logf(WARN, "entity at offset %016llx already has a parent\n", entity_start - _mmap);
            }
        }
        return entityit->second;
    }

    if (fde->attributes & DIRECTORY) {
        std::shared_ptr<DirectoryEntity> de =
            std::make_shared<DirectoryEntity>(entity_start, continuations + 1, parent, utf8_name);
        _offset_to_entity_mapping[de->get_entity_start()] = de;
        this->loadDirectory(de);
        return de;
    } else {
        std::shared_ptr<FileEntity> fe =
            std::make_shared<FileEntity>(entity_start, continuations + 1, parent, utf8_name);
        _offset_to_entity_mapping[fe->get_entity_start()] = fe;
        return fe;
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::init_metadata()
{
    // pretty sure this is 0 because it's the first sector in the partition, not the whole disk
    // but i should verify
    _boot_region.vbr.partition_offset_sectors = 0;
    _boot_region.vbr.volume_length_sectors = NumSectors;
    _boot_region.vbr.fat_offset_sectors =
        2 * sizeof(fs_boot_region<SectorSize>) / SectorSize;
    _boot_region.vbr.fat_length_sectors =
        sizeof(fs_file_allocation_table<SectorSize, SectorsPerCluster, NumSectors>) / SectorSize;
    _boot_region.vbr.cluster_heap_offset_sectors =
        (sizeof(fs_boot_region<SectorSize>) +
         sizeof(fs_root_directory<SectorSize, SectorsPerCluster>)) / SectorSize;
    _boot_region.vbr.cluster_count = NumSectors / SectorsPerCluster;
    _boot_region.vbr.root_directory_cluster = 0; // ??
       // sizeof(fs_volume_metadata <SectorSize, SectorsPerCluster, NumSectors>) / (SectorSize * ClustersPerSector);// ???
    _boot_region.vbr.volume_serial_number = 0; // ???
    _boot_region.vbr.volume_flags = 0; // ??

    // calculate log base 2 of sector size and sectors per cluster
    size_t i = SectorSize;
    _boot_region.vbr.bytes_per_sector = 0;
    while (i >>= 1) { ++_boot_region.vbr.bytes_per_sector; }

    _boot_region.vbr.sectors_per_cluster = 0;
    i = SectorsPerCluster;
    while (i >>= 1) { ++_boot_region.vbr.sectors_per_cluster; }

    _boot_region.vbr.drive_select = 0; // ??
    _boot_region.vbr.percent_used = 100;
    _boot_region.checksum.calculate_checksum((uint8_t*)&_boot_region.vbr, sizeof(_boot_region.vbr));

    std::basic_string<char16_t> volume_label_utf16 = _cvt.from_bytes("Elements");
    memcpy(_root_directory.metadata.label_entry.volume_label, volume_label_utf16.data(), sizeof(char16_t) * volume_label_utf16.length());
    _root_directory.metadata.label_entry.character_count = volume_label_utf16.length(); // dont count null
    // some random number I made up
    const uint8_t guid[] = {
        0x16, 0x06, 0x7e, 0xa1, 0x85, 0x3d, 0xf9, 0x25,
        0x93, 0xda, 0x7d, 0x5c, 0xe9, 0xf1, 0xb9, 0x9d
    };
    memcpy(_root_directory.metadata.guid_entry.volume_guid, guid, sizeof(guid));


    // create upcase table

    // create allocation bitmap

}

// copy metadata and root directory into fs mmap
template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::write_metadata()
{
    memcpy(&_fs->main_boot_region, &_boot_region, sizeof(_boot_region));
    memcpy(&_fs->backup_boot_region, &_boot_region, sizeof(_boot_region));

    // iterate over all entities, fill root directory with every entity not having a parent
    // mark every sector allocated in the FAT and allocation table to account for unknown
    // fragmented files
}

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::load_directory_tree(const std::string &textlogfilename)
{
    // load everything in the log, and put anything without a parent into the root directory
    RecoveryLogTextReader<ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>> reader(textlogfilename);

    reader.parseTextLog(*this, [this](size_t offset, std::variant<std::string, std::exception, bool> entry_info) {
        if (std::holds_alternative<std::string>(entry_info)) {
            // File entry
            const std::string filename = std::get<std::string>(entry_info);
            std::shared_ptr<FileEntity> ent = std::dynamic_pointer_cast<FileEntity>(loadEntity(offset));
            if (!ent) {
                logf(WARN, "Invalid file entry at offset %016llx\n", offset);
                return;
            }
        } else if (std::holds_alternative<bool>(entry_info)) {
            // Bad sector, mark it in the fat
            _fat.entries[offset] = BAD_CLUSTER;
        } else {
            // Exception
            std::exception &ex = std::get<std::exception>(entry_info);
            logf(WARN, "entry_info had exception type %s with message %s\n", typeid(ex).name(), ex.what());
        }
    });

    // find everything without a parent
    for (const auto & [ entry_ptr, entity ] : _offset_to_entity_mapping) {
        if (entity->get_parent() == nullptr) {
            entity->set_parent(_root_directory);
            _root_directory->add_child(entity);
        }
    }
}

constexpr static size_t ClusterHeapDiskStartSector = 0x8C400; // relative to partition start

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
                logf(WARN, "Invalid file entry at offset %016llx\n", offset);
                return;
            }
            if (ent->is_contiguous()) {
                std::ofstream restored_file(restore_dir_name + std::string("/") + filename, std::ios_base::binary);
                const uint32_t start_cluster = ent->get_start_cluster();
                const size_t file_offset = start_cluster * sizeof(fs_cluster<SectorSize, SectorsPerCluster>) + ClusterHeapDiskStartSector * SectorSize;
                const size_t file_size = ent->get_size();
                logf(INFO, "recovering file %s at cluster offset %08lx disk offset %016llx size %d\n",
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
