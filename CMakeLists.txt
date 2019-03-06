
cmake_minimum_required(VERSION 3.6)
project(ExFATRestore)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -D_DEBUG")
set(CMAKE_CXX_STANDARD 17)

include_directories(src)

#find_library(FOUND_LOG4CPLUS log4cplus)
find_library(FOUND_CPPUNIT cppunit)

add_executable(
	ExFATRestore

	src/exception.cpp
	src/logger.cpp
	src/main.cpp
)

target_link_libraries(ExFATRestore pthread) #log4cplus)

if(FOUND_CPPUNIT)
add_executable(
	ExFATRestoreTest

	src/logger.cpp
	test/logger.cpp
	test/main.cpp
)

target_link_libraries(ExFATRestoreTest cppunit pthread)
endif()
