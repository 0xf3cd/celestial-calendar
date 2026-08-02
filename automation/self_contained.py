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
import re

from pathlib import Path
from typing import List, Final

from . import paths
from .utils import run_cmd, green_print, red_print, yellow_print, blue_print


INCLUDE_DIR_RE: Final[re.Pattern] = re.compile(
  r"^\s*include_directories\(\$\{PROJECT_SOURCE_DIR\}/([^)]+)\)", re.MULTILINE
)


def include_roots(src_dir: Path) -> List[Path]:
  """Read the include roots straight out of src/CMakeLists.txt.

  Deliberately derived rather than copied. The gate is only meaningful if it compiles
  headers under the *same* search path the real build gives them: a wider path lets a
  header get away with an include that only resolves for the gate (e.g. "defines.hpp",
  reachable only via -Iastro/vsop87d), and the gate would wave through something the
  build rejects. A narrower one flags includes that are actually fine. Either way a
  hand-copied list drifts silently, so there is no second list to drift.
  """
  cmakelists = src_dir / "CMakeLists.txt"
  roots = [src_dir / name.strip() for name in INCLUDE_DIR_RE.findall(cmakelists.read_text())]
  # src/CMakeLists.txt still names `common`, which no longer exists (tracked in #72).
  # Passing -I for a missing directory is harmless but pointless, so drop it.
  return [r for r in roots if r.is_dir()]


def check_self_contained() -> int:
  """Compile every header on its own to prove it is self-contained.

  A header-only library ships its headers as the product, so each one must compile
  as the first thing a translation unit sees. `-fsyntax-only` gives us that check
  for the cost of a parse, with no object files produced.

  This must run on more than one standard library to be worth anything. Standard
  libraries differ in what they include transitively, so a header that forgets an
  include rides on its neighbours wherever that neighbour happens to provide the
  symbol, and fails elsewhere. `converter.hpp` used `assert` with no <cassert> and
  passed on MSVC STL while failing on libc++ (#71).
  """
  print("#" * 60)
  yellow_print("Checking that every header is self-contained...")

  cxx = os.environ.get("CXX", "clang++")
  src_dir = paths.cpp_src_dir()
  headers = sorted(p for p in src_dir.rglob("*.hpp"))

  if not headers:
    red_print(f"No headers found under {src_dir}")
    return 1

  roots = include_roots(src_dir)
  if not roots:
    red_print(f"No include_directories() found in {src_dir / 'CMakeLists.txt'}")
    return 1

  includes = [f"-I{r}" for r in roots]
  blue_print(f"# Compiler: {cxx} | {len(headers)} header(s) under {src_dir}")
  blue_print(f"# Include roots (from CMakeLists.txt): {', '.join(r.name for r in roots)}")

  failed: List[str] = []
  for header in headers:
    rel = header.relative_to(paths.proj_root())
    ret = run_cmd(
      # -Wno-pragma-once-outside-header: compiling a header as the main file is the
      # whole point here, so that warning is an artefact of the check, not a finding.
      [cxx, "-std=c++23", "-fsyntax-only", "-Wno-pragma-once-outside-header",
       *includes, "-x", "c++", str(header)],
      print_cmd=False,
      print_stdout=False,
      print_stderr=False,
    )
    if ret.retcode == 0:
      green_print(f"PASS  {rel}")
    else:
      red_print(f"FAIL  {rel}")
      # The diagnostic is the actionable part -- print it, do not just count the failure.
      for line in (ret.stderr or ret.stdout).splitlines()[:8]:
        print(f"        {line}")
      failed.append(str(rel))

  print("#" * 60)
  if failed:
    red_print(f"{len(failed)} of {len(headers)} header(s) are not self-contained:")
    for f in failed:
      red_print(f"  - {f}")
    yellow_print("Add the missing #include to the header itself; do not rely on a transitive one.")
    return 1

  green_print(f"All {len(headers)} header(s) are self-contained")
  return 0
