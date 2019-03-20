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
#include <regex>

#include "logger.hpp"

namespace io {
namespace github {
namespace paulyc {
namespace ExFATRestore {

constexpr int exfat_filename_maxlen = 256;
constexpr int exfat_filename_maxlen_utf8 = exfat_filename_maxlen * 2;

static constexpr int32_t BadSectorFlag = -1;

template <typename Filesystem_T>
class RecoveryLogBase : public Loggable
{
public:
    RecoveryLogBase(const std::string &filename) : _filename(filename) {}
    virtual ~RecoveryLogBase() {}

protected:
    std::string _get_utf8_filename(uint8_t *fh, int namelen)
    {
        struct fs_file_directory_entry *fde = (struct fs_file_directory_entry *)fh;
        const int continuations = fde->continuations;
        std::basic_string<char16_t> u16s;
        for (int c = 1; c <= continuations; ++c) {
            fh += 32;
            if (fh[0] == FILE_NAME) {
                struct fs_file_name_entry *n = (struct fs_file_name_entry*)fh;
                for (int i = 0; i < sizeof(n->name); ++i) {
                    if (u16s.length() == namelen) {
                        return _cvt.to_bytes(u16s);
                    } else {
                        u16s.push_back(n->name[i]);
                    }
                }
            }
        }
        return _cvt.to_bytes(u16s);
    }

    std::string _filename;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> _cvt;
};

template <typename Filesystem_T>
class RecoveryLogReader : public RecoveryLogBase<Filesystem_T>
{
public:
    RecoveryLogReader(const std::string &filename) : RecoveryLogBase<Filesystem_T>(filename) {}
    virtual ~RecoveryLogReader() {}
};

template <typename Filesystem_T>
class RecoveryLogTextReader : public RecoveryLogReader<Filesystem_T>
{
public:
    RecoveryLogTextReader(const std::string &filename) : RecoveryLogReader<Filesystem_T>(filename), _logfile(filename) {}

    void parseTextLog(Filesystem_T &fs, std::function<void(size_t, std::variant<std::string, std::exception, bool>)> cb)
    {
        std::regex fde("FDE ([0-9a-fA-F]{16})(?: (.*))?");
        std::regex badsector("BAD_SECTOR ([0-9a-fA-F]{16})");

        for (std::string line; std::getline(_logfile, line); ) {
            std::smatch sm;
            if (std::regex_match(line, sm, fde)) {
                size_t offset;
                std::string filename;
                filename = sm[2];
                try {
                    offset = std::stol(sm[1], nullptr, 16);
                    std::shared_ptr<typename Filesystem_T::BaseEntity> ent = fs.loadEntity(offset);
                    cb(offset, std::variant<std::string, std::exception, bool>(filename));
                } catch (std::exception &ex) {
                    std::cerr << "Writing file entry to binlog, got exception: " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                    std::cerr << "Invalid textlog FDE line, skipping: " << line << std::endl;
                    cb(0, std::variant<std::string, std::exception, bool>(ex));
                }
            } else if (std::regex_match(line, sm, badsector)) {
                size_t offset;
                try {
                    offset = std::stol(sm[1], nullptr, 16);
                    cb(offset, std::variant<std::string, std::exception, bool>(true));
                } catch (std::exception &ex) {
                    std::cerr << "Writing bad sector to binlog, got exception " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                    std::cerr << "Invalid textlog BAD_SECTOR line, skipping: " << line << std::endl;
                    cb(0, std::variant<std::string, std::exception, bool>(ex));
                }
            } else {
                std::cerr << "Unknown textlog line format: " << line << std::endl;
                cb(0, std::variant<std::string, std::exception, bool>(std::exception()));
            }
        }
    }
private:
    std::ifstream _logfile;
};

template <typename Filesystem_T>
class RecoveryLogBinaryReader : public RecoveryLogReader<Filesystem_T>
{
public:
    RecoveryLogBinaryReader(const std::string &filename) : RecoveryLogReader<Filesystem_T>(filename), _logfile(filename, std::ios::binary) {}
private:
    std::ifstream _logfile;
};

template <typename Filesystem_T>
class RecoveryLogWriter : public RecoveryLogBase<Filesystem_T>
{
public:
    RecoveryLogWriter(const std::string &filename) : RecoveryLogBase<Filesystem_T>(filename) {}
    virtual ~RecoveryLogWriter() {}
};

template <typename Filesystem_T>
class RecoveryLogTextWriter : public RecoveryLogWriter<Filesystem_T>
{
public:
    RecoveryLogTextWriter(const std::string &filename) : RecoveryLogWriter<Filesystem_T>(filename), _logfile(filename, std::ios::trunc) {}

    //void writeTextLog(const std::string &devfilename, const std::string &logfilename);
private:
    std::ofstream _logfile;
};

template <typename Filesystem_T>
class RecoveryLogBinaryWriter : public RecoveryLogWriter<Filesystem_T>
{
public:
    RecoveryLogBinaryWriter(const std::string &filename) : RecoveryLogWriter<Filesystem_T>(filename), _logfile(filename, std::ios::binary | std::ios::trunc) {}

    void writeBadSectorToBinLog(size_t offset) {
        _writeToBinLog((const char *)&offset, sizeof(size_t));
        _writeToBinLog((const char *)&BadSectorFlag, sizeof(int32_t));
    }

    void writeEntityToBinLog(size_t offset, uint8_t *fde, std::shared_ptr<typename Filesystem_T::BaseEntity> entity) {
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

#include "recoverylog.cpp"

#endif /* _io_github_paulyc_recoverylog_hpp_ */
