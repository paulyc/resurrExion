//
//  crc32.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 25 July 2020
//
//  Copyright (C) 2016-2019 Alex I. Kuznetsov
//  Copyright (C) 2020 Paul Ciarlo <paul.ciarlo@gmail.com>
//
//  See https://lxp32.github.io/docs/a-simple-example-crc32-calculation/
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

#include "crc32.hpp"

crc32::crc32()
{
    for(uint32_t i=0;i<256;i++) {
        uint32_t ch=i;
        uint32_t crc=0;
        for(size_t j=0;j<8;j++) {
            uint32_t b=(ch^crc)&1;
            crc>>=1;
            if(b) crc=crc^0xEDB88320;
            ch>>=1;
        }
        _table[i]=crc;
    }
}

uint32_t crc32::compute(const char *s,size_t n) {
    uint32_t crc=0xFFFFFFFF;

    for(size_t i=0;i<n;i++) {
        char ch=s[i];
        uint32_t t=(ch^crc)&0xFF;
        crc=(crc>>8)^_table[t];
    }

    return ~crc;
}
