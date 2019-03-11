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

#include <iostream>

#if USE_CPPUNIT

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <system_error>

int main(int argc, char *argv[])
{
    CppUnit::TextUi::TestRunner runner;
    CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();
    runner.addTest(registry.makeTest());
    bool wasSuccessful = runner.run("", false);
    return !wasSuccessful;
}

#else

#include "../src/logger.hpp"
#include "../src/filesystem.hpp"

using namespace io::github::paulyc;

int main(int argc, char *argv[])
{
    Loggable l;
    l.logf(Loggable::INFO, "No tests run, #define USE_CPPUNIT\n");
    //std::cerr << "No tests run, #define USE_CPPUNIT" << std::endl;
    l.logf(Loggable::INFO, "%016llx\n", 10);
    l.logf(Loggable::INFO, "%08lx\n", 10);

    l.logf(Loggable::INFO, "sizeof(struct fs_boot_region) == %d\n", sizeof(struct fs_boot_region<512>));
    assert(sizeof(struct fs_boot_region<512>) % 512 == 0);
    l.logf(Loggable::INFO, "sizeof(struct fs_file_allocation_table) == %d\n", sizeof(struct fs_file_allocation_table<512, 512, 7813560247>));
    assert(sizeof(struct fs_file_allocation_table<512, 512, 7813560247>) % 512 == 0);

    // TODO figure this out....
    //const size_t cluster_heap_sector_offset_expected = cluster_heap_partition_start_sector + partition_start_sector;
    //const size_t cluster_heap_sector_offset_calculated =
    //    (sizeof(fs_volume_metadata<512, 512, 7813560247>) + sizeof(fs_root_directory<512, 512>)) / 512 + partition_start_sector;
    //assert(cluster_heap_sector_offset_expected == cluster_heap_sector_offset_calculated);
    return 0;
}

#endif
