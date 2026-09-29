#!/usr/bin/env python3
#
# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import argparse
import json
import math
import random
import time

from dataclasses import asdict
from pathlib import Path
from typing import Final

from planet_horizons_crawler import (
  HORIZONS_API_VERSION,
  SUN,
  EventRow,
  HorizonsRow,
  Target,
  angular_separation_deg,
  annotate_events,
  derive_events,
  fetch_horizons,
  fetch_horizons_batched,
  fetch_horizons_range,
  horizons_params,
  require_same_census,
)


PLUTO: Final[Target] = Target("PLUTO", "999", "Pluto", "plu060_merged", "DE441", "")
SEED: Final[int] = 298
RANDOM_COUNT: Final[int] = 256
DIRECTED_EPOCHS: Final[tuple[float, ...]] = (
  2409543.5,
  2415020.5,
  2448908.5,
  2451545.0,
  2460676.5,
  2469807.5,
  2475818.5,
  2475819.5,
  2475842.5,
  2475843.5,
  2488068.5,
)
RETAINED_POSITION_EPOCHS: Final[tuple[float, ...]] = (
  2409543.5,
  2448908.5,
  2451545.0,
  2460697.020714,
  2470066.864649,
  2475818.5,
  2475819.5,
  2475842.5,
  2475843.5,
  2487903.349095,
  2488068.5,
)
DIRECTION_CENTERS: Final[tuple[float, ...]] = (
  2475827.5,
  2475834.5,
  2475914.5,
  2475992.5,
  2475999.5,
  2476095.5,
)
YEAR_BOUNDS_2025: Final[tuple[float, float]] = (2460676.500800741, 2461041.500800741)


def calibration_epochs() -> tuple[float, ...]:
  rng = random.Random(SEED)
  random_epochs = tuple(round(rng.uniform(2409543.5, 2488069.499999), 6) for _ in range(RANDOM_COUNT))
  epochs = tuple(sorted(set((*random_epochs, *DIRECTED_EPOCHS))))
  if len(random_epochs) != RANDOM_COUNT or len(epochs) != RANDOM_COUNT + len(DIRECTED_EPOCHS):
    raise RuntimeError("Pluto calibration sample construction changed")
  return epochs


def row_dict(row: HorizonsRow) -> dict[str, float | str]:
  return asdict(row)


def retained_geometry() -> list[dict[str, float | str]]:
  planet_rows = fetch_horizons(PLUTO, RETAINED_POSITION_EPOCHS, "10,20,23,31,43")
  time.sleep(0.15)
  sun_rows = fetch_horizons(SUN, RETAINED_POSITION_EPOCHS, "31")
  result = []
  for planet, sun in zip(planet_rows, sun_rows, strict=True):
    separation = angular_separation_deg(sun, planet)
    if abs(separation - planet.elongation_deg) > 0.0002:
      raise RuntimeError(f"Pluto quantity 23 differs from quantity 31 at JDE {planet.jde}")
    expected_fraction = (1.0 + math.cos(math.radians(planet.phase_angle_deg))) / 2.0
    if abs(expected_fraction - planet.illuminated_fraction) > 0.000002:
      raise RuntimeError(f"Pluto quantity 10 differs from quantity 43 at JDE {planet.jde}")
    result.append({**row_dict(planet), "derived_separation_deg": separation})
  return result


def direction_witnesses() -> list[dict[str, float | bool]]:
  epochs = tuple(value for center in DIRECTION_CENTERS for value in (center - 0.5, center + 0.5))
  rows = fetch_horizons(PLUTO, epochs, "31")
  result = []
  for index, center in enumerate(DIRECTION_CENTERS):
    before = rows[index * 2]
    after = rows[(index * 2) + 1]
    delta = math.remainder(after.lon_deg - before.lon_deg, 360.0)
    result.append({"jde_tt": center, "centered_delta_deg": delta, "retrograde": delta < 0.0})
  return result


def event_census() -> dict[str, object]:
  start_jde, end_jde = YEAR_BOUNDS_2025
  meshes: dict[int, tuple[EventRow, ...]] = {}
  for step_minutes in (1440, 360, 180):
    scan_start = start_jde - 1.0
    scan_end = end_jde + 1.0
    sun_rows = fetch_horizons_range(SUN, scan_start, scan_end, step_minutes, "31")
    time.sleep(0.15)
    planet_rows = fetch_horizons_range(PLUTO, scan_start, scan_end, step_minutes, "10,23,31,43")
    meshes[step_minutes] = derive_events(PLUTO, 2025, start_jde, end_jde, planet_rows, sun_rows)
    time.sleep(0.15)
  require_same_census(PLUTO, 2025, meshes[1440], meshes[180], "1-day")
  require_same_census(PLUTO, 2025, meshes[360], meshes[180], "6-hour")
  annotated = annotate_events(PLUTO, meshes[180])
  spread = max(abs(coarse.jde - fine.jde) for coarse, fine in zip(meshes[360], meshes[180], strict=True))
  return {
    "mesh_events": {str(step): [asdict(event) for event in rows] for step, rows in meshes.items()},
    "retained_events": [asdict(event) for event in annotated],
    "six_hour_vs_three_hour_max_spread_days": spread,
  }


def crawl() -> dict[str, object]:
  epochs = calibration_epochs()
  positions = fetch_horizons_batched(PLUTO, epochs, "20,23,31")
  time.sleep(0.15)
  geometry = retained_geometry()
  time.sleep(0.15)
  directions = direction_witnesses()
  time.sleep(0.15)
  events = event_census()
  request = horizons_params(PLUTO, (2451545.0,), "20,23,31")
  return {
    "source": {
      "api_version": HORIZONS_API_VERSION,
      "target": "Pluto (999)",
      "target_source": PLUTO.target_source,
      "center": "Earth (399)",
      "center_source": PLUTO.center_source,
      "request_pins": {
        key: request[key]
        for key in (
          "APPARENT",
          "CAL_TYPE",
          "CENTER",
          "COMMAND",
          "QUANTITIES",
          "RANGE_UNITS",
          "REF_SYSTEM",
          "TIME_TYPE",
        )
      },
    },
    "sample": {
      "seed": SEED,
      "random_count": RANDOM_COUNT,
      "directed_count": len(DIRECTED_EPOCHS),
      "total_count": len(positions),
    },
    "positions": [row_dict(row) for row in positions],
    "retained_geometry": geometry,
    "directions": directions,
    "events": events,
  }


def main() -> None:
  parser = argparse.ArgumentParser(description="Generate Pluto JPL Horizons validation data")
  parser.add_argument("output", type=Path, help="JSON output path")
  args = parser.parse_args()
  report = crawl()
  args.output.parent.mkdir(parents=True, exist_ok=True)
  args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
  sample = report["sample"]
  event_rows = report["events"]["retained_events"]
  print(
    f"API VERSION {HORIZONS_API_VERSION}; Pluto (999) plu060_merged; Earth (399) DE441; "
    f"seed={sample['seed']} random={sample['random_count']} directed={sample['directed_count']} "
    f"total={sample['total_count']} events={len(event_rows)}"
  )


if __name__ == "__main__":
  main()
