# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.22)
project(celestial-calendar-python VERSION "${SKBUILD_PROJECT_VERSION}" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror -Wpedantic -Wnull-dereference -Wunreachable-code)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include_directories("${REPO_ROOT}/src/astro")
include_directories("${REPO_ROOT}/src/calendar")
include_directories("${REPO_ROOT}/src/util")

if(WIN32)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(ENV{BUILD_VERSION} "${SKBUILD_PROJECT_VERSION}")
add_subdirectory("${REPO_ROOT}/src/shared_lib" shared_lib)

if(WIN32)
  set(PYTHON_NATIVE_SUFFIX ".dll")
elseif(APPLE)
  set(PYTHON_NATIVE_SUFFIX ".dylib")
else()
  set(PYTHON_NATIVE_SUFFIX ".so")
endif()

install(
  FILES "$<TARGET_FILE:celestial_calendar>"
  DESTINATION celestial_calendar/_native
  RENAME "_celestial_calendar${PYTHON_NATIVE_SUFFIX}"
  COMPONENT python
)
install(
  FILES
    "${REPO_ROOT}/LICENSE"
    "${REPO_ROOT}/THIRD_PARTY_NOTICES.txt"
  DESTINATION "${SKBUILD_METADATA_DIR}/licenses"
  COMPONENT python
)
