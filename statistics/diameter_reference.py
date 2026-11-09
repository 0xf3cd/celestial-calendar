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

"""Replay frozen pre-diameter calibration without importing the production geometry."""

import argparse
import decimal
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECEIPT = ROOT / "src/test/provenance/diameter/calibration.json"
FIXTURE = ROOT / "src/test/astro/diameter_reference_data.hpp"


def sphere(radius: float, distance: float) -> float:
  """Tangent triangle, with radius in km and distance in AU."""
  r = radius / 149597870.7
  return math.degrees(2 * math.atan2(r, math.sqrt(distance - r) * math.sqrt(distance + r))) * 3600


def half_digit(text: str) -> float:
  return float(decimal.Decimal(5).scaleb(decimal.Decimal(text).as_tuple().exponent - 1))


def replay(data: dict) -> None:
  """Separate source printed-digit geometry from the frozen provider model gap."""
  maxima = {}
  for row in data["cases"]:
    radius, distance = row["radius_km"], float(row["distance_printed_au"])
    computed = sphere(radius, distance)
    assert abs(computed - row["source_geometry_arcsec"]) <= 2 * math.ulp(computed)
    propagated = max(
      abs(sphere(radius, distance + half_digit(row["distance_printed_au"])) - computed),
      abs(sphere(radius, distance - half_digit(row["distance_printed_au"])) - computed),
    )
    allowance = half_digit(row["angle_printed_arcsec"]) + propagated + 8 * math.ulp(computed)
    assert abs(row["rounding_allowance_arcsec"] - allowance) <= 2 * math.ulp(computed)
    assert abs(computed - float(row["angle_printed_arcsec"])) <= row["rounding_allowance_arcsec"]
    baseline = sphere(radius, row["baseline_distance_au"])
    assert abs(baseline - row["baseline_diameter_arcsec"]) <= 2 * math.ulp(baseline)
    key = str(row["target"]) + ("-geo" if row["site_lon_lat_km"][2] == -9999 else "-topo")
    maxima[key] = max(maxima.get(key, 0), abs(baseline - float(row["angle_printed_arcsec"])))
  assert maxima == data["measured_max_gaps_arcsec"]
  for key, gap in maxima.items():
    exponent = 10 ** math.floor(math.log10(3 * gap))
    assert data["provider_bounds_arcsec"][key] == math.ceil(3 * gap / exponent) * exponent


def render(data: dict) -> str:
  """Emit per-column aligned numeric fixtures from the pinned receipt."""
  rows = []
  for row in data["cases"]:
    key = str(row["target"]) + ("-geo" if row["site_lon_lat_km"][2] == -9999 else "-topo")
    values = [
      row["target"],
      row["jde_tt"],
      row["jd_ut1"],
      *row["site_lon_lat_km"],
      row["radius_km"],
      row["radius_definition"],
      float(row["distance_printed_au"]),
      float(row["angle_printed_arcsec"]),
      row["rounding_allowance_arcsec"],
      row["baseline_distance_au"],
      data["provider_bounds_arcsec"][key],
    ]
    rows.append([str(v) if isinstance(v, int) else repr(v) for v in values])
  widths = [max(len(row[index]) for row in rows) for index in range(len(rows[0]))]
  body = "\n".join("  Row { " + ", ".join(v.rjust(w) for v, w in zip(row, widths, strict=True)) + " }," for row in rows)
  return (
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

// Retained material boundary: JPL Horizons numeric output remains under its source terms,
// outside the project MIT grant. See src/test/provenance/diameter/ATTRIBUTION.md.
// Collected 2026-11-08/09; exact requests, printed digits and pre-feature calibration are in calibration.json.
// Regenerate with statistics/diameter_reference.py; no production diameter code is imported.
#pragma once
#include <array>
#include <cstdint>

namespace astro::apparent_diameter::test::reference {
/** @brief Source radius/range/printed angle and independently calibrated provider comparison. */
struct Row {
  uint32_t target;
  double jde_tt;
  double jd_ut1;
  double longitude_deg;
  double latitude_deg;
  double height_km;
  double radius_km;
  uint32_t definition;
  double distance_au;
  double printed_arcsec;
  double rounding_arcsec;
  double baseline_distance_au;
  double provider_arcsec;
};
// Columns follow Row; -9999 km marks a geocentric row, not a physical site.
// NOLINTBEGIN(modernize-use-designated-initializers): Preserve whole-table column alignment.
inline constexpr std::array ROWS {
"""
    + body
    + """
};
// NOLINTEND(modernize-use-designated-initializers)
} // namespace astro::apparent_diameter::test::reference
"""
  )


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--check", action="store_true")
  parser.add_argument("--output", type=Path, default=FIXTURE)
  args = parser.parse_args()
  pins = json.loads((RECEIPT.parent / "SOURCE_PINS.json").read_text())
  assert hashlib.sha256(RECEIPT.read_bytes()).hexdigest() == pins["calibration_sha256"]
  data = json.loads(RECEIPT.read_text())
  replay(data)
  output = render(data)
  if args.check:
    assert args.output.read_text() == output
  else:
    args.output.write_text(output)
  print(f"PASS {len(data['cases'])} independent disk rows, source rounding and pre-feature provider calibration")


if __name__ == "__main__":
  main()
