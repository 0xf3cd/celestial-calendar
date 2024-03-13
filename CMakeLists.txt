cmake_minimum_required(VERSION 3.22)
project(cpp-calendar)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-std=c++20)
add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_subdirectory(test)
