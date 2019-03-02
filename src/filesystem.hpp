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

#ifndef filesystem_hpp
#define filesystem_hpp

#include <stdint.h>
#include <stddef.h>

#include <fstream>

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

    bool is_directory() const { return attrib & DIR; }
} __attribute__((packed));

struct fs_stream_extension_directory_entry {
	uint8_t type;
    uint8_t flags;
    uint8_t reserved1;
    uint8_t name_length;
    uint16_t name_hash;
    uint16_t reserved2;
    uint64_t valid_size;
    uint64_t reserved3;
    uint32_t start_cluster;
    uint64_t size;

    bool is_contiguous() const { return flags & CONTIGUOUS; }
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
    static bool verifyFileEntry(
        uint8_t *buf,
        size_t bufSize,
        int &file_info_size,
        struct fs_file_directory_entry *&m1,
        struct fs_stream_extension_directory_entry *&m2);
};

}
}
}
}

#endif /* filesystem_hpp */
