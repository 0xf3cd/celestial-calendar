# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture
def programs(tmp_path):
  programs = {}
  for name in ("gmake", "make", "custom"):
    directory = tmp_path / (name + " tools")
    directory.mkdir()
    program = directory / (name + (".exe" if sys.platform == "win32" else ""))
    program.write_bytes(b"discovery fixture")
    program.chmod(0o755)
    programs[name] = program
  return programs


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
def test_cmake_make_program_selection(tmp_path, programs, windows, generator, override, has_make, expected):
  if not has_make:
    programs["make"].unlink()
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


@pytest.mark.parametrize("hook", ["none", "toolchain-cli", "toolchain-env", "project", "named-project"])
def test_cmake_configure_preserves_hooks_and_reports_the_build_program(tmp_path, programs, hook):
  source = tmp_path / "source"
  source.mkdir()
  cmake = (ROOT / "src/CMakeLists.txt").read_text(encoding="utf-8")
  configure = cmake.split("set(CMAKE_CXX_STANDARD", 1)[0].replace(
    "project(celestial-calendar)", "project(celestial-calendar NONE)"
  )
  (source / "CMakeLists.txt").write_text(
    "set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)\n"
    "set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)\n"
    + configure
    + '\nfile(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${CMAKE_MAKE_PROGRAM}")\n',
    encoding="utf-8",
  )
  initialization = tmp_path / "initialize.cmake"
  initialization.write_text(
    f'set(CMAKE_MAKE_PROGRAM "{programs["custom"].as_posix()}" CACHE FILEPATH "caller make")\n',
    encoding="utf-8",
  )
  build = tmp_path / "build"
  command = [
    shutil.which("cmake"),
    "-S",
    str(source),
    "-B",
    str(build),
    "-G",
    "Unix Makefiles",
    "-DCMAKE_HOST_WIN32=TRUE",
    f"-DCMAKE_PROGRAM_PATH={programs['gmake'].parent.as_posix()};{programs['make'].parent.as_posix()}",
  ]
  env = {key: value for key, value in os.environ.items() if key != "CMAKE_TOOLCHAIN_FILE"}
  if hook == "toolchain-env":
    env["CMAKE_TOOLCHAIN_FILE"] = str(initialization)
  elif hook != "none":
    variable = {
      "toolchain-cli": "CMAKE_TOOLCHAIN_FILE",
      "project": "CMAKE_PROJECT_INCLUDE_BEFORE",
      "named-project": "CMAKE_PROJECT_celestial-calendar_INCLUDE_BEFORE",
    }[hook]
    command.append(f"-D{variable}={initialization.as_posix()}")
  result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=30)
  assert result.returncode == 0, result.stdout + result.stderr
  expected = programs["make" if hook == "none" else "custom"].as_posix()
  assert (build / "selected.txt").read_text(encoding="utf-8") == expected
  assert f"-- build program: {expected}" in result.stdout.splitlines()
