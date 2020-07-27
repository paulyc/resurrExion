//
//  filetype.cpp
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

#include "filetype.hpp"

filetype::filetype()
{

}


filetype::Type filetype::identify_cluster(const char *data, size_t len) {
    int ascii_chars = 0;
    int utf8_chars = 0;
    int utf16_chars = 0;
    int16_t s16sum = 0;
   // _conv16.from_bytes(data, data+len);
    static constexpr uint32_t flac_stream = 'CaLf'; //'fLaC';
    static constexpr uint16_t flac_frame_hdr = 0xFFF8;//0b1111111111111000;

    for (size_t i = 0; i < len; ++i) {
        if ((data[i] & 1) == 0) {
            s16sum += *(int16_t*)&data[i];
        }
    }
}
