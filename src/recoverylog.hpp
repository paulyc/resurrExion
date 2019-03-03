//
//  recoverylog.hpp
//  ExFATRestore
//
//  Created by Paul Ciarlo on 2/12/19.
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

#ifndef _io_github_paulyc_recoverylog_hpp_
#define _io_github_paulyc_recoverylog_hpp_

#include <string>
#include <fstream>
#include <functional>
#include <variant>
#include <locale>
#include <codecvt>

#include "filesystem.hpp"

namespace io {
namespace github {
namespace paulyc {
namespace ExFATRestore {

static constexpr size_t sector_size_bytes = 512; // bytes 0x0200
static constexpr size_t sectors_per_cluster = 512; // 0x0200
static constexpr size_t cluster_size_bytes = sector_size_bytes * sectors_per_cluster; // 0x040000
static constexpr size_t disk_size_bytes = 0x000003a352944000; // 0x0000040000000000; // 4TB
static constexpr size_t cluster_heap_disk_start_sector = 0x8c400;
static constexpr size_t cluster_heap_partition_start_sector = 0x283D8;
static constexpr size_t partition_start_sector = 0x64028;

static constexpr uint32_t start_offset_cluster = 0x00adb000;
static constexpr size_t start_offset_bytes = start_offset_cluster * cluster_size_bytes;
static constexpr size_t start_offset_sector = start_offset_bytes / sector_size_bytes;

class RecoveryLogBase
{
public:
    RecoveryLogBase(const std::string &filename) : _filename(filename) {}
    virtual ~RecoveryLogBase() {}

    static constexpr int32_t BadSectorFlag = -1;

protected:
    std::string _get_utf8_filename(uint8_t *fh, int namelen);

    std::string _filename;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> _cvt;
};

class RecoveryLogReader : public RecoveryLogBase
{
public:
    RecoveryLogReader(const std::string &filename) : RecoveryLogBase(filename) {}
    virtual ~RecoveryLogReader() {}
};

class RecoveryLogTextReader : public RecoveryLogReader
{
public:
    RecoveryLogTextReader(const std::string &filename) : RecoveryLogReader(filename), _logfile(filename) {}

    void parseTextLog(ExFATFilesystem &fs, std::function<void(size_t, std::variant<std::string, std::exception, bool>)>);
private:
    std::ifstream _logfile;
};

class RecoveryLogBinaryReader : public RecoveryLogReader
{
public:
    RecoveryLogBinaryReader(const std::string &filename) : RecoveryLogReader(filename), _logfile(filename, std::ios::binary) {}
private:
    std::ifstream _logfile;
};

class RecoveryLogWriter : public RecoveryLogBase
{
public:
    RecoveryLogWriter(const std::string &filename) : RecoveryLogBase(filename) {}
    virtual ~RecoveryLogWriter() {}
};

class RecoveryLogTextWriter : public RecoveryLogWriter
{
public:
    RecoveryLogTextWriter(const std::string &filename) : RecoveryLogWriter(filename), _logfile(filename, std::ios::trunc) {}

    void writeTextLog(const std::string &devfilename, const std::string &logfilename);
private:
    std::ofstream _logfile;
};

class RecoveryLogBinaryWriter : public RecoveryLogWriter
{
public:
    RecoveryLogBinaryWriter(const std::string &filename) : RecoveryLogWriter(filename), _logfile(filename, std::ios::binary | std::ios::trunc) {}

    void writeBadSectorToBinLog(size_t offset) {
        _writeToBinLog((const char *)&offset, sizeof(size_t));
        _writeToBinLog((const char *)&BadSectorFlag, sizeof(int32_t));
    }

    void writeEntityToBinLog(size_t offset, uint8_t *fde, std::shared_ptr<ExFATFilesystem::BaseEntity> entity) {
        const int entries_size_bytes = entity->get_file_info_size();
        _writeToBinLog((const char*)&offset, sizeof(size_t));
        _writeToBinLog((const char*)&entries_size_bytes, sizeof(int32_t));
        _writeToBinLog((const char*)fde, entries_size_bytes);
    }
private:
    void _writeToBinLog(const char *buf, size_t count) { _logfile.write(buf, count); }

    std::ofstream _logfile;
};

}
}
}
}

#endif /* _io_github_paulyc_recoverylog_hpp_ */
