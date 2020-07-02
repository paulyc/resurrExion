//
//  exfat_structs.hpp - ExFAT filesysytem structures
//  resurrExion
//
//  Created by Paul Ciarlo on 9 March 2019
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

#ifndef _github_paulyc_exfat_structs_hpp_
#define _github_paulyc_exfat_structs_hpp_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <string>

namespace github {
namespace paulyc {
namespace resurrExion {
namespace exfat {

enum AttributeFlags {
    READ_ONLY   = 1<<0,
    HIDDEN      = 1<<1,
    SYSTEM      = 1<<2,
    VOLUME      = 1<<3,
    DIRECTORY   = 1<<4,
    ARCH        = 1<<5
};

enum EntryTypeFlags {
    TYPE_CODE       = 1<<0,
    TYPE_IMPORTANCE = 1<<5,
    TYPE_CATEGORY   = 1<<6,
    IN_USE          = 1<<7
};

struct bios_parameter_block {
} __attribute__((packed));

template <size_t SectorSize>
struct sector_t
{
    uint8_t data[SectorSize];
} __attribute__((packed));
static_assert(sizeof(sector_t<512>) == 512);

template <size_t SectorSize, size_t SectorsPerCluster>
struct cluster_t
{
    sector_t<SectorSize> sectors[SectorsPerCluster];
} __attribute__((packed));
static_assert(sizeof(cluster_t<512, 512>) == 512*512);

enum VolumeFlags {
    NO_FLAGS            = 0,
    SECOND_FAT_ACTIVE   = 1<<0,
    VOLUME_DIRTY        = 1<<1, // probably in an inconsistent state
    MEDIA_FAILURE       = 1<<2, // failed read/write and exhausted retry algorithms
    CLEAR_TO_ZERO       = 1<<3  // clear this bit to zero on mount
    // rest are reserved
};

template <size_t SectorSize>
struct boot_sector_t {
    uint8_t  jump_boot[3]              = {0x90, 0x76, 0xEB};   // 0xEB7690 little-endian
    uint8_t  fs_name[8]                = {'E', 'X', 'F', 'A', 'T', '\x20', '\x20', '\x20'};
    uint8_t  zero[53]                  = {0};
    uint64_t partition_offset_sectors  = 0;        // Sector address of partition on media (0 to ignore)
    uint64_t volume_length_sectors;                // Size of ExFAT volume in sectors
    uint32_t fat_offset_sectors;                   // 24 <= Sector offset of FAT <= ClusterHeapOffset - FatLength
    uint32_t fat_length_sectors;                   // Size of (each)FAT in sectors
    uint32_t cluster_heap_offset_sectors;          // FatOffset + FatLength <= Offset of Cluster Heap
    uint32_t cluster_count;                        // Number of clusters in cluster heap
    uint32_t root_directory_cluster;               // 2 <= Cluster address of root directory
    uint32_t volume_serial_number      = 0xdead;   // All values are valid!
    uint16_t fs_revision               = 0x0100;
    uint16_t volume_flags              = NO_FLAGS; // Combination of fs_volume_flags_t
    uint8_t  log2_bytes_per_sector;                // eg, log2(512) == 9, 512<= bytes_per_sector <= 4096 so range [9,12]
    uint8_t  log2_sectors_per_cluster;             // at least 0, at most 25-log2_bytes_per_sector (32MB max cluster size)
    uint8_t  num_fats                   = 1;       // 1 or 2. 2 onlyfor TexFAT (not supported)
    uint8_t  drive_select               = 0x80;    // Extended INT 13h drive number
    uint8_t  percent_used               = 0xFF;    // [0,100] Percentage of heap in use or 0xFF if not available
    uint8_t  reserved[7]                = {0};
    uint8_t  boot_code[390]             = {0xF4};  // x86 HLT instruction
    uint16_t boot_signature             = 0xAA55;
	uint8_t  padding[SectorSize - 512];            // Padded out to sector size
} __attribute__((packed));
static_assert(sizeof(boot_sector_t<512>) == 512);

// for a 512-byte sector. should be same size as a sector
template <size_t SectorSize>
struct extended_boot_structure_t {
    uint8_t  extended_boot_code[SectorSize - 4] = {0};
    uint32_t extended_boot_signature = 0xAA550000;
} __attribute__((packed));
static_assert(sizeof(extended_boot_structure_t<512>) == 512);

template <size_t SectorSize>
struct main_extended_boot_region_t {
    extended_boot_structure_t<SectorSize> ebs[8];
} __attribute__((packed));
static_assert(sizeof(main_extended_boot_region_t<512>) == 8*512);

struct oem_parameters_null_entry_t {
    uint8_t guid[16]     = {0};
    uint8_t reserved[32] = {0};
} __attribute__((packed));
static_assert(sizeof(struct oem_parameters_null_entry_t) == 48);

// First 16 bytes of each field is a GUID and remaining 32 bytes are the parameters (undefined)
template <size_t SectorSize>
struct oem_parameters_t {
    struct oem_parameters_null_entry_t null_entries[10];
    uint8_t reserved[SectorSize - 480]  = {0}; // 32-3616 bytes padded out to sector size
} __attribute__((packed));
static_assert(sizeof(oem_parameters_t<512>) == 512);

// one example of a parameter that would go in an OEM parameter but it's not used
struct flash_parameters_t {
    uint8_t  OemParameterType[16];
    uint32_t EraseBlockSize;
    uint32_t PageSize;
    uint32_t NumberOfSpareBlocks;
    uint32_t tRandomAccess;
    uint32_t tProgram;
    uint32_t tReadCycle;
    uint32_t tWriteCycle;
    uint8_t  Reserved[4];
} __attribute__((packed));
static_assert(sizeof(struct flash_parameters_t) == 48);

struct timestamp {
    uint8_t double_seconds[5];
    uint8_t minute[6];
    uint8_t hour[5];
    uint8_t month[4];
    uint8_t year[7];
} __attribute__((packed));

enum MetadataEntryType {
    END_OF_DIRECTORY    = 0x00,
    ALLOCATION_BITMAP   = 0x01 | IN_USE,                         // 0x81
    UPCASE_TABLE        = 0x02 | IN_USE,                         // 0x82
    VOLUME_LABEL        = 0x03 | IN_USE,                         // 0x83
    FILE_DIR_ENTRY      = 0x05 | IN_USE,                         // 0x85

    FILE_DIR_DELETED    = 0x05,                                  // 0x05

    VOLUME_GUID         = 0x20 | IN_USE,                         // 0xA0
    TEXFAT_PADDING      = 0x21 | IN_USE,                         // 0xA1
    WINDOWS_CE_ACT      = 0x22 | IN_USE,                         // 0xA2

    STREAM_EXTENSION    = 0x00 | IN_USE | TYPE_CATEGORY,         // 0xC0
    FILE_NAME           = 0x01 | IN_USE | TYPE_CATEGORY,         // 0xC1
    WINDOWS_CE_ACL      = 0x02 | IN_USE | TYPE_CATEGORY,         // 0xC2

    STREAM_EXT_DELETED  = 0x00 | TYPE_CATEGORY,                  // 0x40
    FILE_NAME_DELETED   = 0x01 | TYPE_CATEGORY,                  // 0x41

    FILE_TAIL           = 0x00 | IN_USE | TYPE_CATEGORY | TYPE_IMPORTANCE,  // 0xE0
};

enum GeneralDirectoryEntryFlags {
    ALLOCATION_POSSIBLE  = 1<<0, // if 0, first cluster and data length will be undefined in directory entry. must be 0 in volume guid
    NO_FAT_CHAIN         = 1<<1  // must be 0 in volume guid
};

struct raw_entry_t {
    uint8_t type;
    uint8_t data[31];
} __attribute__((packed));
static_assert(sizeof(struct raw_entry_t) == 32);

struct file_directory_entry_t {
    uint8_t  type               = FILE_DIR_ENTRY; // FILE_DIR_ENTRY = 0x85
    uint8_t  continuations;     // between 2 and 18
    uint16_t checksum;
    uint16_t attribute_flags;
    uint8_t  reserved1[2]       = {0};
    uint16_t created_time;
    uint16_t created_date;
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t accessed_time;
    uint16_t accessed_date;
    uint8_t  created_time_cs;
    uint8_t  modified_time_cs;
    uint8_t  accessed_time_cs;
    uint8_t  reserved2[9]       = {0};

    bool isValid() {
        if (type != FILE_DIR_ENTRY) return false;
        if (continuations < 2 || continuations > 18) return false;
        return true;
    }

    uint16_t calc_set_checksum() {
         int file_info_size = (continuations + 1) * 32;
         uint8_t *bufp = (uint8_t*)this;
         uint16_t chksum = 0;

         for (int i = 0; i < file_info_size; ++i) {
             if (i != 2 && i != 3) {
                chksum = (((chksum << 15) & 0x8000) | (chksum >> 1)) + (uint16_t)bufp[i];
            }
        }
        return chksum;
    }
} __attribute__((packed));
static_assert(sizeof(struct file_directory_entry_t) == 32);

struct primary_directory_entry_t {
    uint8_t  type;              // one of fs_directory_entry_t
    uint8_t  secondary_count;   // 0 - 255, number of children in directory
    uint16_t set_checksum;      // checksum of directory entries in this set, excluding this field
    uint16_t flags;             // combination of GeneralDirectoryEntryFlags
    uint8_t  custom_defined[14] = {0};
    uint32_t first_cluster;     // 0 = does not exist, otherwise, in range [2,ClusterCount+1]
    uint64_t data_length;
} __attribute__((packed));
static_assert(sizeof(struct primary_directory_entry_t) == 32);

struct secondary_directory_entry_t {
    uint8_t  type;              // one of fs_directory_entry_t
    uint8_t  flags;             // combination of GeneralDirectoryEntryFlags
    uint8_t  custom_defined[18] = {0};
    uint32_t first_cluster;     // 0 = does not exist, otherwise, in range [2,ClusterCount+1]
    uint64_t data_length;
} __attribute__((packed));
static_assert(sizeof(struct secondary_directory_entry_t) == 32);

struct stream_extension_entry_t {
    uint8_t type            = STREAM_EXTENSION;
    uint8_t flags;          // Combination of GeneralDirectoryEntryFlags
    uint8_t reserved1       = 0;
    uint8_t name_length;
    uint16_t name_hash;
    uint16_t reserved2      = 0;
    uint64_t valid_size;
    uint32_t reserved3      = 0;
    uint32_t first_cluster; // 0 = does not exist, otherwise, in range [2,ClusterCount+1]
    uint64_t size;
    bool isValid() {
        if (type != STREAM_EXTENSION) return false;
        return true;
    }
} __attribute__((packed));
static_assert(sizeof(struct stream_extension_entry_t) == 32);

struct vendor_extension_entry_t {
    uint8_t type;
    uint8_t flags;
    uint8_t vendor_guid[16];
    uint8_t vendor_defined[14];
} __attribute__((packed));
static_assert(sizeof(struct vendor_extension_entry_t) == 32);

struct file_name_entry_t {
    static constexpr int FS_FILE_NAME_ENTRY_SIZE = 15;
    uint8_t type        = FILE_NAME;
    uint8_t reserved    = 0;
    int16_t name[FS_FILE_NAME_ENTRY_SIZE];
} __attribute__((packed));
static_assert(sizeof(struct file_name_entry_t) == 32);

struct allocation_bitmap_entry_t {
    uint8_t type            = ALLOCATION_BITMAP;
    uint8_t bitmap_flags    = 0; // 0 if first allocation bitmap, 1 if second (TexFAT only)
    uint8_t reserved[18]    = {0};
    uint32_t first_cluster  = 2;      /* 0 = does not exist, otherwise, in range [2,ClusterCount+1] */
    uint64_t data_length;   // Size of allocation bitmap in bytes. Ceil(ClusterCount / 8)
} __attribute__((packed));
static_assert(sizeof(struct allocation_bitmap_entry_t) == 32);

struct volume_guid_entry_t {
    constexpr volume_guid_entry_t() {
        for (size_t i = 0; i < 16; ++i) {
            volume_guid[i] = 0;
        }
        this->set_checksum = this->calc_checksum();
    }
	constexpr volume_guid_entry_t(const uint8_t guid[16]) {
		for (size_t i = 0; i < 16; ++i) {
			volume_guid[i] = guid[i];
		}
		this->set_checksum = this->calc_checksum();
	}

	// got to check this algo
	constexpr uint16_t calc_checksum() const {
		const uint8_t *data = (const uint8_t *)this;
		uint16_t set_checksum = 0;
		for (size_t i = 0; i < sizeof(struct volume_guid_entry_t); i++) {
			if (i != 2 && i != 3) {
				set_checksum = ((set_checksum << 15) | (set_checksum >> 1)) + data[i];
			}
		}
		return set_checksum;
	}

    uint8_t  type               = VOLUME_GUID; // 0xA0
    uint8_t  secondary_count    = 0;
	uint16_t set_checksum       = 0;
    uint16_t flags              = 0;           // combination of VolumeGuidFlags, must be 0
	uint8_t  volume_guid[16]    = {0};         // must not be null
    uint8_t  reserved[10]       = {0};
} __attribute__((packed));
static_assert(sizeof(struct volume_guid_entry_t) == 32);

struct volume_label_entry_t {
    static constexpr size_t VOLUME_LABEL_MAX_LENGTH = 11;
    constexpr volume_label_entry_t() {}
    constexpr volume_label_entry_t(const std::basic_string<char16_t> &volume_label_utf16) { set_label(volume_label_utf16); }

    void set_label(const std::basic_string<char16_t> &volume_label_utf16) {
        character_count = std::min(VOLUME_LABEL_MAX_LENGTH, volume_label_utf16.length());
        memcpy(volume_label, volume_label_utf16.data(), sizeof(char16_t) * character_count);
    }

    uint8_t type                                    = VOLUME_LABEL; // 0x83 if volume label exists or 0x03 if it was deleted
    uint8_t character_count                         = 0;            // characters in label
    uint16_t volume_label[VOLUME_LABEL_MAX_LENGTH]  = {0};
    uint8_t reserved[8]                             = {0};
} __attribute__((packed));
static_assert(sizeof(struct volume_label_entry_t) == 32);

struct upcase_table_entry_t {
    uint8_t  type           = UPCASE_TABLE; // 0x82
    uint8_t  reserved0[3]   = {0};
    uint32_t checksum;
    uint8_t  reserved1[12]  = {0};
    uint32_t first_cluster  = 3;      /* 0 = does not exist, otherwise, in range [2,ClusterCount+1] */
    uint64_t data_length;
} __attribute__((packed));
static_assert(sizeof(struct upcase_table_entry_t) == 32);

union metadata_entry_u {
    raw_entry_t raw;
    file_directory_entry_t file_directory_entry;
    primary_directory_entry_t primary_directory_entry;
    secondary_directory_entry_t secondary_directory_entry;
    stream_extension_entry_t stream_extension_entry;
    file_name_entry_t file_name_entry;
    allocation_bitmap_entry_t allocation_bitmap_entry;
    volume_guid_entry_t volume_guid_entry;
    volume_label_entry_t volume_label_entry;
    upcase_table_entry_t upcase_table_entry;
} __attribute__((packed));
static_assert(sizeof(metadata_entry_u) == 32);

template <size_t NumClusters>
struct allocation_bitmap_table_t
{
    static constexpr size_t BitmapSize = NumClusters & 0x7 ? (NumClusters >> 3) + 1 : NumClusters >> 3;
    //static constexpr size_t ClusterSize = SectorSize * SectorsPerCluster;
    //static constexpr size_t PaddingSize = ClusterSize - BitmapSize;

    // first bit in the bitmap (cluster 2) is the lowest-order byte
	uint8_t bitmap[BitmapSize] = {0};
	//uint8_t padding[PaddingSize] = {0};

	constexpr allocation_bitmap_table_t() {
		mark_all_alloc();
	}

	void mark_all_alloc() {
		for (size_t i = 0; i < BitmapSize; ++i) {
			bitmap[i] = 0xFF;
		}
	}

    constexpr allocation_bitmap_entry_t get_directory_entry() const {
        return allocation_bitmap_entry_t {
            .data_length = BitmapSize
        };
    }
} __attribute__((packed));
static_assert(sizeof(struct allocation_bitmap_table_t<29806>) == 3726);

template <int SectorSize, int NumEntries>
struct upcase_table_t {
    constexpr upcase_table_t() {
		unsigned i = 0;
        for (; i < 0x61; ++i) {
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

    constexpr upcase_table_entry_t get_directory_entry() const {
        const uint8_t *data = (const uint8_t*)entries;
        size_t sz_bytes = sizeof(char16_t) * NumEntries;
        uint32_t chksum = 0;

        for (size_t i = 0; i < sz_bytes; ++i) {
            chksum = (((chksum<<31) & 0x80000000) | (chksum >> 1)) + (uint32_t)data[i];
        }

        return upcase_table_entry_t {
            .checksum = chksum,
            .data_length = sz_bytes,
        };
    }
} __attribute__((packed));
static_assert(sizeof(struct file_directory_entry_t) == 32);

template <size_t SectorSize>
struct reserved_sector_t {
    uint8_t reserved[SectorSize] = {0};
} __attribute__((packed));
static_assert(sizeof(struct reserved_sector_t<512>) == 512);

template <size_t SectorSize>
struct boot_checksum_sector_t {
    uint32_t checksum[SectorSize / 4]; // same 32-bit checksum thing repeated

    void fill_checksum(uint8_t *vbr, size_t vbr_size)
    {
        assert(vbr_size == SectorSize * 11);

        uint32_t chksum = 0;
        for (size_t i = 0; i < vbr_size; ++i) {
            if (i != 106 && i != 107 && i != 112) {
                chksum = (((chksum << 31) &0x80000000) | (chksum >> 1)) + (uint32_t)vbr[i];
            }
        }
        for (size_t i = 0; i < sizeof(checksum); ++i) {
            checksum[i] = chksum;
        }
    }
} __attribute__((packed));
static_assert(sizeof(struct boot_checksum_sector_t<512>) == 512);

template <size_t SectorSize>
struct boot_region_t {
    boot_sector_t<SectorSize>               vbr;            // Sector 0
    main_extended_boot_region_t<SectorSize> mebs;           // Sector 1-8
    oem_parameters_t<SectorSize>            oem_params;     // Sector 9
    reserved_sector_t<SectorSize>           reserved;       // Sector 10
    boot_checksum_sector_t<SectorSize>      checksum;       // Sector 11
} __attribute__((packed));
static_assert(sizeof(boot_region_t<512>) == 12*512);

enum FatEntrySpecial {
    BAD_CLUSTER                 = 0xFFFFFFF7,
    MEDIA_DESCRIPTOR_HARD_DRIVE = 0xFFFFFFF8,
    END_OF_FILE                 = 0xFFFFFFFF
};

template <size_t SectorSize, size_t SectorsPerCluster, size_t ClustersInFat>
struct file_allocation_table_t
{
    static constexpr size_t FatSize = (ClustersInFat + 2) * sizeof(uint32_t);
    static constexpr size_t PaddingSize = SectorSize - (FatSize % SectorSize);

    uint32_t media_type             = MEDIA_DESCRIPTOR_HARD_DRIVE;
    uint32_t reserved               = END_OF_FILE;   // Must be 0xFFFFFFFF
    uint32_t entries[ClustersInFat] = {END_OF_FILE}; // ??
    uint8_t  padding[PaddingSize]   = {0}; // pad to sector
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster>
struct root_directory_t
{
    static constexpr size_t ClusterSize = SectorSize * SectorsPerCluster;

    volume_label_entry_t        label_entry;
    allocation_bitmap_entry_t   bitmap_entry;
    upcase_table_entry_t        upcase_entry;
    volume_guid_entry_t         guid_entry;
    file_directory_entry_t      directory_entry;
    stream_extension_entry_t    ext_entry;
    file_name_entry_t           name_entry;
    secondary_directory_entry_t directory_entries[0]; // dynamically sized based on number of child entities
} __attribute__((packed));

struct directory_t
{
    primary_directory_entry_t primary_entry;
    secondary_directory_entry_t secondary_entries[0];
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster, size_t ClustersInFat>
struct cluster_heap_t
{
    cluster_t<SectorSize, SectorsPerCluster> storage[ClustersInFat];
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster, size_t ClustersInFat>
struct fat_region_t {
    constexpr static size_t cluster_heap_start_sector = 0x283D8;
    constexpr static size_t fat_heap_alignment_sectors = cluster_heap_start_sector -
        (2 * sizeof(boot_region_t<SectorSize>) + // 24 sectors = 12288 bytes
        sizeof(file_allocation_table_t<SectorSize, SectorsPerCluster, ClustersInFat>)) / SectorSize;

    file_allocation_table_t<SectorSize, SectorsPerCluster, ClustersInFat> fat;
    sector_t<SectorSize> fat_cluster_heap_alignment[fat_heap_alignment_sectors];
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors, size_t ClustersInFat>
struct data_region_t {
    constexpr static size_t ExcessSectors = NumSectors - ClustersInFat * SectorsPerCluster;

    cluster_heap_t<SectorSize, SectorsPerCluster, ClustersInFat> cluster_heap;
    sector_t<SectorSize> excess_space[ExcessSectors];
} __attribute__((packed));

template <size_t SectorSize, size_t SectorsPerCluster, size_t NumSectors>
struct filesystem_t {
    constexpr static size_t ClusterSize = SectorSize * SectorsPerCluster;
    constexpr static size_t ClustersInFat = (NumSectors - 0x283D8) / 512;
    boot_region_t<SectorSize>                                                  main_boot_region;
    // copy of main_boot_region
    boot_region_t<SectorSize>                                                  backup_boot_region;
    fat_region_t<SectorSize, SectorsPerCluster, ClustersInFat>                 fat_region;
    //root directory is in the cluster heap
    data_region_t<SectorSize, SectorsPerCluster, NumSectors, ClustersInFat>    data_region;
} __attribute__((packed));

} /* namespace exfat */
} /* namespace resurrExion */
} /* namespace paulyc */
} /* namespace github */

#endif /* _github_paulyc_exfat_structs_hpp_ */
