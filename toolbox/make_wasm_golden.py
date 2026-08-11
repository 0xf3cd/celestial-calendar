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
import struct

from ctypes import c_int32, c_uint32, c_uint8, c_double, c_bool, Structure
from datetime import date
from pathlib import Path
from typing import Final

# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import paths, run_cmd


OUT_PATH: Final[Path] = Path(__file__).parent / "wasm_golden.json"

# Dataset shape (#163): the real validity edges at all 24 indices (401 = the UT1 chain's
# floor, 32766 = the declared max of jieqi_jde), the site nav's consumer window
# (1950/2050), and interior years -- plus a seeded random fill. Out-of-window years are
# invalid natively, so the throw path lives in the checker's exception probe instead.
# @2 (#183 follow-up): moon illumination and sidereal sections join. The jieqi section
# keeps its exact point set -- it is generated first, consuming the seeded PRNG in the
# same order as @1. Moon points cover the Example 48.a anchor plus a uniform fill over
# [1900, 2100]; sidereal points cover the J2000 anchor plus a uniform fill over the
# export's declared window [401, 32766] with east-positive longitudes.
SEED: Final[int] = 42
EDGE_YEARS: Final[list[int]] = [401, 1950, 1999, 2026, 2050, 32766]
RANDOM_POINTS: Final[int] = 60
MOON_RANDOM_POINTS: Final[int] = 40
SIDEREAL_RANDOM_POINTS: Final[int] = 40

EXAMPLE_48A_JDE: Final[float] = 2448724.5  # 1992-04-12 0h TT
J2000_JD: Final[float] = 2451545.0         # 2000-01-01 12:00
# JDE bounds for [1900, 2100], and JD bounds for the sidereal export's declared [401, 32766]
# window (jd_to_ut1's floor guard at one end, ut1_to_jd(32766-12-31 0h) at the other).
MOON_JDE_RANGE: Final[tuple[float, float]] = (2415020.5, 2488068.5)
SIDEREAL_JD_RANGE: Final[tuple[float, float]] = (1867522.5, 13688959.5)


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


class _MoonIllumination(Structure):
  """Minimal mirror of `MoonIllumination` in `celestial.h`."""
  _fields_ = [
    ("valid",          c_bool),
    ("illumination",   c_double),
    ("elongation_deg", c_double),
  ]


class _SiderealTime(Structure):
  """Minimal mirror of `SiderealTime` in `celestial.h`."""
  _fields_ = [
    ("valid", c_bool),
    ("value", c_double),
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


def f64_bits(value: float) -> str:
  """A double as its IEEE-754 bit pattern (hex string): JSON numbers cannot hold a uint64,
  and decoding the bits is exact -- no float round-trip through JSON text."""
  return f"0x{struct.unpack('<Q', struct.pack('<d', value))[0]:016x}"


def generate() -> dict:
  lib = ctypes.CDLL(str(find_native_lib()))
  lib.query_jieqi_moment.argtypes = [c_int32, c_uint8]
  lib.query_jieqi_moment.restype = _JieqiMomentQuery
  lib.moon_illumination.argtypes = [c_double]
  lib.moon_illumination.restype = _MoonIllumination
  lib.local_apparent_sidereal_time.argtypes = [c_double, c_double]
  lib.local_apparent_sidereal_time.restype = _SiderealTime

  random.seed(SEED)
  points = [(y, i) for y in EDGE_YEARS for i in range(24)]
  points += [(random.randint(1950, 2050), random.randrange(24)) for _ in range(RANDOM_POINTS)]

  jieqi_entries = []
  for year, idx in points:
    q = lib.query_jieqi_moment(year, idx)
    if not q.valid or q.jq_idx != idx:
      raise RuntimeError(f"query_jieqi_moment({year}, {idx}) failed natively")
    jieqi_entries.append({"year": year, "idx": idx, "y": q.y, "m": q.m, "d": q.d,
                          "frac_bits": f64_bits(q.frac)})

  moon_jdes = [EXAMPLE_48A_JDE]
  moon_jdes += [random.uniform(*MOON_JDE_RANGE) for _ in range(MOON_RANDOM_POINTS)]
  moon_entries = []
  for jde in moon_jdes:
    mi = lib.moon_illumination(jde)
    if not mi.valid:
      raise RuntimeError(f"moon_illumination({jde}) failed natively")
    moon_entries.append({"jde_bits": f64_bits(jde),
                         "illumination_bits": f64_bits(mi.illumination),
                         "elongation_deg_bits": f64_bits(mi.elongation_deg)})

  sidereal_points = [(J2000_JD, 0.0), (SIDEREAL_JD_RANGE[0], 0.0), (SIDEREAL_JD_RANGE[1], 45.0)]
  sidereal_points += [(random.uniform(*SIDEREAL_JD_RANGE), random.uniform(-180.0, 180.0))
                      for _ in range(SIDEREAL_RANDOM_POINTS)]
  sidereal_entries = []
  for jd_ut1, lon in sidereal_points:
    st = lib.local_apparent_sidereal_time(jd_ut1, lon)
    if not st.valid:
      raise RuntimeError(f"local_apparent_sidereal_time({jd_ut1}, {lon}) failed natively")
    sidereal_entries.append({"jd_ut1_bits": f64_bits(jd_ut1), "longitude": lon,
                             "value_bits": f64_bits(st.value)})

  return {
    "schema": "celestial-calendar/wasm-golden@2",
    "provenance": {
      "source_commit": source_commit(),
      "generated_on": f"{platform.system()} {platform.machine()}, {date.today().isoformat()}",
      "seed": SEED,
    },
    "sections": {
      "jieqi":    {"entries": jieqi_entries},
      "moon":     {"entries": moon_entries},
      "sidereal": {"entries": sidereal_entries},
    },
  }


if __name__ == "__main__":
  doc = generate()
  total = sum(len(section["entries"]) for section in doc["sections"].values())
  OUT_PATH.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
  print(f"[ make_wasm_golden ] {total} points -> {OUT_PATH}")
