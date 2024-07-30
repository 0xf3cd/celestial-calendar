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

set_target_properties(celestial_calendar PROPERTIES VERSION ${VERSION_NUMBER})
