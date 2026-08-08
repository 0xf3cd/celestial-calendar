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

from pathlib import Path
from typing import List

from .utils import red_print


def proj_root() -> Path:
  """Locate the project root directory. This function also performs a sanity check so can be slightly slow."""
  root = Path(__file__).parent.parent

  # Ensure it is a git repo root.
  if not (root / ".git").exists():
    red_print("The project root is not a git repository.")
    raise RuntimeError("The project root is not a git repository.")

  return root


def cpp_src_dir() -> Path:
  """Locate the C++ source directory."""
  src = proj_root() / "src"

  # Ensure "CMakeLists.txt" exists.
  if not (src / "CMakeLists.txt").exists():
    red_print("The C++ source directory does not exist.")
    raise RuntimeError("The C++ source directory does not exist.")

  return src


def build_dir() -> Path:
  """Locate the build directory."""
  return proj_root() / "build"


def cpp_test_dir() -> Path:
  """Locate the C++ test directory."""
  return build_dir() / "test"


def shared_lib_dir() -> Path:
  """Locate the built shared library directory."""
  return build_dir() / "shared_lib"


def python_requirements() -> Path:
  """Locate the Python requirements file."""
  return proj_root() / "Requirements.txt"


def find_executables(directory: Path) -> List[Path]:
  """Every executable binary directly under `directory`, sorted by name.

  The suffix is what carries the check on Windows, where `os.access(X_OK)` passes for nearly
  any readable file. CMake's suffixed scaffolding (`CTestTestfile.cmake`,
  `<target>[1]_tests.cmake`) falls out of that check; `Makefile` is suffixless, so it is excluded
  by name -- on Unix the executable bit happens to exclude it, but on Windows (unreliable
  `X_OK`, and the build does use Unix Makefiles there) nothing else would. Deleting build-system
  files is not this step's job.

  What this returns is acted on, not just reported: it is unlinked before the next build, and
  on the benchmark path it is executed. A name that wrongly passes the filter is a file that
  gets deleted, or run.
  """
  if not directory.is_dir():
    return []
  return sorted(p for p in directory.iterdir()
                if p.is_file() and p.suffix.lower() in ("", ".exe")
                and p.name != "Makefile" and os.access(p, os.X_OK))
