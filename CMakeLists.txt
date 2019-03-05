
cmake_minimum_required(VERSION 3.6)
project(ExFATRestore)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -D_DEBUG")
set(CMAKE_CXX_STANDARD 17)

include_directories(src)

#find_library(FOUND_LOG4CPLUS log4cplus)

add_executable(
	ExFATRestore

	src/exception.cpp
	src/logger.cpp
	src/main.cpp
)

#target_link_libraries(ExFATRestore log4cplus)
