//
//  main.cpp
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

#include <iostream>
#include <string>

#include "filesystem.hpp"
#include "../config/fsconfig.hpp"

struct Options
{
    std::string deviceFileName;
    std::string binaryLogFileName;
    std::string textLogFileName;
    bool write_changes = false;
    bool reconstruct = false;
    bool analyze = false;
    bool write_text_log = false;
    bool write_binary_log = false;
    bool read_text_log = false;
    bool read_binary_log = false;
    size_t sector_size = 512;
    size_t sectors_per_cluster = 512;
    size_t num_sectors;
    size_t cluster_heap_offset;
};

int main(int argc, const char * argv[]) {
#if 1
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <device>" << std::endl;
        return -1;
    }

    try {
        io::github::paulyc::ExFATRestore::ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>  fs(argv[1], DiskSize, PartitionStartSector, false);
        fs.init_metadata();
    } catch (std::exception &ex) {
        std::cerr << "Exception " << typeid(ex).name() << " caught: " << ex.what() << std::endl;
        return -2;
    }
#endif

#if 0
    if (argc != 4) {
        std::cerr << "usage: " << argv[0] << " <device> <logfile> <output_dir>" << std::endl;
        return -1;
    }

    try {
        io::github::paulyc::ExFATRestore::ExFATFilesystem<SectorSize, 512, 7813560247>  fs(argv[1], 4000000000000, 409640);
        fs.restore_all_files(argv[3], argv[2]);
    } catch (std::exception &ex) {
        std::cerr << "Exception " << typeid(ex).name() << " caught: " << ex.what() << std::endl;
        return -2;
    }
#endif

#if 0
    if (argc != 4) {
        std::cerr << "usage: " << argv[0] << " <device> <logfile> <output_dir>" << std::endl;
        return -1;
    }

    try {
        io::github::paulyc::ExFATRestore::FileRestorer restorer(argv[1], argv[3]);
        restorer.restore_all_files(argv[2]);
    } catch (std::exception &ex) {
        std::cerr << "Exception " << typeid(ex).name() << " caught: " << ex.what() << std::endl;
        return -2;
    }
#endif

#if 0
    if (argc != 4) {
        std::cerr << "usage: " << argv[0] << " <device> <input_textlogfile> <output_binlogfile>" << std::endl;
        return -1;
    }

    try {
        io::github::paulyc::ExFATRestore::RecoveryLogWriter writer;
        writer.textLogToBinLog(argv[1], argv[2], argv[3]);
    } catch (std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return -2;
    }
#endif

#if 0
    if (argc < 3 || argc > 3) {
        std::cerr << "usage: " << argv[0] << " <device> <logfile>" << std::endl;
        return -1;
    }

    try {
        io::github::paulyc::ExFATRestore::RecoveryLogWriter writer;
        writer.writeLog(argv[1], argv[2]);
    } catch (std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return -2;
    }
#endif
#if 0
    if (argc < 4 || argc > 4) {
        std::cerr << "usage: " << argv[0] << " <device> <logfile> <outputdir>" << std::endl;
        return -1;
    }
    
    try {
        io::github::paulyc::ExFATRestore::RecoveryLogReader reader;
        reader.parse(std::string(argv[1]), std::string(argv[2]), std::string(argv[3]));
    } catch (std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return -2;
    }
#endif
    return 0;
}
