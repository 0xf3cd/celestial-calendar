# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Emit observer fixtures from retained external-oracle receipts, without importing the library.

Inputs are SOFA iauGd2gce output, controlled-distance PyMeeus output and a frozen
Horizons calibration receipt. See observer_reference.md for the schemas and replay.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path


RECEIPTS = Path(__file__).resolve().parents[1] / "src/test/provenance/observer"


def load_receipts(paths: list[Path], pins_path: Path) -> list[dict]:
  """Check complete frozen receipt bytes before deriving fixtures or tolerance bands."""
  pins = json.loads(pins_path.read_text())
  result = []
  for path in paths:
    raw = path.read_bytes()
    expected = pins["receipts"][path.name]
    assert hashlib.sha256(raw).hexdigest() == expected["sha256"], f"receipt identity: {path.name}"
    data = json.loads(raw)
    assert len(data["cases"]) == expected["rows"], f"receipt row count: {path.name}"
    result.append(data)
  return result


def table(name: str, rows: list[list[float]]) -> str:
  """Align each column across the complete initializer block."""
  cells = [[repr(float(value)) for value in row] for row in rows]
  widths = [max(len(row[index]) for row in cells) for index in range(len(cells[0]))]
  body = "\n".join(
    "  { " + ", ".join(cell.rjust(width) for cell, width in zip(row, widths, strict=True)) + " }," for row in cells
  )
  return f"inline constexpr std::array<std::array<double, {len(widths)}>, {len(cells)}> {name} {{{{\n{body}\n}}}};\n"


def bounds(rows: list[dict], actual_key: str) -> list[float]:
  """Round three times the frozen per-column model gap upward to one significant digit."""
  result = []
  for column in range(5):
    differences = []
    for row in rows:
      difference = row[actual_key][column] - row["expected"][column]
      if column in (0, 3):
        difference = math.remainder(difference, 360.0)
      differences.append(abs(difference))
    value = 3.0 * max(differences)
    quantum = 10.0 ** math.floor(math.log10(value))
    result.append(math.ceil(value / quantum) * quantum)
  return result


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--sofa", type=Path, default=RECEIPTS / "sofa-observer-preflight.json")
  parser.add_argument("--pymeeus", type=Path, default=RECEIPTS / "pymeeus-matched-constant-preflight.json")
  parser.add_argument("--horizons", type=Path, default=RECEIPTS / "calibration-results.json")
  parser.add_argument("--pins", type=Path, default=RECEIPTS / "SOURCE_PINS.json")
  parser.add_argument("--output", type=Path, required=True)
  args = parser.parse_args()
  sofa, pymeeus, horizons = load_receipts([args.sofa, args.pymeeus, args.horizons], args.pins)
  assert sofa["seed"] == pymeeus["seed"] == 42
  assert sofa["a_m"] == 6378137.0 and sofa["flattening"] == 1.0 / 298.257223563
  text = (
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
 */

// Retained material boundaries: V20 SOFA 2023-10-11, V15 PyMeeus and V06 JPL Horizons output.
// These numeric fixtures remain under their source terms, outside the project MIT grant.
// Collected 2026-11-07, seed 42 for SOFA/PyMeeus; Horizons epochs/sites are explicit, not random.
// Reproduce with statistics/observer_reference.py; source conditions: statistics/observer_reference.md.
#pragma once

#include <array>

namespace astro::observer::test::reference {

// Longitude/latitude degrees, ellipsoidal height metres, SOFA Cartesian x/y/z metres.
"""
  )
  text += table(
    "SOFA", [[r["longitude_deg"], r["latitude_deg"], r["height_m"], *r["sofa_xyz_m"]] for r in sofa["cases"]]
  )
  text += (
    "\n// RA/Dec degrees, physical AU, latitude degrees, height metres, H degrees, actual oracle AU, output RA/Dec.\n"
  )
  text += table(
    "PYMEEUS",
    [
      [
        r[k]
        for k in (
          "ra_deg",
          "dec_deg",
          "distance_au",
          "latitude_deg",
          "height_m",
          "hour_angle_deg",
          "oracle_distance_au",
          "matched_ra_deg",
          "matched_dec_deg",
        )
      ]
      for r in pymeeus["cases"]
    ],
  )
  text += (
    "\n// Target, longitude/latitude degrees, height km, JD UT1/JDE TT, GAST degrees, Horizons geocentric RA/Dec/AU,\n"
  )
  text += "// Horizons topocentric RA/Dec/AU/azimuth/altitude, all angles degrees.\n"
  text += table(
    "HORIZONS",
    [
      [
        r["target_code"],
        *r["site"][1:4],
        r["jd_ut1"],
        r["jde_tt"],
        r["gast_deg"],
        *r["horizons_geocentric"],
        *r["expected"],
      ]
      for r in horizons["cases"]
    ],
  )
  for name, key in (("REDUCTION_TOL", "horizons_reduced"), ("PROVIDER_TOL", "native_reduced")):
    columns = "RA/Dec degrees, AU, azimuth/altitude degrees"
    text += f"\n// Per-target {name}: {columns}; 3x frozen preflight gaps, rounded up.\n"
    text += table(name, [bounds([r for r in horizons["cases"] if r["target_code"] == code], key) for code in range(10)])
  text += "\n} // namespace astro::observer::test::reference\n"
  args.output.write_text(text)


if __name__ == "__main__":
  main()
