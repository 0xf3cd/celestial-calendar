#!/usr/bin/env python3
#
# Regenerate the shared binding golden dataset (toolbox/bindings_golden.json) from the native library.
#
#########################################################################################
#
# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import sys
import json
import ctypes
import random
import platform
import struct
import argparse

from ctypes import c_uint32, c_double
from datetime import date
from pathlib import Path
from typing import Final

# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import paths, run_cmd
from automation.abi_layout import load_abi
from automation.library_provenance import find_shared_lib, verify_library


OUT_PATH: Final[Path] = paths.proj_root() / paths.BINDING_GOLDEN_RELATIVE

# Dataset shape (#163): the real validity edges at all 24 indices (401 = the UT1 chain's
# floor, 32766 = the declared max of jieqi_jde), the site nav's consumer window
# (1950/2050), and interior years -- plus a seeded random fill. Out-of-window years are
# invalid natively, so the throw path lives in the checker's exception probe instead.
# The jieqi section is generated first to preserve its seeded point set. Moon illumination
# and position angle share the same Example 48.a anchor plus uniform [1900, 2100] inputs;
# sidereal points span its declared [401, 32766] window. Phase rows hold the first moment
# for the existing 2024-2053 new-moon and 2024-2033 quarter/full-moon grids.
SEED: Final[int] = 42
EDGE_YEARS: Final[list[int]] = [401, 1950, 1999, 2026, 2050, 32766]
RANDOM_POINTS: Final[int] = 60
MOON_RANDOM_POINTS: Final[int] = 40
SIDEREAL_RANDOM_POINTS: Final[int] = 40
PHASE_NEW_MOON_YEARS: Final[range] = range(2024, 2054)
PHASE_OTHER_YEARS: Final[range] = range(2024, 2034)

# Event fixtures (#303): the same six queries src/test/shared_lib/events_cabi_test.cpp
# drives across the C ABI. The Mercury/Sun windows and the Moon-Sun pair replay the core
# suite's PyMeeus anchors (src/test/astro/ephemeris_events_test.cpp); the expected counts
# are pinned here so the dataset cannot silently drift from the fixtures it certifies.
EVENT_TARGET_SUN: Final[int] = 0
EVENT_TARGET_MOON: Final[int] = 1
EVENT_TARGET_MERCURY: Final[int] = 2
EVENT_WINDOW: Final[tuple[float, float]] = (2460735.5, 2460785.5)
PAIR_WINDOW: Final[tuple[float, float]] = (2460676.5, 2460707.5)
LONGITUDE_EVENT_FIXTURES: Final[tuple[tuple[str, int, float], ...]] = (
  ("sun-zero", EVENT_TARGET_SUN, 0.0),
  ("moon-fixed", EVENT_TARGET_MOON, 137.5),
  ("mercury-triple", EVENT_TARGET_MERCURY, 0.0),
)
STATION_EVENT_FIXTURES: Final[tuple[tuple[str, int], ...]] = (("mercury-stations", EVENT_TARGET_MERCURY),)
DIFFERENCE_EVENT_FIXTURES: Final[tuple[tuple[str, int, int, float], ...]] = (
  ("pair-minus-60", EVENT_TARGET_MOON, EVENT_TARGET_SUN, -60.0),
  ("pair-plus-60", EVENT_TARGET_MOON, EVENT_TARGET_SUN, 60.0),
)
EXPECTED_EVENT_COUNTS: Final[dict[str, int]] = {
  "sun-zero": 1,
  "moon-fixed": 2,
  "mercury-triple": 3,
  "mercury-stations": 2,
  "pair-minus-60": 1,
  "pair-plus-60": 1,
}

EXAMPLE_48A_JDE: Final[float] = 2448724.5  # 1992-04-12 0h TT
J2000_JD: Final[float] = 2451545.0  # 2000-01-01 12:00
# JDE bounds for [1900, 2100], and JD bounds for the sidereal export's declared [401, 32766]
# window (jd_to_ut1's floor guard at one end, ut1_to_jd(32766-12-31 0h) at the other).
MOON_JDE_RANGE: Final[tuple[float, float]] = (2415020.5, 2488068.5)
SIDEREAL_JD_RANGE: Final[tuple[float, float]] = (1867522.5, 13688959.5)


def source_commit() -> str:
  ret = run_cmd(
    ["git", "rev-parse", "HEAD"],
    cwd=str(paths.proj_root()),
    print_cmd=False,
    print_stdout=False,
    print_stderr=False,
  )
  if ret.retcode != 0:
    raise RuntimeError("git rev-parse HEAD failed -- the dataset records its source commit")
  return ret.stdout.strip()


def f64_bits(value: float) -> str:
  """A double as its IEEE-754 bit pattern (hex string): JSON numbers cannot hold a uint64,
  and decoding the bits is exact -- no float round-trip through JSON text."""
  return f"0x{struct.unpack('<Q', struct.pack('<d', value))[0]:016x}"


def generate() -> dict:
  artifact = find_shared_lib()
  verify_library(artifact, paths.proj_root())
  lib = ctypes.CDLL(str(artifact))
  abi = load_abi()
  for name in (
    "celestial_query_jieqi_moment",
    "celestial_moon_illumination",
    "celestial_moon_position_angle",
    "celestial_moon_phase_moments",
    "celestial_local_apparent_sidereal_time",
    "celestial_longitude_events",
    "celestial_longitude_stations",
    "celestial_longitude_difference_events",
    "celestial_last_error",
    "celestial_last_error_kind",
  ):
    function = getattr(lib, name)
    function.argtypes, function.restype = abi.BINDING_SPECS[name]

  def check_error(operation: str) -> None:
    """A failed allocation of the error message must not look like an empty success."""
    if lib.celestial_last_error_kind():
      message = lib.celestial_last_error()
      detail = message.decode("utf-8", errors="replace") if message else "native failure without a message"
      raise RuntimeError(f"{operation}: {detail}")

  random.seed(SEED)
  points = [(y, i) for y in EDGE_YEARS for i in range(24)]
  points += [(random.randint(1950, 2050), random.randrange(24)) for _ in range(RANDOM_POINTS)]

  jieqi_entries = []
  for year, idx in points:
    q = lib.celestial_query_jieqi_moment(year, idx)
    check_error("celestial_query_jieqi_moment")
    if not q.valid or q.jq_idx != idx:
      raise RuntimeError(f"query_jieqi_moment({year}, {idx}) failed natively")
    jieqi_entries.append({"year": year, "idx": idx, "y": q.y, "m": q.m, "d": q.d, "frac_bits": f64_bits(q.frac)})

  moon_jdes = [EXAMPLE_48A_JDE]
  moon_jdes += [random.uniform(*MOON_JDE_RANGE) for _ in range(MOON_RANDOM_POINTS)]
  moon_entries = []
  for jde in moon_jdes:
    mi = lib.celestial_moon_illumination(jde)
    check_error("celestial_moon_illumination")
    if not mi.valid:
      raise RuntimeError(f"moon_illumination({jde}) failed natively")
    moon_entries.append(
      {
        "jde_bits": f64_bits(jde),
        "illumination_bits": f64_bits(mi.illumination),
        "elongation_deg_bits": f64_bits(mi.elongation_deg),
      }
    )

  moon_position_angle_entries = []
  for jde in moon_jdes:
    pa = lib.celestial_moon_position_angle(jde)
    check_error("celestial_moon_position_angle")
    if not pa.valid:
      raise RuntimeError(f"moon_position_angle({jde}) failed natively")
    moon_position_angle_entries.append({"jde_bits": f64_bits(jde), "angle_deg_bits": f64_bits(pa.angle_deg)})

  sidereal_points = [(J2000_JD, 0.0), (SIDEREAL_JD_RANGE[0], 0.0), (SIDEREAL_JD_RANGE[1], 45.0)]
  sidereal_points += [
    (random.uniform(*SIDEREAL_JD_RANGE), random.uniform(-180.0, 180.0)) for _ in range(SIDEREAL_RANDOM_POINTS)
  ]
  sidereal_entries = []
  for jd_ut1, lon in sidereal_points:
    st = lib.celestial_local_apparent_sidereal_time(jd_ut1, lon)
    check_error("celestial_local_apparent_sidereal_time")
    if not st.valid:
      raise RuntimeError(f"local_apparent_sidereal_time({jd_ut1}, {lon}) failed natively")
    sidereal_entries.append({"jd_ut1_bits": f64_bits(jd_ut1), "longitude": lon, "value_bits": f64_bits(st.value)})

  phase_points = [(year, 0) for year in PHASE_NEW_MOON_YEARS]
  phase_points += [(year, phase_kind) for year in PHASE_OTHER_YEARS for phase_kind in range(1, 4)]
  phase_entries = []
  for year, phase_kind in phase_points:
    root_count = c_uint32(0)
    written = lib.celestial_moon_phase_moments(year, phase_kind, ctypes.byref(root_count), None, 0)
    check_error("celestial_moon_phase_moments")
    if written != 0 or root_count.value == 0:
      raise RuntimeError(f"moon_phase_moments({year}, {phase_kind}) count query failed")

    slots = (c_double * root_count.value)()
    written = lib.celestial_moon_phase_moments(year, phase_kind, ctypes.byref(root_count), slots, len(slots))
    check_error("celestial_moon_phase_moments")
    if written != len(slots) or root_count.value != len(slots):
      raise RuntimeError(f"moon_phase_moments({year}, {phase_kind}) wrote {written} of {root_count.value} roots")
    phase_entries.append(
      {
        "year": year,
        "phase_kind": phase_kind,
        "index": 0,
        "jde_bits": f64_bits(slots[0]),
      }
    )

  def search_events(export: str, record_type: type, args: tuple) -> list:
    """One count pass plus one fill pass through the raw count+fill protocol."""
    total = ctypes.c_uint32(0)
    written = getattr(lib, export)(*args, ctypes.byref(total), None, 0)
    check_error(export)
    if written != 0:
      raise RuntimeError(f"{export}{args} count pass wrote {written}")
    if total.value == 0:
      return []
    slots = (record_type * total.value)()
    fill_total = ctypes.c_uint32(0)
    written = getattr(lib, export)(*args, ctypes.byref(fill_total), slots, total.value)
    check_error(export)
    if written != total.value or fill_total.value != total.value:
      raise RuntimeError(f"{export}{args} fill pass wrote {written} of {total.value}")
    return [dict((field, getattr(slot, field)) for field, _ in record_type._fields_) for slot in slots]

  def check_fixture_count(name: str, events: list) -> None:
    if len(events) != EXPECTED_EVENT_COUNTS[name]:
      raise RuntimeError(f"{name}: recorded {len(events)} events, expected {EXPECTED_EVENT_COUNTS[name]}")

  longitude_event_entries = []
  for name, target_code, level in LONGITUDE_EVENT_FIXTURES:
    events = search_events(
      "celestial_longitude_events", abi.CelestialLongitudeEventRecord, (target_code, level, *EVENT_WINDOW)
    )
    check_fixture_count(name, events)
    longitude_event_entries.append(
      {
        "name": name,
        "target_code": target_code,
        "target_longitude_deg": level,
        "start_jde_tt": EVENT_WINDOW[0],
        "end_jde_tt": EVENT_WINDOW[1],
        "events": [
          {
            "jde_tt_bits": f64_bits(event["jde_tt"]),
            "longitude_deg_bits": f64_bits(event["longitude_deg"]),
            "longitude_rate_deg_per_tt_day_bits": f64_bits(event["longitude_rate_deg_per_tt_day"]),
            "direction_code": event["direction_code"],
          }
          for event in events
        ],
      }
    )

  station_event_entries = []
  for name, target_code in STATION_EVENT_FIXTURES:
    events = search_events(
      "celestial_longitude_stations", abi.CelestialStationEventRecord, (target_code, *EVENT_WINDOW)
    )
    check_fixture_count(name, events)
    station_event_entries.append(
      {
        "name": name,
        "target_code": target_code,
        "start_jde_tt": EVENT_WINDOW[0],
        "end_jde_tt": EVENT_WINDOW[1],
        "events": [
          {
            "jde_tt_bits": f64_bits(event["jde_tt"]),
            "longitude_deg_bits": f64_bits(event["longitude_deg"]),
            "longitude_rate_deg_per_tt_day_bits": f64_bits(event["longitude_rate_deg_per_tt_day"]),
            "motion_before_code": event["motion_before_code"],
            "motion_after_code": event["motion_after_code"],
          }
          for event in events
        ],
      }
    )

  difference_event_entries = []
  for name, first_code, second_code, level in DIFFERENCE_EVENT_FIXTURES:
    events = search_events(
      "celestial_longitude_difference_events",
      abi.CelestialLongitudeDifferenceEventRecord,
      (first_code, second_code, level, *PAIR_WINDOW),
    )
    check_fixture_count(name, events)
    difference_event_entries.append(
      {
        "name": name,
        "first_target_code": first_code,
        "second_target_code": second_code,
        "target_difference_deg": level,
        "start_jde_tt": PAIR_WINDOW[0],
        "end_jde_tt": PAIR_WINDOW[1],
        "events": [
          {
            "jde_tt_bits": f64_bits(event["jde_tt"]),
            "longitude_a_deg_bits": f64_bits(event["longitude_a_deg"]),
            "longitude_b_deg_bits": f64_bits(event["longitude_b_deg"]),
            "longitude_rate_a_deg_per_tt_day_bits": f64_bits(event["longitude_rate_a_deg_per_tt_day"]),
            "longitude_rate_b_deg_per_tt_day_bits": f64_bits(event["longitude_rate_b_deg_per_tt_day"]),
            "signed_difference_deg_bits": f64_bits(event["signed_difference_deg"]),
            "direction_code": event["direction_code"],
          }
          for event in events
        ],
      }
    )

  return {
    "schema": "celestial-calendar/bindings-golden@2",
    "provenance": {
      "source_commit": source_commit(),
      "generated_on": f"{platform.system()} {platform.machine()}, {date.today().isoformat()}",
      "seed": SEED,
    },
    "sections": {
      "jieqi": {"entries": jieqi_entries},
      "moon": {"entries": moon_entries},
      "sidereal": {"entries": sidereal_entries},
      "moon_position_angle": {"entries": moon_position_angle_entries},
      "phases": {"entries": phase_entries},
      "longitude_events": {"entries": longitude_event_entries},
      "longitude_stations": {"entries": station_event_entries},
      "longitude_difference_events": {"entries": difference_event_entries},
    },
  }


def generate_rise_set() -> dict:
  """Capture representative daily results from the receipt-verified native C boundary."""
  artifact = find_shared_lib()
  receipt = verify_library(artifact, paths.proj_root())
  lib = ctypes.CDLL(str(artifact))
  abi = load_abi()
  for name in (
    "celestial_solar_rise_set",
    "celestial_lunar_rise_set",
    "celestial_planet_rise_set",
    "celestial_last_error_kind",
  ):
    getattr(lib, name).argtypes, getattr(lib, name).restype = abi.BINDING_SPECS[name]

  # Fixed parity inputs, shared with rise_set_cabi_test.cpp; no random sampling.
  days = [
    (2000, 1, 1, 40.0, 0.0, 0.0),
    (2025, 3, 20, 51.5, 0.0, 0.0),
    (2025, 6, 21, 67.0, 20.0, 0.0),
    (2025, 12, 21, -67.0, 100.0, 0.0),
    (2026, 11, 16, 40.0, 116.0, 0.0),
    (2000, 1, 1, 40.0, 180.0, 1000.0),
    (2000, 1, 1, 40.0, -180.0, 1000.0),
    (2000, 1, 1, 90.0, 0.0, 0.0),
    (2000, 1, 1, -90.0, 0.0, 0.0),
    (2019, 8, 27, 75.0, 0.0, 0.0),
    (1885, 1, 2, 40.0, 0.0, 0.0),
    (1885, 1, 3, 40.0, 0.0, 0.0),
    (2099, 12, 30, 40.0, 0.0, 0.0),
    (2099, 12, 31, 40.0, 0.0, 0.0),
    (1, 1, 1, 40.0, 0.0, 0.0),
    (32767, 12, 31, 40.0, 0.0, 0.0),
    (2024, 2, 29, 40.0, 0.0, 0.0),
    (2024, 12, 31, 40.0, 0.0, 0.0),
  ]
  entries = []
  for day_index, (year, month, day, latitude, longitude, elevation) in enumerate(days):
    for target in range(10):
      for custom in (False, True):
        args = [year, month, day, latitude, longitude]
        if target == 0:
          name = "celestial_solar_rise_set"
          args += [-18.0 if custom else -50.0 / 60.0]
        elif target == 1:
          name = "celestial_lunar_rise_set"
          args += [2, -20.0, 800.0] if custom else [1, 15.0, 1013.25]
        else:
          name = "celestial_planet_rise_set"
          args = [target, *args, elevation, 0.0 if custom else -34.0 / 60.0]
        native = getattr(lib, name)(*args)
        error = lib.celestial_last_error_kind()
        if (native.valid and error) or (not native.valid and error != 1):
          raise RuntimeError(f"Unexpected daily-query outcome at {(day_index, target, custom)}: error={error}")
        fields = {
          field: f64_bits(getattr(native, field)) if kind is c_double else getattr(native, field)
          for field, kind in native._fields_
        }
        entries.append({"day_index": day_index, "target": target, "custom": custom, "error": error, "fields": fields})
  return {
    "schema": "celestial-calendar/rise-set-parity@1",
    "provenance": {
      "source_commit": receipt["source"]["commit"],
      "source_snapshot_sha256": receipt["source"]["snapshot_sha256"],
      "generated_on": f"{platform.system()} {platform.machine()}, {date.today().isoformat()}",
      "reference": "Native C++ wrappers compared exactly with C in rise_set_cabi_test.cpp",
    },
    "days": days,
    "entries": entries,
  }


def main() -> None:
  """Write the selected native dataset to its default or explicit destination."""
  parser = argparse.ArgumentParser(description="Regenerate the native binding golden dataset.")
  parser.add_argument(
    "--out-path",
    type=Path,
    help="output path (default: bindings_golden.json, or rise_set_golden.json with --rise-set)",
  )
  parser.add_argument("--rise-set", action="store_true", help="generate the separate daily-query parity dataset")
  args = parser.parse_args()

  doc = generate_rise_set() if args.rise_set else generate()
  total = len(doc["entries"]) if args.rise_set else sum(len(section["entries"]) for section in doc["sections"].values())
  destination = args.out_path or (OUT_PATH.with_name("rise_set_golden.json") if args.rise_set else OUT_PATH)
  destination.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
  print(f"[ make_bindings_golden ] {total} points -> {destination}")


if __name__ == "__main__":
  main()
