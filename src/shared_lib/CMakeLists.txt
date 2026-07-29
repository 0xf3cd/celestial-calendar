cmake_minimum_required(VERSION 3.22)

# Find all source files to build the shared library
file(GLOB SOURCES "lib*.cpp")

# Set the version dynamically from an environment variable if available
if(DEFINED ENV{BUILD_VERSION})
  set(VERSION_NUMBER $ENV{BUILD_VERSION})
else()
  set(VERSION_NUMBER "0.0.0") # Default version if the env var is not set
endif()

message(STATUS "Building version ${VERSION_NUMBER}")

add_library(celestial_calendar SHARED ${SOURCES})

set_target_properties(celestial_calendar PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

set_target_properties(celestial_calendar PROPERTIES VERSION ${VERSION_NUMBER})

# #67: compile-check the published C header as pure C (layouts pinned by `_Static_assert`
# inside). Built as part of the default target, so CI exercises it on every platform.
add_library(c_header_check OBJECT c_header_check.c)
set_target_properties(c_header_check PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
