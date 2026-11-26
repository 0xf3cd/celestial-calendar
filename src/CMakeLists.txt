# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)

if(CMAKE_HOST_WIN32 AND CMAKE_GENERATOR STREQUAL "Unix Makefiles"
    AND NOT CMAKE_TOOLCHAIN_FILE
    AND NOT CMAKE_PROJECT_INCLUDE_BEFORE
    AND NOT CMAKE_PROJECT_celestial-calendar_INCLUDE_BEFORE)
  # Strawberry Perl's gmake can fail during recursive output sync; prefer the installed make.
  # https://github.com/StrawberryPerl/Perl-Dist-Strawberry/issues/148
  find_program(CMAKE_MAKE_PROGRAM NAMES make)
endif()

project(celestial-calendar)
message(STATUS "build program: ${CMAKE_MAKE_PROGRAM}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Retained material boundary (T03): Release DLLs statically include Microsoft runtime portions. Those
# portions remain under the selected Visual Studio terms and outside the project MIT grant.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Add compile options for different compilers
add_compile_options(-Wall -Wextra -Werror -Wpedantic -Wnull-dereference -Wunreachable-code)

# Optimization is `CMAKE_BUILD_TYPE`'s job: a flag set in `CMAKE_CXX_FLAGS` lands before the build
# type's own and silently loses to it. Debug is therefore really `-O0`, so golden values computed
# there can differ from a Release run by a ulp (#72).

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/astro)
include_directories(${PROJECT_SOURCE_DIR}/calendar)
include_directories(${PROJECT_SOURCE_DIR}/util)

# Here, not only in `test/`: without a top-level call CMake writes no `CTestTestfile.cmake` next to
# this build tree's root, so `ctest --test-dir build` finds zero tests -- and reports success (#72).
enable_testing()

# Add subdirectories. The order is for readability only — CMake binds targets at generate
# time, so `test` may reference `celestial_calendar` before its directory is processed (#67).
add_subdirectory(shared_lib)
add_subdirectory(test)
add_subdirectory(bench)
