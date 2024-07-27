cmake_minimum_required(VERSION 3.22)
project(celestial-calendar)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compile options for different compilers
add_compile_options(-Wall -Wextra -Wpedantic -Wnull-dereference -Wunreachable-code)

if(NOT WIN32) # Clang++ on Windows compalins about GTest codes.
  add_compile_options(-Werror)
endif()

# Optimization flag
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/common)
include_directories(${PROJECT_SOURCE_DIR}/astro)
include_directories(${PROJECT_SOURCE_DIR}/calendar)
include_directories(${PROJECT_SOURCE_DIR}/util)

# Add subdirectories
add_subdirectory(test)
add_subdirectory(shared_lib)
