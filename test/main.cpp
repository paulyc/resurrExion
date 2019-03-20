//
//  main.cpp - Test runner
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

#ifdef _DEBUG
#include "../src/exception.hpp"
#else
#define _DEBUG 1
#include "../src/exception.hpp"
#undef _DEBUG
#endif

#include "../src/logger.hpp"
#include "../src/filesystem.hpp"
#include "../config/fsconfig.hpp"

using namespace io::github::paulyc;

#if USE_CPPUNIT

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

bool test_cppunit()
{
    CppUnit::TextUi::TestRunner runner;
    CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();
    runner.addTest(registry.makeTest());
    return runner.run("", false);
}

#else /* USE_CPPUNIT */

bool test_cppunit()
{
    std::cerr << "WARNING: Didn't run the CppUnit tests as USE_CPPUNIT is not defined" << std::endl;
    return true;
}

#endif /* USE_CPPUNIT */

bool test_assert_failure()
{
    try {
        ASSERT_THROW(2+2 == 5);
    } catch (const std::exception &ex) {
        std::cerr << "ASSERT_THROW(2+2 == 5) threw: " << ex.what() << std::endl;

        if (std::string(ex.what()).find("2+2 == 5") == std::string::npos) {
            return false;
        }
    }
    return true;
}

bool test_logger()
{
    Loggable l;
    l.logf(Loggable::INFO, "%016llx\n", 10);
    l.logf(Loggable::INFO, "%08lx\n", 10);

    l.logf(Loggable::INFO, "sizeof(struct fs_boot_region) == %d\n", sizeof(struct fs_boot_region<512>));
    ASSERT_THROW(sizeof(struct fs_boot_region<SectorSize>) % SectorSize == 0);
    l.logf(Loggable::INFO, "sizeof(struct fs_file_allocation_table) == %d\n", sizeof(struct fs_file_allocation_table<SectorSize, SectorsPerCluster, NumSectors>));
    ASSERT_THROW(sizeof(struct fs_file_allocation_table<SectorSize, SectorsPerCluster, NumSectors>) % 512 == 0);

    constexpr size_t fs_headers_size_bytes =
    sizeof(fs_boot_region<SectorSize>) * 2 +
    sizeof(fs_fat_region<SectorSize, SectorsPerCluster, ClustersInFat>);
    ASSERT_THROW((fs_headers_size_bytes % SectorSize) == 0);
    constexpr size_t fs_headers_size_sectors = fs_headers_size_bytes / SectorSize;
    l.logf(Loggable::INFO, "ClustersInFat == %08x\n", ClustersInFat);
    l.logf(Loggable::INFO, "fs_headers_size_sectors == %08x\n", fs_headers_size_sectors);
    ASSERT_THROW(fs_headers_size_sectors == ClusterHeapStartSector);
    return true;
}

int main(int argc, char *argv[])
{
    bool success = true;
    try {
        success &= test_assert_failure();
        success &= test_logger();
        success &= test_cppunit();
    } catch (const std::exception &ex) {
        success = false;
        std::cerr << "Test threw exception " << ex.what() << std::endl;
    }

    if (success) {
        std::cerr << "Tests passed." << std::endl;
        return 0;
    } else {
        std::cerr << "Tests failed." << std::endl;
        return 1;
    }
}
