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
import argparse
import subprocess

from pathlib import Path
from typing import Final


PROJ_ROOT: Final[Path] = Path(__file__).parent.parent
SRC_DIR: Final[Path] = PROJ_ROOT / "src"
DEFAULT_OUT_DIR: Final[Path] = PROJ_ROOT / "build" / "wasm"

# The narrow export surface (#163): Jieqi + Julian Day only. Widening it is a deliberate
# act -- every added entry ships bytes to every visitor's browser.
EXPORTS: Final[list[str]] = [
  "query_jieqi_moment", "get_jieqi_name",
  "ut1_to_jd", "ut1_to_jde", "jde_to_ut1",
  "malloc", "free",  # the sret struct return needs caller-side scratch space
]

# HEAP* views are how the JS side reads the sret struct; ccall for convenience wrappers.
RUNTIME_METHODS: Final[list[str]] = ["ccall", "HEAPU8", "HEAP32", "HEAPU32", "HEAPF64"]

# -fwasm-exceptions is not optional: the library throws on bad input and the C ABI turns
# that into `valid = false`; without it a throw is a module trap (#163 terrain).
COMMON_FLAGS: Final[list[str]] = [
  "-Oz", "-std=c++23", "-fwasm-exceptions",
  "-I", str(SRC_DIR / "astro"), "-I", str(SRC_DIR / "calendar"), "-I", str(SRC_DIR / "util"),
  "-sEXPORT_ES6=1", "-sMODULARIZE=1", "-sFILESYSTEM=0",
]


def emsdk_dir(explicit: str | None) -> Path:
  """Locate the emsdk checkout: --emsdk wins, then $EMSDK, then ~/repos/emsdk."""
  candidates = [Path(explicit)] if explicit else []
  if os.environ.get("EMSDK"):
    candidates.append(Path(os.environ["EMSDK"]))
  candidates.append(Path.home() / "repos" / "emsdk")

  for cand in candidates:
    if (cand / "upstream" / "emscripten" / "em++").is_file():
      return cand
  tried = ", ".join(str(c) for c in candidates)
  raise FileNotFoundError(
    f"em++ not found under any candidate ({tried}) -- "
    "clone https://github.com/emscripten-core/emsdk.git, then "
    "./emsdk install <ver> && ./emsdk activate <ver>"
  )


def build_variant(sdk: Path, sources: list[Path], env_name: str, out_dir: Path) -> None:
  """Build one -sENVIRONMENT=<env_name> variant. `web` is the shippable shape; `node`
  exists so toolbox/wasm_check.mjs can run the golden dataset without a browser."""
  env = dict(os.environ)
  env["EMSDK"] = str(sdk)
  env["PATH"] = f"{sdk}:{sdk / 'upstream' / 'emscripten'}:{env['PATH']}"

  out = out_dir / f"celestial-jieqi-{env_name}.mjs"
  cmd = [
    str(sdk / "upstream" / "emscripten" / "em++"),
    *COMMON_FLAGS,
    *[str(s) for s in sources],
    f"-sEXPORTED_FUNCTIONS={','.join('_' + e for e in EXPORTS)}",
    f"-sEXPORTED_RUNTIME_METHODS={','.join(RUNTIME_METHODS)}",
    f"-sENVIRONMENT={env_name}",
    "-o", str(out),
  ]
  print(f"[ build_wasm ] {' '.join(cmd)}")
  ret = subprocess.run(cmd, env=env)
  if ret.returncode != 0:
    raise RuntimeError(f"em++ failed ({env_name} variant), exit {ret.returncode}")
  print(f"[ build_wasm ] {out} + {out.with_suffix('.wasm')}")


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
    description="Build the browser WASM module from the shared-library sources (#163)."
  )
  parser.add_argument("--emsdk", default=None, help="path to the emsdk checkout")
  parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR,
                      help=f"output directory (default {DEFAULT_OUT_DIR})")
  args = parser.parse_args()

  sdk = emsdk_dir(args.emsdk)
  args.out_dir.mkdir(parents=True, exist_ok=True)

  # Mirror shared_lib/CMakeLists.txt's GLOB: every lib*.cpp goes in, so a new source file
  # reaches the wasm build without anyone remembering this script.
  sources = sorted((SRC_DIR / "shared_lib").glob("lib*.cpp"))
  if not sources:
    raise FileNotFoundError(f"no lib*.cpp under {SRC_DIR / 'shared_lib'}")

  for variant in ("web", "node"):
    build_variant(sdk, sources, variant, args.out_dir)
