cmake_minimum_required(VERSION 3.22)
project(celestial-calendar)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compile options for different compilers
add_compile_options(-Wall -Wextra -Werror -Wpedantic -Wnull-dereference -Wunreachable-code)

# Optimization is `CMAKE_BUILD_TYPE`'s job: `CMAKE_CXX_FLAGS` lands before the build type's own
# flags and silently loses to them. This file used to set `-O2`; Release compiled at `-O3` (#72).

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
