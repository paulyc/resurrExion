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

#include <fstream>
#include <memory>
#include <unordered_map>

enum fs_entry_flags_t {
    VALID 		= 0x80,
    CONTINUED 	= 0x40,
    OPTIONAL 	= 0x20
};

enum fs_entry_type_t {
    BITMAP 		= 0x01 | VALID,
    UPCASE 		= 0x02 | VALID,
    LABEL 		= 0x03 | VALID,
    FILE_INFO1 	= 0x05 | VALID,
    FILE_INFO2 	= 0x00 | VALID | CONTINUED,
    FILE_NAME 	= 0x01 | VALID | CONTINUED,
    FILE_TAIL 	= 0x00 | VALID | CONTINUED | OPTIONAL,
};

enum fs_attrib_flags_t {
	RO 		= 0x01,
	HIDDEN 	= 0x02,
	SYSTEM 	= 0x04,
	VOLUME 	= 0x08,
	DIR    	= 0x10,
	ARCH   	= 0x20
};

enum fs_file_flags_t {
	ONE 		= 0x01,
    CONTIGUOUS 	= 0x02
};

struct fs_entry {
    uint8_t type;
    uint8_t data[31];
} __attribute__((packed));

struct fs_volume_boot_record {
    uint8_t    jump_boot[3] = {0x90, 0x76, 0xEB}; // 0xEB7690 little-endian
    uint8_t     fs_name[8] = {'E', 'X', 'F', 'A', 'T', '\x20', '\x20', '\x20'};
} __attribute__((packed));

struct fs_file_directory_entry {
    uint8_t type;
    uint8_t continuations;
    uint16_t checksum;
    uint16_t attrib;
    uint16_t reserved1;
    uint16_t crtime;
    uint16_t crdate;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t atime;
    uint16_t adate;
    uint8_t crtime_cs;
    uint8_t mtime_cs;
    uint8_t atime_cs;
    uint64_t reserved2;
    uint8_t reserved3;
} __attribute__((packed));

struct fs_stream_extension_entry {
	uint8_t type;
    uint8_t flags;
    uint8_t reserved1;
    uint8_t name_length;
    uint16_t name_hash;
    uint16_t reserved2;
    uint64_t valid_size;
    uint32_t reserved3;
    uint32_t start_cluster;
    uint64_t size;
} __attribute__((packed));

constexpr int exfat_name_entry_size = 15;

struct fs_file_name_entry {
    uint8_t type;
    uint8_t reserved;
    int16_t name[exfat_name_entry_size];
} __attribute__((packed));

namespace io {
namespace github {
namespace paulyc {
namespace ExFATRestore {

class ExFATFilesystem
{
public:
    ExFATFilesystem(const char *devname, size_t devsize);
    virtual ~ExFATFilesystem();

    class BaseEntity
    {
    public:
        BaseEntity(void *entry_start, int num_entries);
        virtual ~BaseEntity() {}

        int get_file_info_size() const { return _num_entries * sizeof(struct fs_entry); }
        uint32_t get_start_cluster() const { return ((struct fs_stream_extension_entry *)(_fs_entries + 1))->start_cluster; }
        uint64_t get_size() const { return ((struct fs_stream_extension_entry *)(_fs_entries + 1))->size; }
        std::string get_name() const;
    protected:
        struct fs_entry *_fs_entries;
        int _num_entries;
        std::shared_ptr<BaseEntity> _parent;
    };

    class FileEntity : public BaseEntity
    {
    public:
        FileEntity(void *entry_start, int num_entries) : BaseEntity(entry_start, num_entries) {}

        bool is_contiguous() const { return ((struct fs_stream_extension_entry *)(_fs_entries + 1))->flags & CONTIGUOUS; }
    };

    class DirectoryEntity : public BaseEntity
    {
    public:
        DirectoryEntity(void *entry_start, int num_entries) : BaseEntity(entry_start, num_entries) {}
    private:
    };

    std::shared_ptr<BaseEntity> loadEntity(size_t entry_offset);

    void restore_all_files(const std::string &restore_dir_name, const std::string &textlogfilename);

    void textLogToBinLog(const std::string &textlogfilename, const std::string &binlogfilename);

private:
    int _fd;
    uint8_t *_mmap;
    size_t _devsize;

    std::unordered_map<uint8_t*, std::shared_ptr<BaseEntity>> _offset_to_entity_mapping;
    //std::list<uint64_t> _bad_sector_list;
    std::shared_ptr<DirectoryEntity> _root_directory;
};

}
}
}
}

#endif /* _io_github_paulyc_filesystem_hpp_ */
