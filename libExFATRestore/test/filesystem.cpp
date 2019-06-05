//
//  filesystem.cpp - Filesystem tests
//  ExFATRestore
//
//  Created by Paul Ciarlo on 5 March 2019.
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

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "../config/fsconfig.hpp"
#include "../src/filesystem.hpp"
using namespace io::github::paulyc;

BOOST_AUTO_TEST_SUITE(FilesystemTestSuite)

BOOST_AUTO_TEST_CASE(TestStructs)
{
	Loggable l;
	l.logf(Loggable::INFO, "%016llx\n", 10);
	l.logf(Loggable::INFO, "%08lx\n", 10);

	l.logf(Loggable::INFO, "sizeof(struct fs_boot_region) == %d\n", sizeof(struct fs_boot_region<512>));
	BOOST_STATIC_ASSERT(sizeof(struct fs_boot_region<SectorSize>) % SectorSize == 0);
	l.logf(Loggable::INFO, "sizeof(struct fs_file_allocation_table) == %d\n", sizeof(struct fs_file_allocation_table<SectorSize, SectorsPerCluster, NumSectors>));
	BOOST_STATIC_ASSERT(sizeof(struct fs_file_allocation_table<SectorSize, SectorsPerCluster, NumSectors>) % 512 == 0);

	constexpr size_t fs_headers_size_bytes =
	sizeof(fs_boot_region<SectorSize>) * 2 +
	sizeof(fs_fat_region<SectorSize, SectorsPerCluster, ClustersInFat>);
	BOOST_ASSERT((fs_headers_size_bytes % SectorSize) == 0);
	constexpr size_t fs_headers_size_sectors = fs_headers_size_bytes / SectorSize;
	l.logf(Loggable::INFO, "ClustersInFat == %08x\n", ClustersInFat);
	l.logf(Loggable::INFO, "fs_headers_size_sectors == %08x\n", fs_headers_size_sectors);
	BOOST_ASSERT(fs_headers_size_sectors == ClusterHeapStartSector);
}

BOOST_AUTO_TEST_SUITE_END()
