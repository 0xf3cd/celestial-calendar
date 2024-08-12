cmake_minimum_required(VERSION 3.22)
project(test)

set(CMAKE_CXX_STANDARD 23)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

add_compile_definitions(TESTING=1)
add_compile_definitions(DEBUG=1)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Suppress warnings from GTest
target_compile_options(gtest PRIVATE -w)
target_compile_options(gtest_main PRIVATE -w)
target_compile_options(gmock PRIVATE -w)
target_compile_options(gmock_main PRIVATE -w)

enable_testing()
include(GoogleTest)

macro(ADD_TEST test_path test_name)
  message("Adding ${test_name}")
  add_executable(${test_name} ${test_path})
  target_link_libraries(
    ${test_name}
    GTest::gtest_main
  )
  gtest_discover_tests(${test_name})
endmacro()

file(GLOB_RECURSE SRC_PATHS "*.cpp")

foreach(src_path IN LISTS SRC_PATHS)
  get_filename_component(test_name ${src_path} NAME_WE)
  message("Found test: ${src_path}")
  ADD_TEST(${src_path} ${test_name})
endforeach()
