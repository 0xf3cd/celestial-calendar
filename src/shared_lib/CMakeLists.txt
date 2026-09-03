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

# Capture the original Windows final link. The launcher executes that command once;
# /VERBOSE changes lld-link diagnostics only and the evidence stays outside install artifacts.
if(WIN32 AND DEFINED ENV{CELESTIAL_WINDOWS_EVIDENCE_DIR})
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  if(NOT DEFINED ENV{CELESTIAL_WINDOWS_EVIDENCE_PRODUCER})
    message(FATAL_ERROR "CELESTIAL_WINDOWS_EVIDENCE_PRODUCER is required")
  endif()
  set_property(TARGET celestial_calendar PROPERTY CXX_LINKER_LAUNCHER
    "${Python3_EXECUTABLE};${CMAKE_CURRENT_SOURCE_DIR}/../../toolbox/windows_toolchain_evidence.py;link;--evidence-dir;$ENV{CELESTIAL_WINDOWS_EVIDENCE_DIR};--producer;$ENV{CELESTIAL_WINDOWS_EVIDENCE_PRODUCER};--"
  )
  target_link_options(celestial_calendar PRIVATE "LINKER:/VERBOSE")
endif()

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
# today): every remaining export is a C++ vague-linkage artifact — a weak/unique symbol
# from namespace std (std carries an explicit default-visibility attribute that overrides
# -fvisibility=hidden, and cross-DSO coalescing needs those visible) or anonymous RTTI
# for a builtin/function type (no symbol a consumer could link against).

# SOVERSION tracks major.minor while 0.x: a 0.x minor may break the ABI, so SOVERSION=0
# would falsely claim all 0.x releases mutually compatible. From 1.0 onward the ABI holds
# within a major, which is exactly the promise a major-only SOVERSION makes.
if(VERSION_NUMBER VERSION_LESS "1.0.0")
  string(REGEX MATCH "^[0-9]+\\.[0-9]+" SOVERSION_NUMBER "${VERSION_NUMBER}")
else()
  string(REGEX MATCH "^[0-9]+" SOVERSION_NUMBER "${VERSION_NUMBER}")
endif()

set_target_properties(celestial_calendar PROPERTIES
  VERSION ${VERSION_NUMBER}
  SOVERSION ${SOVERSION_NUMBER}
)

# Release packaging runs `cmake --install` (build_and_test.yml): the real file and the
# SOVERSION symlinks land by CMake's rules. All three destinations are required — the
# Windows .dll goes to RUNTIME, its import .lib to ARCHIVE, and without ARCHIVE no .lib.
# No COMPONENT split: the release shape is a single zip, there is no per-component
# audience (package config / find_package is deliberately not offered — no consumer).
install(TARGETS celestial_calendar
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(FILES celestial.h DESTINATION include)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE" DESTINATION .)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../THIRD_PARTY_NOTICES.txt" DESTINATION .)

# #67: compile-check the published C header as pure C (layouts pinned by `_Static_assert`
# inside). Built as part of the default target, so CI exercises it on every platform.
add_library(c_header_check OBJECT c_header_check.c)
set_target_properties(c_header_check PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
