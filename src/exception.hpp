//
//  exception.hpp
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

#ifndef _io_github_paulyc_exception_hpp_
#define _io_github_paulyc_exception_hpp_

#include <string>
#include <iostream>
#include <exception>
#include <system_error>

namespace io {
namespace github {
namespace paulyc {

inline void assertBreakpoint(bool test, std::string msg)
{
    if (!test) {
        std::cerr << "Breakpoint assertion failed: " << msg << std::endl;
        asm("int $3");
    }
}

}
}
}

#define _io_github_paulyc_STRINGIFY(x) #x

#ifdef _DEBUG

#define ASSERT_BP(test, msg) io::github::paulyc::assertBreakpoint((test), (msg))

#define ASSERT_THROW(assertion) do { \
if ((bool)(assertion) == false) { \
throw std::runtime_error( _io_github_paulyc_STRINGIFY(assertion) ); \
}} while (0);

#else /* _DEBUG */

#define ASSERT_BP(test, msg)
#define ASSERT_THROW(assertion)

#endif /* _DEBUG */

#endif /* _io_github_paulyc_exception_hpp_ */
