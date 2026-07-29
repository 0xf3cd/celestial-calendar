cmake_minimum_required(VERSION 3.22)
project(test)

set(CMAKE_CXX_STANDARD 23)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

# #89: The default build type is Release (`automation/build.py`), which defines NDEBUG and would
# turn every `assert` in the library headers into dead code — even inside test binaries. Strip
# NDEBUG for test targets so internal-invariant asserts stay verifiable; production targets
# (`shared_lib`) keep NDEBUG untouched.
if (MSVC)
  add_compile_options(/UNDEBUG)
else()
  add_compile_options(-UNDEBUG)
endif()

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

# Named `CELESTIAL_ADD_TEST`, not `ADD_TEST`: CMake command names are case-insensitive, and command
# definitions are global once executed — a macro called `ADD_TEST` would shadow the built-in
# `add_test` for everything processed afterwards (including sibling directories). (#89)
macro(CELESTIAL_ADD_TEST test_path test_name)
  message("Adding ${test_name}")
  add_executable(${test_name} ${test_path})
  target_link_libraries(
    ${test_name}
    GTest::gtest_main
  )
  # #67: the C-ABI smoke test drives the real shared library across the `extern "C"` boundary.
  if(test_name STREQUAL "cabi_smoke_test")
    target_link_libraries(${test_name} celestial_calendar)
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/shared_lib)
    # Windows has no rpath — the test executable must find the DLL next to itself.
    if(WIN32)
      add_custom_command(
        TARGET ${test_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:celestial_calendar> $<TARGET_FILE_DIR:${test_name}>
      )
    endif()
  endif()
  gtest_discover_tests(${test_name} DISCOVERY_TIMEOUT 180) # Increase discovery timeout for docker builds
endmacro()

file(GLOB_RECURSE SRC_PATHS "*.cpp")

foreach(src_path IN LISTS SRC_PATHS)
  get_filename_component(test_name ${src_path} NAME_WE)
  message("Found test: ${src_path}")
  CELESTIAL_ADD_TEST(${src_path} ${test_name})
endforeach()
