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
# The release asset, not `archive/refs/tags/...`: GitHub generates archive tarballs on request and
# has changed their compression before, which moves the hash for an unchanged tag. An uploaded
# asset is a fixed file, so the hash below stays true (#72).
FetchContent_Declare(
  googletest
  URL      https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
  URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Keep googletest's own install rules out of the release: `cmake --install` is the
# packaging path (build_and_test.yml), and the default INSTALL_GTEST would otherwise
# drop gtest/gmock headers, static libs, and cmake configs into the release zip.
set(INSTALL_GTEST OFF)
FetchContent_MakeAvailable(googletest)

# Suppress warnings from GTest
target_compile_options(gtest PRIVATE -w)
target_compile_options(gtest_main PRIVATE -w)
target_compile_options(gmock PRIVATE -w)
target_compile_options(gmock_main PRIVATE -w)

# Kept alongside the top-level call (#72): this directory declares its own `project()`, and
# configuring it on its own still succeeds -- only configuring, since the `celestial_calendar`
# target it links against lives one level up.
enable_testing()
include(GoogleTest)

set(ERFA_PROVENANCE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/provenance/erfa/v2.0.1/src)
set(ERFA_CALENDAR_ORACLE_SOURCES
  ${ERFA_PROVENANCE_SRC_DIR}/cal2jd.c
  ${ERFA_PROVENANCE_SRC_DIR}/jd2cal.c
)

# Named `CELESTIAL_ADD_TEST`, not `ADD_TEST`: CMake command names are case-insensitive, and command
# definitions are global once executed — a macro called `ADD_TEST` would shadow the built-in
# `add_test` for everything processed afterwards (including sibling directories). (#89)
macro(CELESTIAL_ADD_TEST test_path test_name)
  message("Adding ${test_name}")
  add_executable(${test_name} ${test_path})
  if("${test_name}" STREQUAL "julian_day_erfa_test")
    target_sources(${test_name} PRIVATE ${ERFA_CALENDAR_ORACLE_SOURCES})
    target_include_directories(${test_name} SYSTEM PRIVATE ${ERFA_PROVENANCE_SRC_DIR})
    if(MSVC)
      set_source_files_properties(${ERFA_CALENDAR_ORACLE_SOURCES} PROPERTIES COMPILE_OPTIONS "/w")
    else()
      set_source_files_properties(${ERFA_CALENDAR_ORACLE_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
    endif()
  endif()
  target_link_libraries(
    ${test_name}
    GTest::gtest_main
  )
  # #67: tests under `test/shared_lib/` drive the real shared library across the `extern "C"` boundary.
  if("${test_path}" MATCHES "/shared_lib/")
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
  # rise_set_test holds a test whose failure MODE is a hang (it pins the golden-section
  # iteration cap: without the cap the loop never terminates). Give its tests a bounded
  # TIMEOUT so that shape fails red instead of stalling the gate — no other suite needs one
  # (R5 P3-6). `set_tests_properties` cannot name the test here: discovery happens at build
  # time, after configure.
  if("${test_name}" STREQUAL "rise_set_test")
    gtest_discover_tests(${test_name} DISCOVERY_TIMEOUT 180 PROPERTIES TIMEOUT 120)
  else()
    gtest_discover_tests(${test_name} DISCOVERY_TIMEOUT 180) # Increase discovery timeout for docker builds
  endif()
endmacro()

file(GLOB_RECURSE SRC_PATHS "*.cpp")

foreach(src_path IN LISTS SRC_PATHS)
  get_filename_component(test_name ${src_path} NAME_WE)
  message("Found test: ${src_path}")
  CELESTIAL_ADD_TEST(${src_path} ${test_name})
endforeach()
