//
//  filesystem.hpp
//  resurrExion
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

#ifndef _github_paulyc_filesystem_hpp_
#define _github_paulyc_filesystem_hpp_

#include <stdint.h>

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
#include "recoverylog.hpp"
#include "logger.hpp"
#include "exfat_structs.hpp"
#include "entity.hpp"

#include <stddef.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef O_RSYNC
#define O_RSYNC O_SYNC
#endif

namespace github {
namespace paulyc {
namespace resurrExion {

class BaseEntity;

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
class ExFATFilesystem : public Loggable
{
public:
    class restore_error : public std::runtime_error {
    public:
        explicit restore_error(const std::string &msg) : std::runtime_error(msg) {}
    };

    ExFATFilesystem(const char *devname, size_t devsize, size_t partition_first_sector, bool write_changes) :
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
    
    virtual ~ExFATFilesystem()
    {
        if (_mmap != MAP_FAILED) {
            munmap(_mmap, _devsize);
        }

        if (_fd != -1) {
            close(_fd);
        }
    }

    std::shared_ptr<BaseEntity> loadEntity(uint8_t *entry_offset, std::shared_ptr<BaseEntity> parent);
    void loadDirectory(std::shared_ptr<DirectoryEntity> de);

    void init_metadata();
    void write_metadata();

    void load_directory_tree(const std::string &textlogfilename);

    constexpr static size_t ClusterHeapDiskStartSector = 0x8C400; // relative to partition start

    // TODO take out this trash
    void restore_all_files(const std::string &restore_dir_name, const std::string &textlogfilename)
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
    void textLogToBinLog(
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

    void scanWriteToLog() {
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

private:
    static constexpr size_t ClustersInFat = (NumSectors - 0x283D8) / 512;

    int      _fd;
    uint8_t *_mmap;
    size_t   _devsize;
    bool     _write_changes;
    uint8_t *_partition_start;
    uint8_t *_partition_end;

    // not part of actual partition, to be copied over later after being initialized
    exfat::boot_region_t<SectorSize> _boot_region;
    exfat::file_allocation_table_t<SectorSize, SectorsPerCluster, ClustersInFat> _fat;
    exfat::allocation_bitmap_table_t<ClustersInFat> _allocation_bitmap;
    exfat::upcase_table_t<SectorSize, 256> _upcase_table;
    exfat::root_directory_t<SectorSize, SectorsPerCluster> _root_directory;

    // pointer to the start of the actual mmap()ed partition
    exfat::filesystem_t<SectorSize, SectorsPerCluster, NumSectors> *_fs; // actual mmaped filesystem

    std::unordered_map<exfat::metadata_entry_u*, std::shared_ptr<BaseEntity>> _offset_to_entity_mapping;
    std::unique_ptr<RootDirectoryEntity> _root_directory_entity;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> _cvt;

    std::vector<char16_t> _invalid_file_name_characters;

    std::string _get_utf8_filename(exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde)
    {
        const int continuations = fde->continuations;
        const int namelen = sde->name_length;
        std::basic_string<char16_t> u16s;
        for (int c = 2; c <= continuations; ++c) {
            exfat::file_name_entry_t *n = (exfat::file_name_entry_t *)(((uint8_t*)fde) + c*32);
            if (n->type == exfat::FILE_NAME) {
                for (int i = 0; i < sizeof(n->name); ++i) {
                    if (u16s.length() == namelen) {
                        return _cvt.to_bytes(u16s);
                    } else {
                        u16s.push_back((char16_t)n->name[i]);
                    }
                }
            }
        }
        return _cvt.to_bytes(u16s);
    }
};

} /* namespace resurrExion */
} /* namespace paulyc */
} /* namespace github */

#include "filesystem.cpp"

#endif /* _github_paulyc_filesystem_hpp_ */
