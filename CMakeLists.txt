cmake_minimum_required(VERSION 3.10)
project(TinyRpc)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(MUDUO_ROOT "/home/xinyu/muduo-master")
set(MUDUO_LIB ${MUDUO_ROOT}/build/lib)

include_directories(${MUDUO_ROOT})

link_directories(${MUDUO_LIB})

find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

set(LIBS
    protobuf
    pthread
    muduo_net
    muduo_base
)

add_subdirectory(src)
add_subdirectory(example)
