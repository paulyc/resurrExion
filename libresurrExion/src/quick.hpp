//
//  quick.hpp
//  resurrExion
//
//  Created by Paul Ciarlo on 5 July 2020
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

#ifndef _github_paulyc_quick_hpp_
#define _github_paulyc_quick_hpp_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <unistd.h>

#ifndef O_RSYNC
#define O_RSYNC O_SYNC
#endif

#include <unordered_map>
#include <typeinfo>
#include <exception>
#include <system_error>
#include <locale>
#include <codecvt>
#include <vector>
#include <fstream>
#include <iostream>
#include <functional>
#include <memory>
#include <regex>
#include <list>

#include <mariadb++/connection.hpp>

#include "exfat_structs.hpp"

using namespace github::paulyc;
using namespace github::paulyc::resurrExion;

typedef uint8_t my_cluster[512*512];

#include "../../config/fsconfig.hpp"
/*constexpr static size_t SectorSize             = 512;
constexpr static size_t SectorsPerCluster      = 512;
constexpr static size_t NumSectors             = 7813560247;
constexpr static size_t ClustersInFat          = (NumSectors - 0x283D8) / 512;
constexpr static size_t PartitionStartSector   = 0x64028;
constexpr static size_t ClusterHeapStartSector = 0x283D8; // relative to partition start
constexpr static size_t DiskSize               = (NumSectors + PartitionStartSector) * SectorSize;
*/

class Entity;
class File;
class Directory;

namespace {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cvt;
    std::shared_ptr<Directory> RootDirectory;
}

inline bool is_valid_filename_char(char16_t ch) {
    switch (ch) {
    case '\'':
    case '*':
    case '/':
    case ':':
    case '<':
    case '>':
    case '?':
    case '\\':
    case '|':
        return false;
    default:
        return ch >= 0x0020;
    }
}

std::string get_utf8_filename(exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde);

#include "quickentity.hpp"

#include "quickfs.hpp"

#endif
