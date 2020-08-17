//
//  boneyard.cpp
//  resurrExion
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

int fix_orphans_method(const std::vector<std::string> &args) {
    if (args.size() < 2) {
        return -1;
    }
    FilesystemStub stub;
    stub.open(args[0]);
    stub.parseTextLog(args[1]);
    stub.adopt_orphans();
    stub.dirty_writeback();
    stub.close();
    return 0;
}

int orphans_method(const std::vector<std::string> &args) {
    if (args.size() < 3) {
        return -1;
    }
    FilesystemStub stub;
    stub.open(args[0]);
    RecoveryLog<> log;
    //stub.parseTextLog(args[1]);
    //stub.log_results(args[2].c_str());
    stub.log_sql_results("logfile.sql");
    stub.close();
    return 0;
}

//
//  main.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 2/11/19
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

#include <iostream>
#include <string>
#include <list>

#include "database.hpp"
#include "filesystem.hpp"
#include "../config/fsconfig.hpp"

using github::paulyc::resurrExion::ExFATFilesystem;

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
    sectorofs_t num_sectors;
    byteofs_t cluster_heap_offset;
};

int help_method(const char *argv0) {
    std::cerr << argv0 << " <method>" << std::endl;
    return -1;
}

int analyze_method(const std::vector<std::string> &args) {
    if (args.size() < 1) return -1;

    return -2;
}

int init_method(const std::vector<std::string> &args) {
    if (args.size() < 1) return -1;
    try {
        ExFATFilesystem<SectorSize, SectorsPerCluster, NumSectors>  fs(args[0].c_str(), DiskSize, PartitionStartSector, false);
        fs.init_metadata();
        return 0;
    } catch (std::exception &ex) {
        std::cerr << "Exception " << typeid(ex).name() << " caught: " << ex.what() << std::endl;
        return -2;
    }
}

//void log_sql(const char *sqlfilename);
int orphans_method(const std::vector<std::string> &args);
int fix_orphans_method(const std::vector<std::string> &args);

//class parser
enum method { default_, help, analyze, orphans, fix_orphans };

int main()
{
    method m = default_;
    std::vector<std::string> method_args;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "analyze") {
            m = analyze;
        } else if (arg == "help") {
            m = help;
        } else if (arg == "orphans") {
            m = orphans;
        } else if (arg == "fix-orphans") {
            m = fix_orphans;
        } else {
            method_args.push_back(arg);
        }
    }

    switch (m) {
        case analyze:
            return analyze_method(method_args);
        case default_:
        case orphans:
            return orphans_method(method_args);
        case fix_orphans:
            return fix_orphans_method(method_args);
        case help:
        default:
            return help_method(argv[0]);
    }


    namespace poptions = boost::program_options;
    poptions::options_description optdesc("Allowed options");
    optdesc.add_options()
        ("help,h", "show help")
    ;
    poptions::positional_options_description p;
    poptions::variables_map varmap;
    poptions::command_line_parser clparser(argc, argv);
    clparser.options(optdesc);
    clparser.positional(p);
    poptions::store(clparser.run(), varmap);
    poptions::notify(varmap);

    try {
        github::paulyc::resurrExion::RecoveryLogReader reader;
        reader.parse(std::string(argv[1]), std::string(argv[2]), std::string(argv[3]));
        github::paulyc::resurrExion::RecoveryLog<github::paulyc::resurrExion::ExFATFilesystem<SectorSize, 512, 7813560247>> writer;
        writer.writeTextLog("/dev/sdb"/*argv[1]*/, "recovery.log"/*argv[2]*/);
        github::paulyc::resurrExion::RecoveryLogWriter writer;
        writer.textLogToBinLog(argv[1], argv[2], argv[3]);
        github::paulyc::resurrExion::FileRestorer restorer(argv[1], argv[3]);
        restorer.restore_all_files(argv[2]);
        github::paulyc::resurrExion::ExFATFilesystem<SectorSize, 512, 7813560247>  fs(argv[1], 4000000000000, 409640);
        fs.restore_all_files(argv[3], argv[2]);
    } catch (std::exception &ex) {
        std::cerr << "Exception " << typeid(ex).name() << " caught: " << ex.what() << std::endl;
        return -2;
    }
    return 0;
}
