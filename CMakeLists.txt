
cmake_minimum_required(VERSION 3.6)
project(ExFATRestore)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DDEBUG")
set(CMAKE_CXX_STANDARD 17)

include_directories(src)

add_executable(
	ExFATRestore

	src/exception.cpp
	src/filesystem.cpp
	src/main.cpp
	src/recoverylog.cpp
)
