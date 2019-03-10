
cmake_minimum_required(VERSION 3.6)
project(ExFATRestore)

set(USE_CPPUNIT FALSE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -D_DEBUG")
set(CMAKE_CXX_STANDARD 17)

include_directories(src)

#find_library(FOUND_LOG4CPLUS log4cplus)

set(NO_COMPILE_SOURCES
	src/entity.hpp
	src/exfat_structs.hpp
	src/filesystem.hpp
	src/filesystem.cpp
	src/logger.hpp
	src/recoverylog.hpp
	src/recoverylog.cpp
)

add_executable(
	ExFATRestore

	src/entity.cpp
	src/exception.cpp
	src/logger.cpp
	src/main.cpp
	${NO_COMPILE_SOURCES}
)

set_source_files_properties(${NO_COMPILE_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)

target_link_libraries(ExFATRestore pthread) #log4cplus)

if(USE_CPPUNIT)
find_library(FOUND_CPPUNIT cppunit)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CPPUNIT")
endif()

add_executable(
	ExFATRestoreTest

	src/logger.cpp
	test/logger.cpp
	test/main.cpp
)

if(USE_CPPUNIT)
target_link_libraries(ExFATRestoreTest cppunit pthread)
else()
target_link_libraries(ExFATRestoreTest pthread)
endif()
