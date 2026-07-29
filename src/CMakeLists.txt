cmake_minimum_required(VERSION 3.22)
project(celestial-calendar)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compile options for different compilers
add_compile_options(-Wall -Wextra -Werror -Wpedantic -Wnull-dereference -Wunreachable-code)

# Optimization flag
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/common)
include_directories(${PROJECT_SOURCE_DIR}/astro)
include_directories(${PROJECT_SOURCE_DIR}/calendar)
include_directories(${PROJECT_SOURCE_DIR}/util)

# Add subdirectories
# `shared_lib` first: the C-ABI smoke test links the `celestial_calendar` target (#67),
# and `target_link_libraries` only binds a real target if it is already defined.
add_subdirectory(shared_lib)
add_subdirectory(test)
