//
//  types.hpp
//  resurrExion
//
//  Created by Paul Ciarlo on 1 July 2020
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

#ifndef _github_paulyc_types_hpp_
#define _github_paulyc_types_hpp_

#include <functional>
#include <string>
#include <iomanip>
#include <cstddef>

namespace github {
namespace paulyc {
namespace resurrExion {

// no high bit
typedef uint64_t byteofs_t;
typedef int64_t  bytediff_t;
static constexpr auto stobyteofs = [](const std::string &str, std::size_t *idx, int base) { return std::stoull(str, idx, base); };
static constexpr byteofs_t INVALID_BYTEOFS = ~0;

// no high bit
typedef uint64_t sectorofs_t;
typedef int64_t  sectordiff_t;
static constexpr auto stosectorofs = stobyteofs;
static constexpr byteofs_t INVALID_SECTOROFS = ~0;

// no high bit
typedef uint32_t clusterofs_t;
typedef int32_t  clusterdiff_t;
static constexpr auto stoclusterofs = stobyteofs;
static constexpr byteofs_t INVALID_CLUSTEROFS = ~0;

constexpr static size_t       SectorSize             = 512; // 0x0200
constexpr static size_t       SectorsPerCluster      = 512; // 0x0200
constexpr static sectorofs_t  NumSectors             = 7813560247;
constexpr static clusterofs_t ClustersInFat          = (NumSectors - 0x283D8) / 512;
constexpr static sectorofs_t  PartitionStartSector   = 0x64028;
constexpr static sectorofs_t  ClusterHeapStartSector = 0x283D8; // relative to partition start
constexpr static byteofs_t    DiskSize               = (NumSectors + PartitionStartSector) * SectorSize; // 0x000003a352944000 = 40000000000 = 4TB
constexpr static sectorofs_t  ClusterHeapStartSectorVsDisk = PartitionStartSector + ClusterHeapStartSector; // relative to whole disk
constexpr static byteofs_t    ClusterHeapStartOffset = ClusterHeapStartSectorVsDisk * SectorSize;

byteofs_t cluster_number_to_offset(clusterofs_t cluster) {
    // have to subtract 2 because the first cluster in the cluster heap has index 2 and
    // by my definition the cluster heap simply starts there
    return ((cluster) * SectorsPerCluster + ClusterHeapStartSectorVsDisk) * SectorSize;
}

}
}
}

#endif /* _github_paulyc_types_hpp_ */
