
cmake_minimum_required(VERSION 3.6)
project(ExFATRestore)

set(USE_CPPUNIT TRUE)
set(USE_LOG4CPLUS FALSE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -D_DEBUG")
set(CMAKE_CXX_STANDARD 17)

if(USE_CPPUNIT)
#    add_library(cppunit SHARED IMPORTED)
    find_library(CPPUNIT_FOUND cppunit)
    if(CPPUNIT_FOUND)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CPPUNIT")
        include_directories("/usr/local/include") #${CPPUNIT_INCLUDE_DIRS})
        link_directories("/usr/local/lib") #${CPPUNIT_LIBRARIES})
    endif(CPPUNIT_FOUND)
endif(USE_CPPUNIT)

if(USE_LOG4CPLUS)
    find_library(LOG4CPLUS_FOUND log4cplus)
endif(USE_LOG4CPLUS)

include_directories(src)

set(LIB_COMPILE_SOURCES
    config/fsconfig.hpp
    src/entity.cpp
    src/entity.hpp
    src/exfat_structs.hpp
    src/exception.cpp
    src/exception.hpp
    src/filesystem.hpp
    src/journal.cpp
    src/journal.hpp
    src/logger.cpp
    src/logger.hpp
    src/recoverylog.hpp
)

set(LIB_NO_COMPILE_SOURCES
	src/filesystem.cpp
	src/recoverylog.cpp
)

add_library(
    ExFATRestore

    SHARED

    ${LIB_COMPILE_SOURCES}
    ${LIB_NO_COMPILE_SOURCES}
)

set_source_files_properties(${LIB_NO_COMPILE_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
target_link_libraries(ExFATRestore pthread)

if(LOG4CPLUS_FOUND)
    target_link_libraries(ExFATRestore log4cplus)
endif(LOG4CPLUS_FOUND)

find_package(Boost 1.58.0 COMPONENTS program_options)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    add_executable(restore src/main.cpp)
    target_link_libraries(restore ExFATRestore ${Boost_LIBRARIES})
endif()

set(TEST_COMPILE_SOURCES
	test/filesystem.hpp
	test/filesystem.cpp
	test/journal.hpp
	test/journal.cpp
	test/logger.hpp
	test/logger.cpp
	test/main.cpp
)

set(TEST_NO_COMPILE_SOURCES
)

add_executable(
	test

	${TEST_COMPILE_SOURCES}
	${TEST_NO_COMPILE_SOURCES}
)

set_source_files_properties(${TEST_NO_COMPILE_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
target_link_libraries(test ExFATRestore)

if(CPPUNIT_FOUND)
    target_link_libraries(test cppunit)
endif(CPPUNIT_FOUND)
