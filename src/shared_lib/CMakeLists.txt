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

# #91: the export set is exactly the entry points declared in celestial.h. Both halves
# are required — hiding without CELESTIAL_API hides the entries too; the macro without
# hiding exports internals as well (WINDOWS_EXPORT_ALL_SYMBOLS did the latter).
set_target_properties(celestial_calendar PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)

# Private to the library build: flips CELESTIAL_API from dllimport to dllexport on
# Windows. Consumers of celestial.h need no defines.
target_compile_definitions(celestial_calendar PRIVATE CELESTIAL_BUILDING_DLL)

# Surviving exports after the hiding (#91's exception list, human-facing copy; the
# machine-readable source is SURVIVOR_EXCEPTIONS in automation/export_surface.py, empty
# today): every remaining export is a weak/unique vague-linkage symbol from the standard
# library — namespace std carries an explicit default-visibility attribute that overrides
# -fvisibility=hidden, and cross-DSO coalescing of those symbols needs them visible.

# SOVERSION tracks major.minor while 0.x: a 0.x minor may break the ABI, so SOVERSION=0
# would falsely claim all 0.x releases mutually compatible. Trigger: at the 1.0 release,
# switch to major-only — from then on the ABI holds within a major, which is exactly the
# promise SOVERSION makes.
string(REGEX MATCH "^[0-9]+\\.[0-9]+" SOVERSION_NUMBER "${VERSION_NUMBER}")

set_target_properties(celestial_calendar PROPERTIES
  VERSION ${VERSION_NUMBER}
  SOVERSION ${SOVERSION_NUMBER}
)

# Release packaging runs `cmake --install` (build_and_test.yml): the real file and the
# SOVERSION symlinks land by CMake's rules, replacing the hand-rolled `-type f` copy that
# shipped only the real file. All three destinations are required — the Windows .dll goes
# to RUNTIME, its import .lib to ARCHIVE, and without ARCHIVE no .lib is installed.
# No COMPONENT split: the release shape is a single zip, there is no per-component
# audience (package config / find_package is deliberately not offered — no consumer).
install(TARGETS celestial_calendar
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(FILES celestial.h DESTINATION include)

# #67: compile-check the published C header as pure C (layouts pinned by `_Static_assert`
# inside). Built as part of the default target, so CI exercises it on every platform.
add_library(c_header_check OBJECT c_header_check.c)
set_target_properties(c_header_check PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
