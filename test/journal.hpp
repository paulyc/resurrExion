//
//  filesystem.hpp - Write journal tests
//  ExFATRestore
//
//  Created by Paul Ciarlo on 19 March 2019.
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

#ifndef _io_github_paulyc_journal_hpp_test_
#define _io_github_paulyc_journal_hpp_test_

#include <cppunit/extensions/HelperMacros.h>

#include "../src/logger.hpp"
#include "../src/journal.hpp"

using namespace io::github::paulyc;

class JournalTest : public CppUnit::TestFixture,
                    public Loggable
{
    CPPUNIT_TEST_SUITE(JournalTest);
    CPPUNIT_TEST(testAddDiff);
    CPPUNIT_TEST(testCommit);
    CPPUNIT_TEST(testRollback);
    CPPUNIT_TEST_SUITE_END();

    TransactionJournal _journal;
    uint8_t _test_buffer[1024] = {0};

public:
    JournalTest() : _journal(_test_buffer) {}

    void setUp() { logf(INFO, "JournalTest::setUp()\n"); }
    void tearDown() { logf(INFO, "JournalTest::tearDown()\n"); }

    void testAddDiff() {
        logf(INFO, "JournalTest::testAddDiff\n");
    }

    void testCommit() {
        logf(INFO, "JournalTest::testCommit\n");
    }

    void testRollback() {
        logf(INFO, "JournalTest::testRollback\n");
    }
};

#endif /* _io_github_paulyc_journal_hpp_test_ */
