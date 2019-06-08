#
# ExFATRestore - recover damaged ExFAT filesystem
#
# Copyright (C) 2019 Paul Ciarlo <paul.ciarlo@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

set(TEST_HEADERS
	filesystem.hpp
	journal.hpp
	logger.hpp
)

set(TEST_SOURCES
	filesystem.cpp
	journal.cpp
	logger.cpp
	main.cpp
)

add_executable(
        testresurrExion
	${TEST_HEADERS}
	${TEST_SOURCES}
)

target_include_directories(testresurrExion PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(testresurrExion resurrExion ${Boost_LIBRARIES})
