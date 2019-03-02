//
//  filesystem.cpp
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

#include "filesystem.hpp"

/**
 * if valid, return bytes in entry
 * if invalid, return -1
 */
bool io::github::paulyc::ExFATRestore::ExFATFilesystem::verifyFileEntry(
    uint8_t *buf,
    size_t bufSize,
    int &file_info_size,
    struct fs_file_directory_entry *&m1,
    struct fs_stream_extension_directory_entry *&m2)
{
    if (buf[0] == FILE_INFO1 && buf[32] == FILE_INFO2) {
        m1 = (struct fs_file_directory_entry*)(buf);
        m2 = (struct fs_stream_extension_directory_entry*)(buf+32);
        const int continuations = m1->continuations;
        if (continuations >= 2 && continuations <= 18) {
            int i;
            file_info_size = (continuations + 1) * 32;
            uint16_t chksum = 0;
            if (bufSize < file_info_size) {
                return false;
            }

            for (i = 0; i < 32; ++i) {
                if (i != 2 && i != 3) {
                    chksum = ((chksum << 15) | (chksum >> 1)) + buf[i];
                }
            }

            for (; i < file_info_size; ++i) {
                chksum = ((chksum << 15) | (chksum >> 1)) + buf[i];
            }

            if (chksum == m1->checksum) {
                return file_info_size;
            }
        }
    }

    return false;
}
