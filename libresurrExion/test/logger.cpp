//
//  logger.cpp - Console/file/syslog logging interface tests
//  ExFATRestore
//
//  Created by Paul Ciarlo on 5 March 2019.
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

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "logger.hpp"
using io::github::paulyc::Loggable;

BOOST_AUTO_TEST_SUITE(LoggerTestSuite)

BOOST_AUTO_TEST_CASE(TestLogger)
{
	Loggable l;
	l.logf(Loggable::INFO, "%016llx\n", 10);
	l.logf(Loggable::INFO, "%08lx\n", 10);
	l.logf(Loggable::INFO, "Testing logger %s\n", "something else");
}

BOOST_AUTO_TEST_SUITE_END()
