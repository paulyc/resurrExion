//
//  recoverylog.cpp
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

#include "filesystem.hpp"
#include "recoverylog.hpp"
#include "exception.hpp"

#include <sstream>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <regex>

#include <sys/mman.h>
#include <unistd.h>

void io::github::paulyc::ExFATRestore::RecoveryLogBase::parseTextLog(
    std::ifstream &dev,
    std::ifstream &textlog,
    std::function<void(size_t, std::variant<std::string, std::exception, bool>)> cb)
{
    std::regex fde("FDE ([0-9a-fA-F]{16})(?: (.*))?");
    std::regex badsector("BAD_SECTOR ([0-9a-fA-F]{16})");

    for (std::string line; std::getline(textlog, line); ) {
        std::smatch sm;
        if (std::regex_match(line, sm, fde)) {
            size_t offset;
            std::string filename;
            filename = sm[2];
            try {
                offset = std::stol(sm[1], nullptr, 16);
                cb(offset, std::variant<std::string, std::exception, bool>(filename));
            } catch (std::exception &ex) {
                std::cerr << "Writing file entry to binlog, got exception: " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                std::cerr << "Invalid textlog FDE line, skipping: " << line << std::endl;
                cb(0, std::variant<std::string, std::exception, bool>(ex));
            }
        } else if (std::regex_match(line, sm, badsector)) {
            int32_t bad_sector_flag;
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

void io::github::paulyc::ExFATRestore::RecoveryLogReader::parse(
	const std::string &devfilename,
    const std::string &logfilename,
    const std::string &outdir)
{
    std::ifstream dev(devfilename, std::ios_base::binary);
    std::ifstream logfile(logfilename);
    std::string line;
    size_t line_no = 0;

    while (std::getline(logfile, line)) {
        size_t node_offset, cluster_offset;
        std::string line_ident, dummy;
        std::istringstream iss(line);

        ++line_no;
        iss >> std::hex;

        if (iss >> line_ident) {
            if (line_ident == "CLUSTER") {
                if (iss >> cluster_offset >> dummy >> node_offset) {
                    std::cerr << std::hex << "Read up to cluster " << cluster_offset <<
                    	" disk offset " << node_offset << std::endl;
                } else {
                    break;
                }
            } else if (line_ident == "FDE") {
                if (iss >> node_offset) {
                    _processFDE(dev, node_offset, outdir);
                } else {
                    break;
                }
            } else if (line_ident == "EFI") {
                // just ignore this since we're loading it out of the FDE anyway
            } else if (line_ident == "EFN") {
                // just ignore this since we're loading it out of the FDE anyway
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

constexpr int exfat_filename_maxlen = 256;
constexpr int exfat_filename_maxlen_utf8 = exfat_filename_maxlen * 2;

void io::github::paulyc::ExFATRestore::RecoveryLogReader::_processFDE(
	std::ifstream &dev,
    size_t offset,
    const std::string &outdir)
{
    std::string fname;

    uint8_t buffer[4096];
    dev.seekg(offset);
    dev.read((char*)buffer, sizeof(buffer));

    struct fs_file_directory_entry *m1 = (struct fs_file_directory_entry*)(buffer);
    struct fs_stream_extension_directory_entry *m2 = (struct fs_stream_extension_directory_entry*)(buffer+32);
    const int continuations = m1->continuations;
    if (continuations >= 2 && continuations <= 18) {
        int i, file_info_size = (continuations + 1) * 32;
        uint16_t chksum = 0;

        for (i = 0; i < 32; ++i) {
            if (i != 2 && i != 3) {
                chksum = ((chksum << 15) | (chksum >> 1)) + buffer[i];
            }
        }

        for (; i < file_info_size; ++i) {
            chksum = ((chksum << 15) | (chksum >> 1)) + buffer[i];
        }

        if (chksum == m1->checksum) {
            try {
                fname = convert_utf16_to_utf8(buffer, m2->name_length);
            } catch (std::exception &e) {
                std::cerr << e.what() << std::endl;
                fname = "ERR";
            }
        } else {
            std::cerr << "bad checksum" << std::endl;
        }
    }

    if (fname.rfind(".DTS") == std::string::npos && fname.rfind(".dts") == std::string::npos) {
        std::cerr << "Skipping file " << fname << std::endl;
        return;
    }

    if (!(m2->flags & CONTIGUOUS)) {
        std::cerr << "File " << fname << " is fragmented" << std::endl;
        return;
    }

    std::string outpath = outdir + "/" + fname;
    std::ofstream outfile(outpath, std::ios::binary);

    std::cout << "Saving file " << fname << " (size " << m2->size <<
        ") to " << outpath << std::endl;

    char bytes[cluster_size_bytes];
    size_t bytes_left = m2->size;
    // this calculation might be wrong but we can't get it without the file info entries
    size_t file_offset = (start_offset_cluster + m2->start_cluster) * cluster_size_bytes;
    dev.seekg(file_offset);
    while (bytes_left > 0) {
        size_t bytes_to_read = std::min(bytes_left, sizeof(bytes));
        dev.read(bytes, bytes_to_read);
        bytes_left -= bytes_to_read;
        outfile.write(bytes, bytes_to_read);
    }
    outfile.close();

    std::cout << "Done saving file " << fname << std::endl;
}

std::string io::github::paulyc::ExFATRestore::RecoveryLogBase::convert_utf16_to_utf8(uint8_t *fh, int namelen)
{
    struct fs_file_directory_entry *m1 = (struct fs_file_directory_entry *)fh;
    const int continuations = m1->continuations;
    std::basic_string<char16_t> u16s;
    for (int c = 1; c <= continuations; ++c) {
        fh += 32;
        if (fh[0] == FILE_NAME) {
            struct fs_file_name_entry *n = (struct fs_file_name_entry*)fh;
            for (int i = 0; i < exfat_name_entry_size; ++i) {
                if (u16s.length() == namelen) {
                    return _cvt.to_bytes(u16s);
                } else {
                    u16s += n->name[i];
                }
            }
        }
    }
    return _cvt.to_bytes(u16s);
}

void io::github::paulyc::ExFATRestore::RecoveryLogWriter::writeLog(
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
        //if (io::github::paulyc::ExFATRestore::ExFATFilesystem::verifyFileEntry(bufp, bufend - bufp)) {
        //}

        if (bufp[0] == FILE_INFO1) {
            if (bufp[32] == FILE_INFO2 && bufp[64] == FILE_NAME) {
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
                            fname = convert_utf16_to_utf8(bufp, m2->name_length);
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

constexpr int32_t io::github::paulyc::ExFATRestore::RecoveryLogWriter::BadSectorFlag;

void io::github::paulyc::ExFATRestore::RecoveryLogWriter::_writeBinlogFileEntry(
    std::ifstream &dev,
    size_t offset,
    std::ofstream &binlog)
{
    uint8_t buffer[1024];
    int32_t file_entry_sz;
    struct fs_file_directory_entry *m1;
    struct fs_stream_extension_directory_entry *m2;

    dev.seekg(offset, std::ios_base::beg);
    dev.read((char*)buffer, sizeof(buffer));

    if (dev) {
        bool valid = io::github::paulyc::ExFATRestore::ExFATFilesystem::verifyFileEntry(
            buffer,
            sizeof(buffer),
            file_entry_sz,
            m1,
            m2);

        if (valid) {
            BP_ASSERT(file_entry_sz >= 96 && file_entry_sz <= 1024, "Invalid file entry size");
            binlog.write((const char *)&offset, sizeof(size_t));
            binlog.write((const char *)&file_entry_sz, sizeof(int32_t));
            binlog.write((const char*)buffer, file_entry_sz);
        } else {
            std::cerr << "Invalid file entry at offset " << offset << std::endl;
        }
    } else {
        std::cerr << "Failed to read " << sizeof(buffer) << " bytes at offset " << offset << std::endl;
    }
}

/**
 * binlog format:
 * 64-bit unsigned int, disk offset of byte in record or sector
 * 32-bit int, number of bytes in record, OR -1 if bad sector
 * variable bytes equal to number of bytes in record
 */
void io::github::paulyc::ExFATRestore::RecoveryLogWriter::textLogToBinLog(
    const std::string &devfilename,
    const std::string &textlogfilename,
    const std::string &binlogfilename)
{
    std::ifstream dev(devfilename, std::ios::binary);
    std::ifstream textlog(textlogfilename);
    std::ofstream binlog(binlogfilename, std::ios::binary | std::ios::trunc);

#if 1
    parseTextLog(dev, textlog, [this, &binlog, &dev](size_t offset, std::variant<std::string, std::exception, bool> entry_info) {
        if (std::holds_alternative<std::string>(entry_info)) {
            // File entry
            _writeBinlogFileEntry(dev, offset, binlog);
        } else if (std::holds_alternative<bool>(entry_info)) {
            // Bad sector
            binlog.write((const char *)&offset, sizeof(size_t));
            binlog.write((const char *)&BadSectorFlag, sizeof(int32_t));
        } else {
            // Exception
            std::exception &ex = std::get<std::exception>(entry_info);
            std::cerr << "entry_info had " << typeid(ex).name() << " with message: " << ex.what() << std::endl;
        }
    });
#else
    std::regex fde("FDE ([0-9a-fA-F]{16})(?: (.*))?");
    std::regex badsector("BAD_SECTOR ([0-9a-fA-F]{16})");

    for (std::string line; std::getline(textlog, line); ) {
        std::smatch sm;
        if (std::regex_match(line, sm, fde)) {
            size_t offset;
            std::string filename;
            filename = sm[2];
            try {
                offset = std::stol(sm[1], nullptr, 16);
                _writeBinlogFileEntry(dev, offset, binlog);
            } catch (std::exception &ex) {
                std::cerr << "Writing file entry to binlog, got exception: " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                std::cerr << "Invalid textlog FDE line, skipping: " << line << std::endl;
            }
        } else if (std::regex_match(line, sm, badsector)) {
            int32_t bad_sector_flag;
            size_t offset;
            try {
                offset = std::stol(sm[1], nullptr, 16);
                binlog.write((const char *)&offset, sizeof(size_t));
                binlog.write((const char *)&BadSectorFlag, sizeof(int32_t));
            } catch (std::exception &ex) {
                std::cerr << "Writing bad sector to binlog, got exception " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                std::cerr << "Invalid textlog BAD_SECTOR line, skipping: " << line << std::endl;
            }
        } else {
            std::cerr << "Unknown textlog line format: " << line << std::endl;
        }
    }
#endif
}
