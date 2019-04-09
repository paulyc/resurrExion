//
//  exfat_structs.hpp - ExFAT filesysytem structures
//  ExFATRestore
//
//  Created by Paul Ciarlo on 9 March 2019.
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

#ifndef _io_github_paulyc_exfat_structs_hpp_
#define _io_github_paulyc_exfat_structs_hpp_

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <algorithm>

enum fs_entry_flags_t {
    VALID       = 0x80,
    CONTINUED   = 0x40,
    OPTIONAL    = 0x20
};

enum fs_directory_entry_t {
    END_OF_DIRECTORY    = 0x00,
    ALLOCATION_BITMAP   = 0x01 | VALID,                         // 0x81
    UPCASE_TABLE        = 0x02 | VALID,                         // 0x82
    VOLUME_LABEL        = 0x03 | VALID,                         // 0x83
    FILE_DIR_ENTRY      = 0x05 | VALID,                         // 0x85
    VOLUME_GUID         = 0x20 | VALID,                         // 0xA0
    TEXFAT_PADDING      = 0x21 | VALID,                         // 0xA1
    WINDOWS_CE_ACT      = 0x22 | VALID,                         // 0xA2
    STREAM_EXTENSION    = 0x00 | VALID | CONTINUED,             // 0xC0
    FILE_NAME           = 0x01 | VALID | CONTINUED,             // 0xC1
    FILE_TAIL           = 0x00 | VALID | CONTINUED | OPTIONAL,  // 0xE0
};

enum fs_attrib_flags_t {
    READ_ONLY   = 1<<0,
    HIDDEN      = 1<<1,
    SYSTEM      = 1<<2,
    VOLUME      = 1<<3,
    DIRECTORY   = 1<<4,
    ARCH        = 1<<5
};

struct fs_bios_parameter_block {
} __attribute__((packed));

struct fs_entry {
    uint8_t type;
    uint8_t data[31];
} __attribute__((packed));

template <size_t SectorSize>
struct fs_sector
{
    uint8_t data[SectorSize];
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster>
struct fs_cluster
{
    fs_sector<SectorSize> sectors[SectorsPerCluster];
} __attribute__((packed));

enum fs_volume_flags_t {
    SECOND_FAT_ACTIVE   = 1<<0,
    VOLUME_DIRTY        = 1<<1,
    MEDIA_FAILURE       = 1<<2,
    CLEAR_TO_ZERO       = 1<<3
};

template <size_t SectorSize>
struct fs_volume_boot_record {
    uint8_t  jump_boot[3]               = {0x90, 0x76, 0xEB};   // 0xEB7690 little-endian
    uint8_t  fs_name[8]                 = {'E', 'X', 'F', 'A', 'T', '\x20', '\x20', '\x20'};
    uint8_t  zero[53]                   = {0};
    uint64_t partition_offset_sectors;          // Sector address of partition
    // (TODO is this the partition or the disk length? I think partition)
    uint64_t volume_length_sectors;             // Size of total volume in sectors
    uint32_t fat_offset_sectors;                // Sector address of first FAT (24?)
    uint32_t fat_length_sectors;                // Size of FAT in sectors
    uint32_t cluster_heap_offset_sectors;       // Sector offset of cluster heap
    uint32_t cluster_count;                     // Number of clusters in cluster heap
    uint32_t root_directory_cluster;            // Cluster address of root directory
    uint32_t volume_serial_number;
    uint16_t fs_revision                = 0x0100;
    uint16_t volume_flags;                      // Combination of fs_volume_flags_t
    uint8_t  bytes_per_sector;                  // Power of two
    uint8_t  sectors_per_cluster;               // Power of two
    // Cluster size must be in range 512-4096 so bytes_per_sector + sectors_per_cluster <= 25
    uint8_t  num_fats                   = 1;    // 1 or 2. 2 onlyfor TexFAT (not supported)
    uint8_t  drive_select               = 0x80; // Extended INT 13h drive number
    uint8_t  percent_used;                      // Percentage of heap in use
    uint8_t  reserved[7]                = {0};
    uint8_t  boot_code[390]             = {0};
    uint16_t boot_signature             = 0xAA55;
    uint8_t  padding[SectorSize - 512] ;        // Padded out to sector size
} __attribute__((packed));

// for a 512-byte sector. should be same size as a sector
template <size_t SectorSize>
struct fs_extended_boot_structure {
    uint8_t  extended_boot_code[SectorSize - 4] = {0};
    uint32_t extended_boot_signature = 0xAA550000;
} __attribute__((packed));

template <size_t SectorSize>
struct fs_main_extended_boot_region {
    fs_extended_boot_structure<SectorSize> ebs[8];
} __attribute__((packed));

// First 16 bytes of each field is a GUID and remaining 32 bytes are the parameters (undefined)
template <size_t SectorSize>
struct fs_oem_parameters {
    uint8_t parameters0[48]             = {0};
    uint8_t parameters1[48]             = {0};
    uint8_t parameters2[48]             = {0};
    uint8_t parameters3[48]             = {0};
    uint8_t parameters4[48]             = {0};
    uint8_t parameters5[48]             = {0};
    uint8_t parameters6[48]             = {0};
    uint8_t parameters7[48]             = {0};
    uint8_t parameters8[48]             = {0};
    uint8_t parameters9[48]             = {0};
    uint8_t reserved[SectorSize - 480]  = {0}; // 32-3616 bytes padded out to sector size
} __attribute__((packed));

// one example of a parameter that would go in an OEM parameter but it's not used
struct fs_flash_parameters {
    uint8_t  OemParameterType[8];
    uint32_t EraseBlockSize;
    uint32_t PageSize;
    uint32_t NumberOfSpareBlocks;
    uint32_t tRandomAccess;
    uint32_t tProgram;
    uint32_t tReadCycle;
    uint32_t tWriteCycle;
    uint8_t  Reserved[4];
} __attribute__((packed));

struct fs_timestamp {
    uint8_t double_seconds[5];
    uint8_t minute[6];
    uint8_t hour[5];
    uint8_t month[4];
    uint8_t year[7];
} __attribute__((packed));

struct fs_file_attributes {
    uint8_t read_only;      // 1 = read only
    uint8_t hidden;         // 1 = hidden
    uint8_t system;         // 1 = system
    uint8_t reserved0       = 0;
    uint8_t directory;      // 0 = file 1 = directory
    uint8_t archive;
    uint8_t reserved1[10]   = {0};
} __attribute__((packed));

struct fs_file_directory_entry {
    uint8_t  type               = FILE_DIR_ENTRY;   // FILE_DIR_ENTRY = 0x85
    uint8_t  continuations;
    uint16_t checksum;
    uint16_t attributes;
    uint8_t  reserved0[2]       = {0};
    uint16_t created_time;
    uint16_t created_date;
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t accessed_time;
    uint16_t accessed_date;
    uint8_t  created_time_cs;
    uint8_t  modified_time_cs;
    uint8_t  accessed_time_cs;
    uint8_t  reserved1[9]       = {0};
} __attribute__((packed));

enum fs_file_flags_t {
    ALLOC_POSSIBLE  = 1<<0, // if 0, first cluster and data length will be undefined in directory entry
    CONTIGUOUS      = 1<<1
};

struct fs_primary_directory_entry {
    uint8_t  type;              // one of fs_directory_entry_t
    uint8_t  secondary_count;   // 0 - 255, number of children in directory
    uint16_t set_checksum;      // checksum of directory entries in this set, excluding this field
    uint8_t  primary_flags;     // combination of fs_file_flags_t
    uint8_t  reserved[14] = {0};
    uint32_t first_cluster;
    uint64_t data_length;
} __attribute__((packed));

struct fs_secondary_directory_entry {
    uint8_t  type;              // one of fs_directory_entry_t
    uint8_t  secondary_flags;   // combination of fs_file_flags_t
    uint8_t  reserved[18]   = {0};
    uint32_t first_cluster;
    uint64_t data_length;
} __attribute__((packed));

struct fs_stream_extension_entry {
	uint8_t type            = STREAM_EXTENSION;
    uint8_t flags;          // Combination of fs_file_flags_t
    uint8_t reserved0       = 0;
    uint8_t name_length;
    uint16_t name_hash;
    uint16_t reserved1      = 0;
    uint64_t valid_size;
    uint32_t reserved2      = 0;
    uint32_t first_cluster;
    uint64_t size;
} __attribute__((packed));

constexpr int FS_FILE_NAME_ENTRY_SIZE = 15;

struct fs_file_name_entry {
    uint8_t type        = FILE_NAME;
    uint8_t reserved    = 0;
    int16_t name[FS_FILE_NAME_ENTRY_SIZE];
} __attribute__((packed));

struct fs_allocation_bitmap_entry {
    uint8_t type            = ALLOCATION_BITMAP;
    uint8_t bitmap_flags    = 0; // 0 if first allocation bitmap, 1 if second (TexFAT only)
    uint8_t reserved[18]    = {0};
    uint32_t first_cluster; // First data cluster number
    uint64_t data_length;   // Size of allocation bitmap in bytes. Ceil(ClusterCount / 8)
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
struct fs_allocation_bitmap_table
{
    static constexpr size_t NumClusters = NumSectors / SectorsPerCluster;
    static constexpr size_t BitmapSize = (NumClusters / 8) + ((NumClusters % 8) == 0 ? 0 : 1);
    static constexpr size_t PaddingSize = SectorSize - (BitmapSize % SectorSize);

    uint8_t bitmap[BitmapSize];
    uint8_t padding[PaddingSize];
} __attribute__((packed));

enum fs_volume_guid_flags {
    ALLOCATION_POSSIBLE = 1<<0, // must be 0
    NO_FAT_CHAIN        = 1<<1  // must be 0
};

struct fs_volume_guid_entry {
    uint8_t  type               = VOLUME_GUID; // 0xA0
    uint8_t  secondary_count    = 0;
    uint16_t set_checksum;
    uint16_t flags              = 0;    // combination of fs_volume_guid_flags, must be 0
    uint8_t  volume_guid[16];           // must not be null
    uint8_t  reserved[10]       = {0};
} __attribute__((packed));

struct fs_volume_label_entry {
    fs_volume_label_entry() {}
    fs_volume_label_entry(std::basic_string<char16_t> volume_label_utf16) { set_label(volume_label_utf16); }

    void set_label(std::basic_string<char16_t> volume_label_utf16) {
        character_count = std::min(sizeof(volume_label), volume_label_utf16.length());
        memcpy(volume_label, volume_label_utf16.data(), sizeof(char16_t) * character_count);
    }

    uint8_t type                = VOLUME_LABEL; // 0x83 if volume label exists or 0x03 if it was deleted
    uint8_t character_count     = 0;            // characters in label
    uint16_t volume_label[11]   = {0};
    uint8_t reserved[8]         = {0};
} __attribute__((packed));

struct fs_upcase_table_entry {
    void calc_checksum(const uint8_t *data, size_t bytes) {
        checksum = 0;
        for (size_t i = 0; i < bytes; i++) {
            checksum = (checksum << 31) | (checksum >> 1) + data[i];
        }
    }
    uint8_t  type           = UPCASE_TABLE; // 0x82
    uint8_t  reserved0[3]   = {0};
    uint32_t checksum;
    uint8_t  reserved1[12]  = {0};
    uint32_t first_cluster;
    uint64_t data_length;
} __attribute__((packed));

template <int SectorSize, int NumEntries>
struct fs_upcase_table {
    fs_upcase_table() {
        unsigned i;
        for (i = 0; i < 0x61; ++i) {
            entries[i] = i;
        }
        for (; i <= 0x7B; ++i) {
            // a-z => A=>z (0x61-0x7a => 0x41-0x5a, clear 0x20 bit)
            entries[i] = i ^ 0x20; // US-ASCII letters
        }
        for (; i < 0xE0; ++i) {
            entries[i] = i;
        }
        for (; i < 0xFF; ++i) {
            if (i == 0xD7 || i == 0xF7) { // multiplication and division signs
                entries[i] = i;
            } else {
                entries[i] = i ^ 0x20; // ISO-8859-1 letters with diacritics
            }
        }
    }
    char16_t entries[NumEntries];
} __attribute__((packed));

template <size_t SectorSize>
struct fs_reserved_sector {
    uint8_t reserved[SectorSize] = {0};
} __attribute__((packed));

template <size_t SectorSize>
struct fs_checksum_sector {
    uint32_t checksum[SectorSize / 4];

    void calculate_checksum(uint8_t *vbr, size_t vbr_size)
    {
        uint32_t chksum = 0;
        for (size_t i = 0; i < vbr_size; ++i) {
            if (i != 106 && i != 107 && i != 112) {
                chksum = ((chksum << 31) | (chksum >> 1)) + vbr[i];
            }
        }
        for (size_t i = 0; i < sizeof(checksum); ++i) {
            checksum[i] = chksum;
        }
    }
} __attribute__((packed));

template <size_t SectorSize>
struct fs_boot_region {
    fs_volume_boot_record<SectorSize>        vbr;            // Sector 0
    fs_main_extended_boot_region<SectorSize> mebs;           // Sector 1-8
    fs_oem_parameters<SectorSize>            oem_params;     // Sector 9
    fs_reserved_sector<SectorSize>           reserved;       // Sector 10
    fs_checksum_sector<SectorSize>           checksum;       // Sector 11
} __attribute__((packed));

enum fs_fat_entry_t {
    BAD_CLUSTER                 = 0xFFFFFFF7,
    MEDIA_DESCRIPTOR_HARD_DRIVE = 0xFFFFFFF8,
    END_OF_FILE                 = 0xFFFFFFFF
};

template <size_t SectorSize, size_t SectorsPerCluster, size_t ClustersInFat>
struct fs_file_allocation_table
{
    static constexpr size_t FatSize = (ClustersInFat + 2) * sizeof(uint32_t);
    static constexpr size_t PaddingSize = SectorSize - (FatSize % SectorSize);

    uint32_t media_type             = MEDIA_DESCRIPTOR_HARD_DRIVE;
    uint32_t reserved               = END_OF_FILE;   // Must be 0xFFFFFFFF
    uint32_t entries[ClustersInFat] = {END_OF_FILE}; // ??
    uint8_t  padding[PaddingSize]   = {0}; // pad to sector
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster>
struct fs_root_directory
{
    static constexpr size_t ClusterSize = SectorSize * SectorsPerCluster;

    fs_volume_label_entry        label_entry;
    fs_allocation_bitmap_entry   bitmap_entry;
    fs_upcase_table_entry        upcase_entry;
    fs_volume_guid_entry         guid_entry;
    fs_file_directory_entry      directory_entry;
    fs_stream_extension_entry    ext_entry;
    fs_file_name_entry           name_entry;
    fs_secondary_directory_entry directory_entries[0]; // dynamically sized based on number of child entities
} __attribute__((packed));

struct fs_directory
{
    void calc_entry_set_checksum(uint8_t secondary_count) {
        primary_entry.secondary_count = secondary_count;
        uint16_t &chksum = primary_entry.set_checksum = 0;
        const uint8_t *data = (const uint8_t*)&primary_entry;
        for (int i = 0; i < sizeof(struct fs_primary_directory_entry); ++i) {
            if (i != 2 && i != 3) {
                chksum = ((chksum << 31) | (chksum >> 1)) + data[i];
            }
        }
        for (size_t sec_entry = 0; sec_entry < secondary_count; ++sec_entry) {
            data = (const uint8_t*)(secondary_entries + sec_entry);
            for (int i = 0; i < sizeof(struct fs_secondary_directory_entry); ++i) {
                chksum = ((chksum << 31) | (chksum >> 1)) + data[i];
            }
        }
    }
    struct fs_primary_directory_entry primary_entry;
    struct fs_secondary_directory_entry secondary_entries[0];
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster, size_t ClustersInFat>
struct fs_cluster_heap
{
    fs_cluster<SectorSize, SectorsPerCluster> storage[ClustersInFat];
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster, size_t ClustersInFat>
struct fs_fat_region {
    constexpr static size_t cluster_heap_start_sector = 0x283D8;
    constexpr static size_t fat_heap_alignment_sectors = cluster_heap_start_sector -
        (2 * sizeof(fs_boot_region<SectorSize>) + // 24 sectors = 12288 bytes
        sizeof(fs_file_allocation_table<SectorSize, SectorsPerCluster, ClustersInFat>)) / SectorSize;

    fs_file_allocation_table<SectorSize, SectorsPerCluster, ClustersInFat> fat;
    fs_sector<SectorSize> fat_cluster_heap_alignment[fat_heap_alignment_sectors];
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors, size_t ClustersInFat>
struct fs_data_region {
    constexpr static size_t ExcessSectors = NumSectors - ClustersInFat * SectorsPerCluster;

    fs_cluster_heap<SectorSize, SectorsPerCluster, ClustersInFat> cluster_heap;
    fs_sector<SectorSize> excess_space[ExcessSectors];
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
struct fs_filesystem {
    constexpr static size_t ClustersInFat = (NumSectors - 0x283D8) / 512;
    fs_boot_region<SectorSize>                                                  main_boot_region;
    // copy of main_boot_region
    fs_boot_region<SectorSize>                                                  backup_boot_region;
    fs_fat_region<SectorSize, SectorsPerCluster, ClustersInFat>                 fat_region;
    //root directory is in the cluster heap
    fs_data_region<SectorSize, SectorsPerCluster, NumSectors, ClustersInFat>    data_region;
} __attribute__((packed));

#endif /* _io_github_paulyc_exfat_structs_hpp_ */
