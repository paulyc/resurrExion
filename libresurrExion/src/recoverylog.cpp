//
//  recoverylog.cpp
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

#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>

#include <sstream>
#include <iomanip>
#include <regex>

#if 0
using namespace github::paulyc::resurrExion;

template <typename Filesystem_T>
void RecoveryLogTextWriter<Filesystem_T>::writeTextLog(
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
#endif
