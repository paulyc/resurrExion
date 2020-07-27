//
//  fsconfig.hpp - Parameters of a specific ExFAT filesystem
//  resurrExion
//
//  Created by Paul Ciarlo on 15 March 2019.
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

#ifndef _github_paulyc_exfat_fsconfig_hpp_
#define _github_paulyc_exfat_fsconfig_hpp_

/*
 static constexpr size_t sector_size_bytes = 512; // bytes 0x0200
 static constexpr size_t sectors_per_cluster = 512; // 0x0200
 static constexpr size_t cluster_size_bytes = sector_size_bytes * sectors_per_cluster; // 0x040000
 static constexpr size_t disk_size_bytes = 0x000003a352944000; // 40000000000; // 4TB
 static constexpr size_t cluster_heap_disk_start_sector = 0x8c400;
 static constexpr size_t cluster_heap_partition_start_sector = 0x283D8;
 static constexpr size_t partition_start_sector = 0x64028;

 static constexpr uint32_t start_offset_cluster = 0x00adb000;
 static constexpr size_t start_offset_bytes = start_offset_cluster * cluster_size_bytes;
 static constexpr size_t start_offset_sector = start_offset_bytes / sector_size_bytes;
 */

#include <cstddef>

#include "../libresurrExion/src/types.hpp"

constexpr static size_t SectorSize = 512;
constexpr static size_t SectorsPerCluster = 512;
constexpr static size_t ClusterSize = SectorSize*SectorsPerCluster;
constexpr static sectorofs_t NumSectors = 7813560247;
constexpr static clusterofs_t ClustersInFat = (NumSectors - 0x283D8) / 512;
constexpr static sectorofs_t PartitionStartSector = 0x64028;
constexpr static sectorofs_t ClusterHeapStartSector = 0x283D8; // relative to partition start
constexpr static size_t ClusterHeapStartSectorRelWholeDisk = PartitionStartSector + ClusterHeapStartSector;
constexpr static size_t ClusterHeapStartOffset = ClusterHeapStartSectorRelWholeDisk * SectorSize;
constexpr static byteofs_t DiskSize = (NumSectors + PartitionStartSector) * SectorSize;

#endif /* _github_paulyc_exfat_fsconfig_hpp_ */
