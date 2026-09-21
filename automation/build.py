# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import os
import json
import shutil

from . import paths
from .env import Tool, check_tool
from .gtest import clear_test_binaries
from .utils import run_cmd, yellow_print, red_print, green_print, ProcReturn


BUILD_DIR = paths.build_dir()
SRC_DIR = paths.cpp_src_dir()


def run_cmake(build_version: str, build_type: str = "Release", export_compile_commands: bool = True) -> int:
  """Run CMake to generate build files."""
  print("#" * 60)
  yellow_print(f"# Building version {build_version} in {build_type} mode")

  if not BUILD_DIR.exists():
    red_print("# Build directory not found")
    yellow_print(f"# Creating {BUILD_DIR}")
    BUILD_DIR.mkdir()

  yellow_print("# Running cmake...")

  cmds = ["cmake", str(SRC_DIR), f"-DCMAKE_BUILD_TYPE={build_type}"]

  # Pass `-G` only when the caller has not chosen: CMake reads `CMAKE_GENERATOR` from the
  # environment, and a `-G` on the command line silently wins over it. The default is load-bearing,
  # not a leftover -- the Windows legs build with clang plus choco's `make` (`build_and_test.yml`,
  # `random_soak.yml`), and a generator that lays binaries out per configuration would put them
  # somewhere the `build/test/*` acceptance run does not look (#72).
  if "CMAKE_GENERATOR" not in os.environ:
    cmds += ["-G", "Unix Makefiles"]

  if export_compile_commands:
    cmds.append("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")

  env = os.environ.copy()
  env["BUILD_VERSION"] = build_version

  ret: ProcReturn = run_cmd(cmds, cwd=BUILD_DIR, env=env)
  if ret.retcode == 0:
    green_print("# CMake ran successfully")
  else:
    red_print("# Failed to run CMake")

  # Remove external translation units from `compile_commands.json`.
  if export_compile_commands and ret.retcode == 0:
    db = BUILD_DIR / "compile_commands.json"
    raw_contents = json.loads(db.read_text())
    filtered = [
      entry
      for entry in raw_contents
      if "googletest" not in entry["file"]
      and not (
        "/src/test/provenance/erfa/" in entry["file"].replace("\\", "/")
        and entry["file"].replace("\\", "/").endswith(("/cal2jd.c", "/jd2cal.c"))
      )
    ]
    db.write_text(json.dumps(filtered, indent=2))

  print("#" * 60)
  return ret.retcode


def build_project(cpu_cores: int = 8) -> int:
  """Build the C++ project using the specified number of CPU cores."""
  print("#" * 60)

  assert BUILD_DIR.exists(), "Build directory not found"
  assert BUILD_DIR.is_dir(), "Build directory is not a directory"

  # Clear the test binaries first, so that the build is what puts every one of them back (#155).
  clear_test_binaries()

  yellow_print("# Building the C++ projects...")
  ret: ProcReturn = run_cmd(
    ["cmake", "--build", ".", "--parallel", str(cpu_cores)], cwd=BUILD_DIR, env=os.environ.copy()
  )

  print("#" * 60)
  return ret.retcode


def build_docs(build_version: str) -> int:
  """Generate the opt-in API reference without configuring or compiling C++."""
  if not check_tool(Tool("doxygen"), report=True):
    red_print("# Install Doxygen as described in docs/API.md before running project.py --docs")
    return 1
  root = paths.proj_root()
  config = root / "docs" / "Doxyfile"
  if not config.is_file():
    red_print(f"# Doxygen configuration is missing: {config}")
    return 1
  output = paths.build_dir() / "api-docs"
  if output.exists():
    shutil.rmtree(output)
  output.mkdir(parents=True)
  result = run_cmd(["doxygen", str(config)], cwd=root, env={**os.environ, "BUILD_VERSION": build_version})
  if result.retcode != 0:
    return result.retcode

  html = output / "html"
  pages = {
    "index.html": ("API Reference", build_version),
    "celestial_8h.html": ("query_jieqi_moment", "last_error"),
    "structastro_1_1toolbox_1_1SphericalCoordinate.html": ("\u03bb", "\u03b2"),
    "structcalendar_1_1lunar_1_1converter_1_1Converter.html": ("is_valid_gregorian", "\u516c\u5386", "\u9634\u5386"),
  }
  try:
    for name, required in pages.items():
      text = (html / name).read_text(encoding="utf-8")
      if not all(fragment in text for fragment in required):
        raise ValueError(f"generated page is incomplete: {name}")
    for name in ("doxygen.css", "navtree.js", "search/search.js"):
      if not (html / name).is_file() or (html / name).stat().st_size == 0:
        raise ValueError(f"generated asset is missing or empty: {name}")
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.txt"):
      if (html / name).read_bytes() != (root / name).read_bytes():
        raise ValueError(f"documentation attribution differs: {name}")
    if any(html.glob("*_source.html")):
      raise ValueError("API reference must not include source listings")
  except (OSError, UnicodeError, ValueError) as error:
    red_print(f"# API reference validation failed: {error}")
    return 1
  green_print("# API reference: build/api-docs/html/index.html")
  return 0


def clean_build() -> int:
  """Clean the build directory."""
  print("#" * 60)

  if BUILD_DIR.exists():
    yellow_print("# Build dir exists.")
    red_print(f"# Removing {BUILD_DIR}")
    shutil.rmtree(BUILD_DIR)

  green_print("# Build cleaned...")
  print("#" * 60)
  return 0
