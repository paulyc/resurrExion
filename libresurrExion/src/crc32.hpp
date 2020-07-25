//
//  crc32.hpp
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

#ifndef CRC32_HPP
#define CRC32_HPP

#include <stdint.h>
#include <stddef.h>

class crc32
{
public:
    crc32();
    uint32_t compute(const char *s,size_t n);
private:
    uint32_t _table[256];
};

#endif // CRC32_HPP
