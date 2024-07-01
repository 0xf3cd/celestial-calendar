cmake_minimum_required(VERSION 3.22)
project(cpp-calendar)

set(CMAKE_CXX_STANDARD 23)
add_compile_options(-std=c++2b)
add_compile_options(-Wall -Wextra -Wpedantic)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

include_directories(${PROJECT_SOURCE_DIR}/common)
include_directories(${PROJECT_SOURCE_DIR}/calendar)
include_directories(${PROJECT_SOURCE_DIR}/util)

add_subdirectory(test)
