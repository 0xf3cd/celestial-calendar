# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import shutil
import subprocess
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
  ("windows", "generator", "override", "has_make", "expected"),
  [
    (True, "Unix Makefiles", False, True, "make"),
    (True, "Unix Makefiles", True, True, "custom"),
    (False, "Unix Makefiles", False, True, "gmake"),
    (False, "Unix Makefiles", True, True, "custom"),
    (True, "Ninja", False, True, ""),
    (True, "Ninja", True, True, "custom"),
    (True, "Unix Makefiles", False, False, "gmake"),
  ],
)
def test_cmake_make_program_selection(tmp_path, windows, generator, override, has_make, expected):
  programs = {}
  for name in ("gmake", "make", "custom"):
    directory = tmp_path / (name + " tools")
    directory.mkdir()
    program = directory / (name + (".exe" if sys.platform == "win32" else ""))
    if name != "make" or has_make:
      program.write_bytes(b"discovery fixture")
      program.chmod(0o755)
    programs[name] = program

  preamble = (ROOT / "src/CMakeLists.txt").read_text(encoding="utf-8").split("project(celestial-calendar)", 1)[0]
  script = tmp_path / "selection.cmake"
  script.write_text(
    "set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)\n"
    "set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)\n"
    f'set(CMAKE_PROGRAM_PATH "{programs["gmake"].parent.as_posix()};{programs["make"].parent.as_posix()}")\n'
    f"set(CMAKE_HOST_WIN32 {'TRUE' if windows else 'FALSE'})\n"
    f'set(CMAKE_GENERATOR "{generator}")\n'
    + (f'set(CMAKE_MAKE_PROGRAM "{programs["custom"].as_posix()}" CACHE FILEPATH "")\n' if override else "")
    + preamble
    + '\nif(CMAKE_GENERATOR STREQUAL "Unix Makefiles")\n'
    + '  include("${CMAKE_ROOT}/Modules/CMakeUnixFindMake.cmake")\n'
    + "endif()\n"
    + 'file(WRITE "${CMAKE_CURRENT_LIST_DIR}/selected.txt" "${CMAKE_MAKE_PROGRAM}")\n',
    encoding="utf-8",
  )
  result = subprocess.run([shutil.which("cmake"), "-P", str(script)], capture_output=True, text=True)
  assert result.returncode == 0, result.stdout + result.stderr
  selected = (tmp_path / "selected.txt").read_text(encoding="utf-8")
  assert selected == (programs[expected].as_posix() if expected else "")
