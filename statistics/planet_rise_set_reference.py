#!/usr/bin/env python3
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Replay pre-feature planetary event calibration and emit fixtures without importing the library."""

import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECEIPT = ROOT / "src/test/provenance/planet-rise-set/calibration.json"
FIXTURE = ROOT / "src/test/astro/planet_rise_set_reference_data.hpp"


def replay(data: dict) -> None:
  """Check reference brackets, separate numerical/clock comparisons and frozen regression allowances."""
  maxima = {}
  count = 0
  numerical_max = 0.0
  width_max = 0.0
  for row in data["cases"]:
    for event in row["events"]:
      lo, hi = (event[key]["jde_tt"] for key in ("reference_lower", "reference_upper"))
      width = (hi - lo) * 86400
      assert 0 < width <= 0.0101
      assert width == event["reference_width_seconds"]
      key = "hour_angle_deg" if event["kind"] == "transit" else "altitude_deg"
      threshold = 0 if event["kind"] == "transit" else -34 / 60
      a, b = (event[key2][key] - threshold for key2 in ("reference_lower", "reference_upper"))
      assert a > 0 >= b if event["kind"] == "set" else a < 0 <= b
      native = event["native_jde_tt"]
      lower, upper = event["independent_default_clock_bracket_jde_tt"]
      numerical = max(abs(native - lower), abs(native - upper)) * 86400
      assert 0 < (upper - lower) * 86400 <= 0.00011
      assert numerical == event["numerical_solver_error_bound_seconds"] and numerical <= 0.0002
      source_lo, source_hi = event["source_TDB_UT1_clock_bracket_jde_tt"]
      assert event["measured_clock_shift_seconds"] == ((lower + upper) - (source_lo + source_hi)) * 43200
      assert event["source_clock_physical_mid_gap_seconds"] == ((source_lo + source_hi) - (lo + hi)) * 43200
      target = str(row["case"]["target"])
      maxima[target] = max(maxima.get(target, 0), max(abs(native - lo), abs(native - hi)) * 86400)
      numerical_max, width_max = max(numerical_max, numerical), max(width_max, width)
      count += 1
  assert len(data["cases"]) == 12 and count == 32 and set(maxima) == set(map(str, range(2, 10)))
  assert numerical_max == data["solver_error_max_seconds"] and width_max == data["reference_bracket_max_seconds"]
  for target, gap in maxima.items():
    allowance = 3 * gap + 0.0021
    quantum = 10 ** math.floor(math.log10(allowance))
    assert data["full_model_test_tolerances_seconds"][target] == math.ceil(allowance / quantum) * quantum


def verify_raw(data: dict, directory: Path) -> None:
  """Verify original response bytes and reconstruct final scalar brackets from Horizons Q2/Q4/Q7."""
  for source in data["source_requests"]:
    path = directory / source["path"]
    assert json.loads(path.read_text())["params"] == source["params"]
    raw = path.with_name(path.name.replace(".request.json", ".response.txt")).read_bytes()
    assert len(raw) == source["response_bytes"] and hashlib.sha256(raw).hexdigest() == source["response_sha256"]
    header, block = raw.decode().split("$$SOE", 1)
    assert "NO (AIRLESS)" in header
    assert all(identity in header for identity in source["source_identity"])
    labels = next(line for line in header.splitlines() if "Date_" in line and "L_Ap_Sid_Time" in line)
    columns = [value.strip() for value in next(csv.reader([labels]))]
    assert "Date_________JDTT" in columns
    samples = []
    for values in csv.reader(io.StringIO(block.split("$$EOE")[0])):
      if values and any(value.strip() for value in values):
        assert len(values) == len(columns)
        samples.append({column: value.strip() for column, value in zip(columns, values, strict=True) if column})
    if path.name != "refine-0.01.request.json":
      continue
    case = next(row for row in data["cases"] if row["case"]["id"] == path.parent.name)
    for event in case["events"]:
      for endpoint in ("reference_lower", "reference_upper"):
        expected = event[endpoint]
        sample = next(row for row in samples if float(row["Date_________JDTT"]) == expected["jde_tt"])
        assert float(sample["Elevation_(a-app)"]) == expected["altitude_deg"]
        assert float(sample["R.A.__(a-app)"]) == expected["ra_deg"]
        assert float(sample["DEC___(a-app)"]) == expected["dec_deg"]
        assert (
          math.remainder(15 * float(sample["L_Ap_Sid_Time"]) - expected["ra_deg"], 360) == expected["hour_angle_deg"]
        )
        assert float(sample["TDB-UT"]) - float(sample["UT1-UTC"]) == expected["TDB_UT1_seconds"]


def aligned(rows: list[list], row_type: str = "") -> str:
  values = [[repr(v) for v in row] for row in rows]
  widths = [max(len(row[index]) for row in values) for index in range(len(values[0]))]
  prefix = f"{row_type} " if row_type else ""
  return "\n".join(
    "  " + prefix + "{ " + ", ".join(v.rjust(w) for v, w in zip(row, widths, strict=True)) + " }," for row in values
  )


def render(data: dict) -> str:
  """Emit explicit day inputs and event brackets, aligned over each whole table."""
  days, events = [], []
  for index, row in enumerate(data["cases"]):
    case = row["case"]
    mask = sum(1 << ("rise", "transit", "set").index(e["kind"]) for e in row["events"])
    days.append(
      [
        case["target"],
        *map(int, case["date"].split("-")),
        case["latitude"],
        case["longitude"],
        case["height_km"],
        row["native_polar"],
        mask,
      ]
    )
    for event in row["events"]:
      events.append(
        [
          index,
          ("rise", "transit", "set").index(event["kind"]),
          event["reference_lower"]["jde_tt"],
          event["reference_upper"]["jde_tt"],
          *event["independent_default_clock_bracket_jde_tt"],
          data["full_model_test_tolerances_seconds"][str(case["target"])],
        ]
      )
  banner = (
    """/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: """
    + "MIT"
    + """
 */"""
  )
  return (
    banner
    + """

// Retained material boundary: JPL Horizons numeric output remains under its source terms,
// outside the project MIT grant. See src/test/provenance/planet-rise-set/ATTRIBUTION.md.
// Collected 2026-11-09; fixed sites/epochs, no random seed. Regenerate with statistics/planet_rise_set_reference.py.
#pragma once

#include <array>
#include <cstdint>

namespace astro::rise_set::planet::test::reference {

/** @brief Exact target, UT1 date, WGS84 site and expected daily event topology. */
struct Day {
  uint32_t target;
  int32_t year;
  uint32_t month;
  uint32_t day;
  double latitude_deg;
  double longitude_deg;
  double height_km;
  uint32_t polar;
  uint32_t event_mask;
};

/** @brief Source and independent native-clock TT brackets, with the frozen model allowance. */
struct Event {
  uint32_t day_index;
  uint32_t event_index;
  double source_lower_jde_tt;
  double source_upper_jde_tt;
  double native_lower_jde_tt;
  double native_upper_jde_tt;
  double allowance_seconds;
};

// Columns follow Day; event_mask bits are rise/transit/set from least to most significant.
// NOLINTBEGIN(modernize-use-designated-initializers): Preserve whole-table column alignment.
inline constexpr std::array DAYS {
"""
    + aligned(days, "Day")
    + """
};

// Columns follow Event; event_index is rise=0, transit=1, set=2.
inline constexpr std::array EVENTS {
"""
    + aligned(events, "Event")
    + """
};
// NOLINTEND(modernize-use-designated-initializers)

} // namespace astro::rise_set::planet::test::reference
"""
  )


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--check", action="store_true")
  parser.add_argument("--raw-dir", type=Path)
  parser.add_argument("--output", type=Path, default=FIXTURE)
  args = parser.parse_args()
  pins = json.loads((RECEIPT.parent / "SOURCE_PINS.json").read_text())
  assert hashlib.sha256(RECEIPT.read_bytes()).hexdigest() == pins["calibration_sha256"]
  data = json.loads(RECEIPT.read_text())
  replay(data)
  if args.raw_dir:
    verify_raw(data, args.raw_dir)
  output = render(data)
  if args.check:
    assert args.output.read_text() == output
  else:
    args.output.write_text(output)
  print("PASS 12 days / 32 reference brackets, numerical/clock separation and frozen allowances")


if __name__ == "__main__":
  main()
