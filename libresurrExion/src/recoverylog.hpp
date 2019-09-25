//
//  recoverylog.hpp
//  resurrExion
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

#ifndef _github_paulyc_recoverylog_hpp_
#define _github_paulyc_recoverylog_hpp_

#include <string>
#include <fstream>
#include <functional>
#include <variant>
#include <locale>
#include <codecvt>
#include <regex>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <cstdio>
#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

#include "logger.hpp"
#include "exfat_structs.hpp"

namespace github {
namespace paulyc {
namespace resurrExion {

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
        exfat::file_directory_entry_t *fde = (exfat::file_directory_entry_t *)fh;
        const int continuations = fde->continuations;
        std::basic_string<char16_t> u16s;
        for (int c = 1; c <= continuations; ++c) {
            fh += 32;
            if (fh[0] == exfat::FILE_NAME) {
                exfat::file_name_entry_t *n = (exfat::file_name_entry_t*)fh;
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

    void writeTextLog(
        const std::string &devfilename,
        const std::string &logfilename)
    {

        std::ifstream dev(devfilename, std::ios::binary);
        std::ofstream log(logfilename, std::ios::app);

        bool eof = false;
        uint64_t buffer_offset = 0x0000000000000000;
                              // 0x0000000100000000 == 4 GB
        uint8_t buffer[0x100000], *bufp = buffer, *bufend = buffer + sizeof(buffer);
        dev.seekg(buffer_offset);
        dev.read((char*)buffer, sizeof(buffer));

        while (!eof || bufp + 96 < bufend) {
            //if (io::github::paulyc::ExFATRestore::ExFATFilesystem::verifyFileEntity(bufp, bufend - bufp)) {
            //}

            if (bufp[0] == exfat::FILE_DIR_ENTRY) {
                if (bufp[32] == exfat::STREAM_EXTENSION && bufp[64] == exfat::FILE_NAME) {
                    struct fs_file_directory_entry *m1 = (struct fs_file_directory_entry*)(bufp);
                    struct fs_stream_extension_directory_entry *m2 = (struct fs_stream_extension_directory_entry*)(bufp+32);
                    const int continuations = m1->continuations;
                    if (continuations >= 2 && continuations <= 18) {
                        int i, file_info_size = (continuations + 1) * 32;
                        uint16_t chksum = 0;

                        for (i = 0; i < 32; ++i) {
                            if (i != 2 && i != 3) {
                                chksum = ((chksum << 15) | (chksum >> 1)) + bufp[i];
                            }
                        }

                        for (; i < file_info_size; ++i) {
                            chksum = ((chksum << 15) | (chksum >> 1)) + bufp[i];
                        }

                        if (chksum == m1->checksum) {
                            std::string fname;
                            try {
                                fname = _get_utf8_filename(bufp, m2->name_length);
                            } catch (std::exception &e) {
                                std::cerr << e.what() << std::endl;
                                fname = "ERR";
                            }
                            log << "FDE " << std::setfill('0') << std::setw(16) << std::hex << buffer_offset + (bufp - buffer) << ' ' << fname << std::endl;
                            std::cout << "FDE " << std::setfill('0') << std::setw(16) << std::hex << buffer_offset + (bufp - buffer) << ' ' << fname << std::endl;
                            bufp += file_info_size - 1;
                        }
                    }
                }
            }

            ++bufp;

            const size_t buf_used = bufp - buffer;
            if (buf_used >= 0xF0000 && !eof) {
                ssize_t buf_left = sizeof(buffer) - buf_used;
                memmove(buffer, bufp, buf_left);
                bufp = buffer;
                buffer_offset += buf_used;
                ssize_t to_fill = sizeof(buffer) - buf_left;

                try {
                    dev.read((char*)(buffer + buf_left), to_fill);
                    auto state = dev.rdstate();
                    if (state & std::ifstream::goodbit) {
                        //allgood
                    } else if (state & std::ifstream::eofbit) {
                        std::cout << "EOF" << std::endl;
                        eof = true;
                    } else if ((state & std::ifstream::failbit) || (state & std::ifstream::badbit)) {
                        to_fill += buffer_offset % 512;
                        buf_left -= buffer_offset % 512;
                        size_t read_offset = buffer_offset + buf_left; // round down to sector

                        while (to_fill > 0) {
                            if (dev.fail() || dev.bad()) {
                                log << "BAD_SECTOR " << std::setfill('0') << std::setw(16) << std::hex << read_offset << std::endl;
                                log.flush();
                                std::cout << "BAD_SECTOR " << std::setfill('0') << std::setw(16) << std::hex << read_offset << std::endl;
                            }

                            dev.clear();
                            dev.seekg(read_offset);
                            dev.read((char*)(buffer + buf_left), to_fill < 512 ? to_fill : 512);
                            buf_left += 512;
                            to_fill -= 512;
                            read_offset += 512;
                        }
                    }
                } catch (std::exception &e) {
                    std::cerr << "ioexception: " << e.what() << std::endl;
                }
            }
        }
        log.close();
    }
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

#endif /* _io_github_paulyc_recoverylog_hpp_ */
