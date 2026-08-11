#!/usr/bin/env python3
#
# Build the browser WASM module from the shared-library sources (#163).
#
#########################################################################################
#
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
import sys
import argparse
import subprocess

from pathlib import Path
from typing import Final


PROJ_ROOT: Final[Path] = Path(__file__).parent.parent
SRC_DIR: Final[Path] = PROJ_ROOT / "src"
DEFAULT_OUT_DIR: Final[Path] = PROJ_ROOT / "build" / "wasm"
MODULE_STEM: Final[str] = "celestial-jieqi"

# The narrow export surface (#163): Jieqi + Julian Day + `last_error` -- the reader is
# exported so a wasm consumer getting `valid = false` can learn why (the writers are
# listed on `last_error` in celestial.h). Widened once (#183 follow-up): moon
# illumination + local apparent sidereal time, the nav's two remaining real-data needs.
# Widening the surface further is a deliberate act -- every added entry ships bytes to
# the browser.
EXPORTS: Final[list[str]] = [
  "query_jieqi_moment",
  "get_jieqi_name",
  "ut1_to_jd",
  "ut1_to_jde",
  "jde_to_ut1",
  "moon_illumination",
  "local_apparent_sidereal_time",
  "last_error",
  "malloc",
  "free",  # the sret struct return needs caller-side scratch space
]

# HEAP* views are how the JS side reads the sret struct; ccall for convenience wrappers.
RUNTIME_METHODS: Final[list[str]] = ["ccall", "HEAPU8", "HEAP32", "HEAPU32", "HEAPF64"]

# -fwasm-exceptions is not optional: the library throws on bad input and the C ABI turns
# that into `valid = false`; without it a throw is a module trap (#163).
# -DNDEBUG keeps this a Release-shaped build like every other consumer (AGENTS.md gotcha
# 8): without it asserts stay live -- they abort (which the valid=false contract forbids)
# and bake the build machine's absolute paths into the artifact via __FILE__.
# -sENVIRONMENT=web,node is one build for both homes: the two ENVIRONMENT variants produce
# a bit-identical .wasm (measured #163), only the glue differs, and this glue speaks both.
COMMON_FLAGS: Final[list[str]] = [
  "-Oz",
  "-std=c++23",
  "-DNDEBUG",
  "-fwasm-exceptions",
  "-I",
  str(SRC_DIR / "astro"),
  "-I",
  str(SRC_DIR / "calendar"),
  "-I",
  str(SRC_DIR / "util"),
  "-sEXPORT_ES6=1",
  "-sMODULARIZE=1",
  "-sFILESYSTEM=0",
  "-sENVIRONMENT=web,node",
]


def emsdk_dir(explicit: str | None) -> Path:
  """Locate the emsdk checkout. An explicit --emsdk is authoritative: a bad one is an
  error, not a hint to look elsewhere. Without it, $EMSDK (set by `emsdk_env.sh`) is
  the only fallback -- no machine-specific default paths in a shared script."""
  candidates = [Path(explicit)] if explicit else []
  if not explicit and os.environ.get("EMSDK"):
    candidates.append(Path(os.environ["EMSDK"]))

  for cand in candidates:
    if (cand / "upstream" / "emscripten" / "em++").is_file():
      return cand
  tried = ", ".join(str(c) for c in candidates) or "(none)"
  raise FileNotFoundError(
    f"em++ not found under {tried} -- clone https://github.com/emscripten-core/emsdk.git, "
    "then ./emsdk install <ver> && ./emsdk activate <ver>, and pass --emsdk or set $EMSDK"
  )


def emsdk_python(sdk: Path) -> str:
  """The interpreter em++ is driven with. Prefer the python emsdk ships -- it does not
  ship one on every platform; without a bundled one, use the interpreter running this
  script (already >= 3.10 wherever this script parses). The point is keeping em++ off
  whatever python3 the host PATH holds (stock macOS = 3.9, which emscripten rejects)."""
  candidates = sorted((sdk / "python").glob("*/bin/python3"))
  return str(candidates[0]) if candidates else sys.executable


def build(sdk: Path, sources: list[Path], out_dir: Path) -> None:
  """Build celestial-jieqi.{mjs,wasm}. Stale outputs are cleared first (#155's lesson:
  a renamed or deleted variant must stop silently passing the checker)."""
  env = dict(os.environ)
  env["EMSDK"] = str(sdk)
  env["EMSDK_PYTHON"] = str(emsdk_python(sdk))
  env["PATH"] = os.pathsep.join([str(sdk), str(sdk / "upstream" / "emscripten"), env["PATH"]])

  out = out_dir / f"{MODULE_STEM}.mjs"
  cmd = [
    str(sdk / "upstream" / "emscripten" / "em++"),
    *COMMON_FLAGS,
    *[str(s) for s in sources],
    f"-sEXPORTED_FUNCTIONS={','.join('_' + e for e in EXPORTS)}",
    f"-sEXPORTED_RUNTIME_METHODS={','.join(RUNTIME_METHODS)}",
    "-o",
    str(out),
  ]
  print(f"[ build_wasm ] {' '.join(cmd)}")
  ret = subprocess.run(cmd, env=env)
  if ret.returncode != 0:
    raise RuntimeError(f"em++ failed, exit {ret.returncode}")
  print(f"[ build_wasm ] {out} + {out.with_suffix('.wasm')}")


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Build the browser WASM module from the shared-library sources (#163).")
  parser.add_argument("--emsdk", default=None, help="path to the emsdk checkout (else $EMSDK)")
  parser.add_argument(
    "--out-dir", type=Path, default=DEFAULT_OUT_DIR, help=f"output directory (default {DEFAULT_OUT_DIR})"
  )
  args = parser.parse_args()

  sdk = emsdk_dir(args.emsdk)
  args.out_dir.mkdir(parents=True, exist_ok=True)
  for stale in args.out_dir.glob(f"{MODULE_STEM}*"):
    stale.unlink()

  # Mirrors shared_lib/CMakeLists.txt's GLOB for source FILES: a new lib*.cpp needs no
  # edit here. Include DIRECTORIES are a second copy of src/CMakeLists.txt's
  # include_directories (see COMMON_FLAGS) -- a new src/<domain>/ must be added by hand.
  sources = sorted((SRC_DIR / "shared_lib").glob("lib*.cpp"))
  if not sources:
    raise FileNotFoundError(f"no lib*.cpp under {SRC_DIR / 'shared_lib'}")

  build(sdk, sources, args.out_dir)
