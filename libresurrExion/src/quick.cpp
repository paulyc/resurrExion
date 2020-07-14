//
//  quick.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 14 July 2020
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

#include "quick.hpp"

std::string get_utf8_filename(exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde)
{
    const size_t entry_count = fde->continuations + 1;
    // i dont know that this means anything without decoding the string because UTF-16 is so dumb re: the "emoji problem"
    const size_t namelen = sde->name_length;
    struct exfat::file_name_entry_t *n = reinterpret_cast<struct exfat::file_name_entry_t*>(fde);
    std::basic_string<char16_t> u16s;
    for (size_t c = 2; c < entry_count && u16s.length() <= namelen; ++c) {
        if (n[c].type == exfat::FILE_NAME) {
            for (size_t i = 0; i < exfat::file_name_entry_t::FS_FILE_NAME_ENTRY_SIZE && u16s.length() <= namelen; ++i) {
                u16s.push_back((char16_t)n[c].name[i]);
            }
        }
    }
    return cvt.to_bytes(u16s);
}

Entity * make_entity(byteofs_t offset, exfat::file_directory_entry_t *fde, const std::string &suggested_name) {
    if (fde->attribute_flags & exfat::DIRECTORY) {
        Directory *newdir = new Directory(offset, fde);
        //if (newdir->get_parent() == RootDirectory.get()) {
        //    RootDirectory->add_child(newdir);
        //}
        newdir->load_name(suggested_name);

        return newdir;
    } else {
        File *newfile = new File(offset, fde);
        //if (newfile->get_parent() == RootDirectory.get()) {
        //    RootDirectory->add_child(newfile);
        //}
        newfile->load_name(suggested_name);
        return newfile;
    }
}

