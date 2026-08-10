#!/usr/bin/env python3
#
# Regenerate the WASM golden dataset (toolbox/wasm_golden.json) from the native library.
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

import sys
import json
import ctypes
import random
import platform

from ctypes import c_int32, c_uint32, c_uint8, c_double, c_bool, Structure
from datetime import date
from pathlib import Path
from typing import Final

# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

import struct

from automation import paths, run_cmd


OUT_PATH: Final[Path] = Path(__file__).parent / "wasm_golden.json"

# Dataset shape (#163): the four window-edge years at all 24 indices (boundary behaviour
# is where regressions would live), plus a seeded random fill for the interior.
SEED: Final[int] = 42
EDGE_YEARS: Final[list[int]] = [1950, 1999, 2026, 2050]
RANDOM_POINTS: Final[int] = 60


class _JieqiMomentQuery(Structure):
  """Minimal mirror of `JieqiMomentQuery` in `celestial.h`."""
  _fields_ = [
    ("valid",  c_bool),
    ("jq_idx", c_uint8),
    ("y",      c_int32),
    ("m",      c_uint32),
    ("d",      c_uint32),
    ("frac",   c_double),
  ]


def find_native_lib() -> Path:
  """The built `libcelestial_calendar` under `build/shared_lib` (build it first)."""
  folder = paths.shared_lib_dir()
  candidates = sorted(folder.glob("libcelestial_calendar.dylib")) or \
               sorted(folder.glob("libcelestial_calendar.so")) or \
               sorted(folder.glob("celestial_calendar.dll"))
  if not candidates:
    raise FileNotFoundError(f"native shared library not found under {folder} -- run ./project.py --build first")
  return candidates[0]


def source_commit() -> str:
  ret = run_cmd(
    ["git", "rev-parse", "HEAD"],
    cwd=str(paths.proj_root()), print_cmd=False, print_stdout=False, print_stderr=False,
  )
  if ret.retcode != 0:
    raise RuntimeError("git rev-parse HEAD failed -- the dataset records its source commit")
  return ret.stdout.strip()


def generate() -> dict:
  lib = ctypes.CDLL(str(find_native_lib()))
  lib.query_jieqi_moment.argtypes = [c_int32, c_uint8]
  lib.query_jieqi_moment.restype = _JieqiMomentQuery

  random.seed(SEED)
  points = [(y, i) for y in EDGE_YEARS for i in range(24)]
  points += [(random.randint(1950, 2050), random.randrange(24)) for _ in range(RANDOM_POINTS)]

  entries = []
  for year, idx in points:
    q = lib.query_jieqi_moment(year, idx)
    if not q.valid or q.jq_idx != idx:
      raise RuntimeError(f"query_jieqi_moment({year}, {idx}) failed natively")
    # frac as its IEEE-754 bit pattern (hex string): JSON numbers cannot hold a uint64,
    # and the checker compares bit distances, not floats.
    bits = struct.unpack("<Q", struct.pack("<d", q.frac))[0]
    entries.append({"year": year, "idx": idx, "y": q.y, "m": q.m, "d": q.d,
                    "frac_bits": f"0x{bits:016x}"})

  return {
    "schema": "celestial-calendar/wasm-golden@1",
    "provenance": {
      "source_commit": source_commit(),
      "generated_on": f"{platform.system()} {platform.machine()}, {date.today().isoformat()}",
      "seed": SEED,
      # The checker allows a ULP gap, not bit equality: the wasm build links musl's libm
      # while each native platform links its own, and their trig results can differ at the
      # 1-ULP level (#163 terrain: 208 ULP measured macOS-native vs wasm).
    },
    "entries": entries,
  }


if __name__ == "__main__":
  doc = generate()
  OUT_PATH.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
  print(f"[ make_wasm_golden ] {len(doc['entries'])} points -> {OUT_PATH}")
