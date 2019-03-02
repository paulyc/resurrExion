//
//  recovery.cpp
//  ExFATRestore
//
//  Created by Paul Ciarlo on 1 March 2019.
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

#include "recovery.hpp"
#include "filesystem.hpp"
#include "exception.hpp"

using namespace io::github::paulyc::ExFATRestore;

io::github::paulyc::ExFATRestore::FileRestorer::FileRestorer(
    const std::string &devname,
    const std::string &restore_dir_name) :
        _dev(devname, std::ios::binary),
        _restore_dir_name(restore_dir_name)
{
}

void io::github::paulyc::ExFATRestore::FileRestorer::restore_all_files(const std::string &textlogfilename)
{
    std::ifstream textlog(textlogfilename);

    _rlog.parseTextLog(_dev, textlog, [this](size_t offset, std::variant<std::string, std::exception, bool> entry_info){
        if (std::holds_alternative<std::string>(entry_info)) {
            // File entry
            _restore_file(offset, std::get<std::string>(entry_info));
        } else if (std::holds_alternative<bool>(entry_info)) {
            // Bad sector, don't care
        } else {
            // Exception
            std::exception &ex = std::get<std::exception>(entry_info);
            std::cerr << "entry_info had " << typeid(ex).name() << " with message: " << ex.what() << std::endl;
        }
    });
}

bool io::github::paulyc::ExFATRestore::FileRestorer::_restore_file(size_t offset, std::string filename)
{
    uint8_t buffer[0x10000];
    _dev.seekg(offset, std::ios_base::beg);
    _dev.read((char*)buffer, sizeof(buffer));

    if (_dev) {
        int file_entry_sz;
        struct fs_file_directory_entry *m1;
        struct fs_stream_extension_directory_entry *m2;
        bool valid = ExFATFilesystem::verifyFileEntry(
            buffer,
            1024,
            file_entry_sz,
            m1,
            m2
        );
        if (valid) {
            if (m2->flags & CONTIGUOUS) {
                std::ofstream restored_file(_restore_dir_name + std::string("/") + filename, std::ios_base::binary);
                const size_t file_offset = m2->start_cluster * cluster_size_bytes + cluster_heap_disk_start_sector * sector_size_bytes;
                ssize_t bytes_remaining = m2->valid_size;
                std::cout << "recovering file " << filename <<
                    " at cluster offset " << m2->start_cluster <<
                    " disk offset " << file_offset <<
                    " size " << bytes_remaining << std::endl;
                _dev.seekg(file_offset, std::ios_base::beg);

                while (bytes_remaining > 0) {
                    const size_t to_read = std::min((ssize_t)sizeof(buffer), bytes_remaining);
                    _dev.read((char*)buffer, to_read);
                    const int bytes_read = _dev.gcount();
                    bytes_remaining -= bytes_read;
                    restored_file.write((char*)buffer, bytes_read);
                }

                restored_file.close();

                return true;
            } else {
                std::cerr << "Non-contiguous file: " << filename << std::endl;
            }

        } else {
            std::cerr << "Invalid file entry at offset " << offset << std::endl;
        }
    } else {
        std::cerr << "Failed to read " << sizeof(buffer) << " bytes at offset " << offset << std::endl;
    }
    return false;
}
