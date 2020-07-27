//
//  filetype.hpp
//  resurrExion
//
//  Created by Paul Ciarlo on 27 July 2019
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

#ifndef RESURREX_FILETYPE_HPP
#define RESURREX_FILETYPE_HPP

#include <stdint.h>
#include <stddef.h>
#include <codecvt>
#include <locale>

class filetype
{
public:
    enum Type { flac, aiff, wave, mp4, aac, mp3,
                jpg, avi, mov, mkv, text, tif, gif, png,
                html, s3d, dds, eqg, ogg, ams, asg, adg, exe,
                torrent, obj, elf, unknown };
    filetype();
    ~filetype() {}
    Type identify_cluster(const char *data, size_t len);
private:
    //std::wstring_convert<std::codecvt<char16_t, char, std::mbstate_t>> _conv16;
    //std::wstring_convert<std::codecvt<char32_t, char, std::mbstate_t>> _conv32;
};

#endif // RESURREX_FILETYPE_HPP
