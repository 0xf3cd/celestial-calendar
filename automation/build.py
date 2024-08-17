# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
# 
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
# 
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import os
import json
import shutil

from . import paths
from .utils import (
  run_cmd, yellow_print, red_print, green_print, ProcReturn
)


BUILD_DIR = paths.build_dir()
SRC_DIR = paths.cpp_src_dir()


def run_cmake(
  build_version: str, 
  build_type: str = "Release", 
  export_compile_commands: bool = True
) -> int:
  """Run CMake to generate build files."""
  print("#" * 60)
  yellow_print(f"# Building version {build_version} in {build_type} mode")

  if not BUILD_DIR.exists():
    red_print("# Build directory not found")
    yellow_print(f"# Creating {BUILD_DIR}")
    BUILD_DIR.mkdir()

  yellow_print("# Running cmake...")

  cmds = ["cmake", str(SRC_DIR), "-G", "Unix Makefiles", f"-DCMAKE_BUILD_TYPE={build_type}"]
  if export_compile_commands:
    cmds.append("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")

  env = os.environ.copy()
  env["BUILD_VERSION"] = build_version

  ret: ProcReturn = run_cmd(cmds, cwd=BUILD_DIR, env=env)
  if ret.retcode == 0:
    green_print("# CMake ran successfully")    
  else:
    red_print("# Failed to run CMake")

  # Remove googletest files in `compile_commands.json`
  if export_compile_commands and ret.retcode == 0:
    db = BUILD_DIR / "compile_commands.json"
    raw_contents = json.loads(db.read_text())
    filtered = list(filter(lambda x: "googletest" not in x["file"], raw_contents))
    db.write_text(json.dumps(filtered, indent=2))

  print("#" * 60)
  return ret.retcode


def build_project(cpu_cores: int = 8) -> int:
  """Build the C++ project using the specified number of CPU cores."""
  print("#" * 60)

  assert BUILD_DIR.exists(), "Build directory not found"
  assert BUILD_DIR.is_dir(), "Build directory is not a directory"

  yellow_print("# Building the C++ projects...")
  ret: ProcReturn = run_cmd(["cmake", "--build", ".", "--parallel", str(cpu_cores)], 
                            cwd=BUILD_DIR, env=os.environ.copy())

  print("#" * 60)
  return ret.retcode


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
