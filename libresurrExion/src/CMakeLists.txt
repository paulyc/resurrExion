#
# resurrExion - recover damaged ExFAT filesystem
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

add_library(resurrExion SHARED
    libmain.cpp
    ../../config/fsconfig.hpp
    entity.hpp
    entity.cpp
    exfat_structs.hpp
    exception.hpp
    exception.cpp
    filesystem.hpp
    journal.hpp
    logger.hpp
    logger.cpp
    recoverylog.hpp
    recoverylog.cpp
    types.hpp
    types.cpp
    database.cpp
    database.hpp
    quick.hpp
    quick.cpp
    quickentity.cpp
    quickentity.hpp
    quickfs.cpp
    quickfs.hpp
    crc32.cpp
    crc32.hpp
    postgres.cpp
    postgres.hpp
)

#set_source_files_properties(${LIB_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
target_include_directories(resurrExion PUBLIC /usr/include/mysql)
target_link_libraries(resurrExion -lmysqlpp)
