cmake_minimum_required(VERSION 3.22)
project(test)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

add_compile_definitions(TESTING=1)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.tar.gz
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

macro(ADD_TEST test_name)
  message("Adding test for ${test_name}")
  add_executable(${test_name} ${test_name}.cpp)
  target_link_libraries(
    ${test_name}
    GTest::gtest_main
  )
  gtest_discover_tests(${test_name})
endmacro()

file(GLOB SRC_PATHS "*.cpp")

foreach(src_path IN LISTS SRC_PATHS)
  get_filename_component(test_name ${src_path} NAME_WE)
  ADD_TEST(${test_name})
endforeach()