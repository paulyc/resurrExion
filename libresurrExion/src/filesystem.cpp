//
//  filesystem.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 20 July 2020
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

#include "filesystem.hpp"

namespace github {
namespace paulyc {
namespace resurrExion {

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::ExFATFilesystem(
        const char *devname,
        byteofs_t devsize,
        byteofs_t partition_first_sector,
        bool write_changes
) :
    _fd(-1),
    _mmap((uint8_t*)MAP_FAILED),
    _devsize(devsize),
    _write_changes(write_changes)
{
    const int oflags = write_changes ? O_RDWR | O_DSYNC | O_RSYNC : O_RDONLY;
    _fd = open(devname, oflags);
    if (_fd == -1) {
        throw std::system_error(std::error_code(errno, std::system_category()));
    }

    const int mprot = write_changes ? PROT_READ | PROT_WRITE : PROT_READ;
    const int mflags = write_changes ? MAP_SHARED : MAP_PRIVATE;
    _mmap = (uint8_t*)mmap(0, _devsize, mprot, mflags, _fd, 0);
    if (_mmap == (uint8_t*)MAP_FAILED) {
        throw std::system_error(std::error_code(errno, std::system_category()));
    }

    _partition_start = _mmap + partition_first_sector * SectorSize;
    _partition_end = _partition_start + (NumSectors + 1) * SectorSize;
    _fs = (exfat::filesystem_t<SectorSize, SectorsPerCluster, NumSectors>*)_partition_start;

    _invalid_file_name_characters = {'"', '*', '/', ':', '<', '>', '?', '\\', '|'};
    for (char16_t ch = 0; ch <= 0x001F; ++ch) {
        _invalid_file_name_characters.push_back(ch);
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::~ExFATFilesystem()
{
    if (_mmap != MAP_FAILED) {
        munmap(_mmap, _devsize);
    }

    if (_fd != -1) {
        close(_fd);
    }
}
template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
std::shared_ptr<BaseEntity> ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::loadEntityOffset(byteofs_t entity_offset, std::shared_ptr<BaseEntity> parent) {
    return loadEntity(_mmap + entity_offset, parent);
}

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
std::shared_ptr<BaseEntity> ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::loadEntity(uint8_t *entity_start, std::shared_ptr<BaseEntity> parent)
{
    exfat::file_directory_entry_t *fde = (exfat::file_directory_entry_t*)(entity_start);
    exfat::stream_extension_entry_t *streamext = (exfat::stream_extension_entry_t*)(entity_start+32);
    exfat::file_name_entry_t *n = (exfat::file_name_entry_t*)(entity_start+64);

    if (fde->type != exfat::FILE_DIR_ENTRY || streamext->type != exfat::STREAM_EXTENSION || n->type != exfat::FILE_NAME) {
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
        if (n[c].type == exfat::FILE_NAME) {
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
    std::unordered_map<exfat::metadata_entry_u*, std::shared_ptr<BaseEntity>>::iterator entityit =
        _offset_to_entity_mapping.find((exfat::metadata_entry_u*)entity_start);
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

    if (fde->attribute_flags & exfat::DIRECTORY) {
        std::shared_ptr<DirectoryEntity> de =
            std::make_shared<DirectoryEntity>(entity_start, continuations + 1, parent, utf8_name);
        _offset_to_entity_mapping[de->get_entity_start()] = de;
        this->loadDirectory(de);
        return std::move(de);
    } else {
        std::shared_ptr<FileEntity> fe =
            std::make_shared<FileEntity>(entity_start, continuations + 1, parent, utf8_name);
        _offset_to_entity_mapping[fe->get_entity_start()] = fe;
        return std::move(fe);
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::loadDirectory(std::shared_ptr<DirectoryEntity> de)
{
    const clusterofs_t start_cluster = de->get_start_cluster();
    exfat::metadata_entry_u *dirent;
    if (start_cluster == 0) {
        // directory children immediately follow
        dirent = de->get_entity_start() + de->get_num_entries();
    } else {
        dirent = (exfat::metadata_entry_u *)_fs->data_region.cluster_heap.storage[start_cluster].sectors[0].data;
    }

    const uint8_t num_secondary_entries = ((exfat::primary_directory_entry_t*)dirent)->secondary_count;
    for (uint8_t secondary_entry = 0; secondary_entry < num_secondary_entries; ++secondary_entry) {
        std::shared_ptr<BaseEntity> child = this->loadEntity((uint8_t*)dirent, de);
        de->add_child(child);
        child->set_parent(de);
        dirent += child->get_num_entries();
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::init_metadata()
{
    constexpr clusterofs_t cluster_count = NumSectors / SectorsPerCluster;
    // pretty sure this is 0 because it's the first sector in the partition, not the whole disk
    // but i should verify
    _boot_region.vbr.partition_offset_sectors = 0;
    _boot_region.vbr.volume_length_sectors = NumSectors;
    _boot_region.vbr.fat_offset_sectors =
        2 * sizeof(exfat::boot_region_t<SectorSize>) / SectorSize;
    _boot_region.vbr.fat_length_sectors =
        sizeof(exfat::file_allocation_table_t<SectorSize, SectorsPerCluster, NumSectors>) / SectorSize;
    _boot_region.vbr.cluster_heap_offset_sectors =
        (sizeof(exfat::boot_region_t<SectorSize>) +
         sizeof(exfat::root_directory_t<SectorSize, SectorsPerCluster>)) / SectorSize;
    _boot_region.vbr.cluster_count = cluster_count;
    _boot_region.vbr.root_directory_cluster = 3;
       // sizeof(fs_volume_metadata <SectorSize, SectorsPerCluster, NumSectors>) / (SectorSize * ClustersPerSector);// ???
    _boot_region.vbr.volume_serial_number = 0xDEADBEEF;
    _boot_region.vbr.volume_flags = exfat::VOLUME_DIRTY;

    // calculate log base 2 of sector size and sectors per cluster
    size_t i = SectorSize;
    _boot_region.vbr.log2_bytes_per_sector = 0;
    while (i >>= 1) { ++_boot_region.vbr.log2_bytes_per_sector; }

    _boot_region.vbr.log2_sectors_per_cluster = 0;
    i = SectorsPerCluster;
    while (i >>= 1) { ++_boot_region.vbr.log2_sectors_per_cluster; }

    _boot_region.vbr.percent_used = 100;
    _boot_region.checksum.fill_checksum((uint8_t*)&_boot_region.vbr, sizeof(_boot_region.vbr));

    _root_directory.label_entry.set_label(_cvt.from_bytes("Elements"));

    // create allocation bitmap, just set every cluster allocated so we don't overwrite anything
    // after mounting the filesystem
    _allocation_bitmap.mark_all_alloc();
    _root_directory.bitmap_entry.data_length = sizeof(_allocation_bitmap.bitmap);
    _root_directory.bitmap_entry.first_cluster = 2;
    _root_directory.bitmap_entry = _allocation_bitmap.get_directory_entry();

    _root_directory.upcase_entry = _upcase_table.get_directory_entry();

    _root_directory.upcase_entry.data_length = sizeof(_upcase_table);
    _root_directory.upcase_entry.first_cluster = 3;

    //exfat::fs_volume_guid_entry        guid_entry;
    // some random number I made up
    const uint8_t guid[] = {
        0x16, 0x06, 0x7e, 0xa1, 0x85, 0x3d, 0xf9, 0x25,
        0x93, 0xda, 0x7d, 0x5c, 0xe9, 0xf1, 0xb9, 0x9d
    };
    memcpy(_root_directory.guid_entry.volume_guid, guid, sizeof(guid));
    _root_directory.guid_entry.calc_checksum();
    //_root_directory
//    fs_file_directory_entry     directory_entry;
//    fs_stream_extension_entry   ext_entry;
//    fs_file_name_entry          name_entry;
}

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::write_metadata()
{
    memcpy(&_fs->main_boot_region, &_boot_region, sizeof(_boot_region));
    memcpy(&_fs->backup_boot_region, &_boot_region, sizeof(_boot_region));

    memcpy(&_fs->fat_region, &_fat, sizeof(_fat));

    memcpy(&_fs->data_region, &_root_directory);

    // iterate over all entities, fill root directory with every entity not having a parent
    // mark every sector allocated in the FAT and allocation table to account for unknown
    // fragmented files
}

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::load_directory_tree(const std::string &textlogfilename)
{
    // load everything in the log, and put anything without a parent into the root directory
    typedef RecoveryLog<ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>> log_t;
    typedef typename log_t::Entity_T entity_t;
    log_t reader;

    reader.parseTextLog(textlogfilename, *this, [this](byteofs_t offset, entity_t ent, std::optional<std::exception> except) {
        if (except.has_value()) {
            const std::exception &ex = except.value();
            logf(WARN, "entry_info had exception type %s with message %s\n", typeid(ex).name(), ex.what());
        } else if (ent != nullptr) {
            switch (ent->get_type()) {
            case BaseEntity::File:
            {
                std::shared_ptr<FileEntity> f = std::dynamic_pointer_cast<FileEntity>(ent);
                if (!f) {
                    logf(WARN, "Invalid file entry at offset %016llx\n", offset);
                    return;
                }
                break;
            }
            case BaseEntity::Directory:
            {
                std::shared_ptr<DirectoryEntity> d = std::dynamic_pointer_cast<DirectoryEntity>(ent);
                if (!d) {
                    logf(WARN, "Invalid directory entry at offset %016llx\n", offset);
                    return;
                }
                for (auto &child: d->get_children()) {
                    child->set_parent(ent);
                }
                break;
            }
            default:
                logf(WARN, "Unknown entity type\n");
                break;
            }
        } else {
            // Bad sector, mark it in the fat
            _fat.entries[offset] = exfat::BAD_CLUSTER;
        }
    });

    // find everything without a parent
    for (const auto & [ entry_ptr, entity ] : _offset_to_entity_mapping) {
        if (entity->get_parent() == nullptr) {
            fprintf(stderr, "ORPHAN %016llx %s\n", entry_ptr, entity->get_name());
            entity->set_parent(_root_directory);
            _root_directory->add_child(entity);
        }
    }
}

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::find_orphans(std::function<void(byteofs_t offset, std::shared_ptr<BaseEntity> ent)> fun) {
    for (auto & [ entry_ptr, entity ] : _offset_to_entity_mapping) {
        if (entity->get_parent() == nullptr) {
            fun(((std::streamoff)entry_ptr - (std::streamoff)_fs), entity);
        }
    }
}

constexpr static byteofs_t ClusterHeapDiskStartSector = 0x8C400; // relative to partition start

// TODO take out this trash
template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::restore_all_files(const std::string &restore_dir_name, const std::string &textlogfilename)
{
    RecoveryLog<ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>> reader;

    reader.parseTextLog(textlogfilename, *this, [this, &restore_dir_name](size_t offset, std::variant<std::string, std::exception, bool> entry_info){
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
                const size_t file_offset = start_cluster * sizeof(exfat::cluster_t<SectorSize, SectorsPerCluster>) + ClusterHeapDiskStartSector * SectorSize;
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

// todo take out this trash
// todo split out the binlog logic
/**
 * binlog format:
 * 64-bit unsigned int, disk offset of byte in record or sector
 * 32-bit int, number of bytes in record, OR -1 if bad sector
 * variable bytes equal to number of bytes in record
 */
template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::textLogToBinLog(
    const std::string &textlogfilename,
    const std::string &binlogfilename)
{
    RecoveryLog<ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>> readwriter;

    readwriter.writeBinLog(binlogfilename, [this, &textlogfilename, &readwriter](std::ofstream &binlog) {
        readwriter.parseTextLog(textlogfilename, *this, [this, &binlog, &readwriter](size_t offset, std::variant<std::string, std::exception, bool> entry_info) {
            if (std::holds_alternative<std::string>(entry_info)) {
                // File entry
                std::shared_ptr<BaseEntity> entity = loadEntity(_mmap + offset, nullptr);
                if (entity) {
                    readwriter.writeEntityToBinLog(binlog, offset, _mmap + offset, entity);
                } else {
                    logf(WARN, "failed to loadEntity at offset %016ull\n", offset);
                }
            } else if (std::holds_alternative<bool>(entry_info)) {
                // Bad sector
                readwriter.writeBadSectorToBinLog(binlog, offset);
            } else {
                // Exception
                std::exception &ex = std::get<std::exception>(entry_info);
                logf(WARN, "entry_info had exception type %s with message %s\n", typeid(ex).name(), ex.what());
            }
        });
    });
}

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
void ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>::scanWriteToLog() {
    uint8_t *end = _partition_end - sizeof(exfat::file_directory_entry_t);
    for (uint8_t *peek = _partition_start; peek < end; ++peek) {
        if (*peek == exfat::FILE_DIR_ENTRY) {
            exfat::file_directory_entry_t *fde = (exfat::file_directory_entry_t*)(peek);
            exfat::stream_extension_entry_t *m2 = (exfat::stream_extension_entry_t *)(peek+32);
            if (fde->isValid() && m2->isValid()) {
                if (fde->calc_set_checksum() == fde->checksum) {
                    printf("FDE %016zull\n", peek-1);
                }

            }
        }
    }
}

}
}
}
