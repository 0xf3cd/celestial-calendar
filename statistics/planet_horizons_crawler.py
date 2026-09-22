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

import sys
import time

from dataclasses import dataclass
from typing import Final

import requests


HORIZONS_URL: Final[str] = "https://ssd.jpl.nasa.gov/api/horizons.api"
HORIZONS_API_VERSION: Final[str] = "1.2"
# Explicit TLIST responses stop after 80 epochs, so the daily scan is replayed in batches.
HORIZONS_BATCH_SIZE: Final[int] = 80

# Six fixed TT epochs span 1901-2094 and include Meeus Example 33.a. Applying the same epochs to
# every planet samples different orbital phases and elongations without selecting on this library's
# output. Quantity 23 records elongation so the light-deflection residual can be audited separately.
EPOCHS: Final[tuple[float, ...]] = (
  2415385.5,
  2433651.5,
  2448976.5,
  2451545.0,
  2461050.5,
  2486166.25,
)
# The earliest daily minimum elongation in 2026 supplies one directed conjunction row per planet.
# This scan is source-only: selection never consults this library's output.
NEAR_SUN_SCAN_EPOCHS: Final[tuple[float, ...]] = tuple(2461041.5 + day for day in range(365))


@dataclass(frozen=True)
class Target:
  enum_name: str
  command: str
  horizons_name: str
  target_source: str
  center_source: str
  pymeeus_module: str


TARGETS: Final[tuple[Target, ...]] = (
  Target("MERCURY", "199", "Mercury", "DE441", "DE441", "Mercury"),
  Target("VENUS", "299", "Venus", "DE441", "DE441", "Venus"),
  Target("MARS", "499", "Mars", "mar099", "DE441", "Mars"),
  Target("JUPITER", "599", "Jupiter", "jup365_merged", "DE441", "Jupiter"),
  Target("SATURN", "699", "Saturn", "sat441l", "DE441", "Saturn"),
  Target("URANUS", "799", "Uranus", "ura184_merged", "DE441", "Uranus"),
  Target("NEPTUNE", "899", "Neptune", "nep098_merged", "nep098_merged", "Neptune"),
)


@dataclass(frozen=True)
class HorizonsRow:
  jde: float
  date: str
  lon_deg: float
  lat_deg: float
  range_au: float
  range_rate_km_s: float
  elongation_deg: float


def horizons_params(target: Target, epochs: tuple[float, ...]) -> dict[str, str]:
  return {
    "format": "text",
    "COMMAND": f"'{target.command}'",
    "OBJ_DATA": "'NO'",
    "MAKE_EPHEM": "'YES'",
    "EPHEM_TYPE": "'OBSERVER'",
    "CENTER": "'500@399'",
    "TLIST": "'" + " ".join(f"{jde:.6f}" for jde in epochs) + "'",
    "TLIST_TYPE": "'JD'",
    "TIME_TYPE": "'TT'",
    "QUANTITIES": "'20,23,31'",
    "ANG_FORMAT": "'DEG'",
    "EXTRA_PREC": "'YES'",
    "CAL_FORMAT": "'BOTH'",
    "CSV_FORMAT": "'YES'",
  }


def fetch_horizons(target: Target, epochs: tuple[float, ...]) -> tuple[HorizonsRow, ...]:
  response = requests.get(HORIZONS_URL, params=horizons_params(target, epochs), timeout=60)
  response.raise_for_status()
  text = response.text

  if f"API VERSION: {HORIZONS_API_VERSION}" not in text:
    raise RuntimeError(f"unexpected Horizons API version; expected {HORIZONS_API_VERSION}")
  target_line = next((line for line in text.splitlines() if line.startswith("Target body name:")), "")
  center_line = next((line for line in text.splitlines() if line.startswith("Center body name:")), "")
  if f"{target.horizons_name} ({target.command})" not in target_line:
    raise RuntimeError(f"Horizons response is not for {target.horizons_name} ({target.command})")
  if "Earth (399)" not in center_line:
    raise RuntimeError(f"Horizons response for {target.horizons_name} is not geocentric")
  if f"{{source: {target.target_source}}}" not in target_line:
    raise RuntimeError(f"Horizons response for {target.horizons_name} target does not use {target.target_source}")
  if f"{{source: {target.center_source}}}" not in center_line:
    raise RuntimeError(f"Horizons response for {target.horizons_name} center does not use {target.center_source}")

  lines = text.splitlines()
  soe, eoe = lines.index("$$SOE"), lines.index("$$EOE")
  header = next(
    ([cell.strip() for cell in line.split(",")] for line in reversed(lines[:soe]) if "ObsEcLon" in line),
    None,
  )
  if header is None:
    raise RuntimeError("Horizons column header line not found")
  jd_i = next(i for i, name in enumerate(header) if "JD" in name)
  if "JDTT" not in header[jd_i]:
    raise RuntimeError(f"Horizons JD column is not TT: {header[jd_i]!r}")
  columns = {name: header.index(name) for name in ("delta", "deldot", "S-O-T", "ObsEcLon", "ObsEcLat")}

  rows = []
  for line in lines[soe + 1 : eoe]:
    cells = [cell.strip() for cell in line.split(",")]
    rows.append(
      HorizonsRow(
        jde=float(cells[jd_i]),
        date=cells[0],
        lon_deg=float(cells[columns["ObsEcLon"]]),
        lat_deg=float(cells[columns["ObsEcLat"]]),
        range_au=float(cells[columns["delta"]]),
        range_rate_km_s=float(cells[columns["deldot"]]),
        elongation_deg=float(cells[columns["S-O-T"]]),
      )
    )
  returned_epochs = tuple(row.jde for row in rows)
  if returned_epochs != epochs:
    raise RuntimeError(
      f"Horizons epochs differ for {target.horizons_name}: expected {epochs!r}, got {returned_epochs!r}"
    )
  return tuple(rows)


def fetch_horizons_batched(target: Target, epochs: tuple[float, ...]) -> tuple[HorizonsRow, ...]:
  rows = []
  for offset in range(0, len(epochs), HORIZONS_BATCH_SIZE):
    rows.extend(fetch_horizons(target, epochs[offset : offset + HORIZONS_BATCH_SIZE]))
    time.sleep(0.15)
  return tuple(rows)


def pymeeus_position(target: Target, jde: float) -> tuple[float, float]:
  from importlib import import_module

  from pymeeus.Epoch import Epoch

  module = import_module(f"pymeeus.{target.pymeeus_module}")
  implementation = getattr(module, target.pymeeus_module)
  right_ascension, declination, _elongation = implementation.geocentric_position(Epoch(jde))
  return right_ascension(), declination()


def emit_horizons(target: Target, rows: tuple[HorizonsRow, ...]) -> None:
  print(f"\n// {target.horizons_name} ({target.command})")
  for row in rows:
    print(
      f"  {{ Planet::{target.enum_name:<7}, {row.jde:13.6f}, {row.lon_deg:12.7f}, "
      f"{row.lat_deg:11.7f}, {row.range_au:15.12f}, {row.elongation_deg:9.4f} }},"
      f"  // {row.date} TT, rdot {row.range_rate_km_s:+.4f} km/s"
    )


def emit_pymeeus(target: Target) -> None:
  print(f"\n// {target.horizons_name}")
  for jde in EPOCHS:
    right_ascension, declination = pymeeus_position(target, jde)
    print(f"  {{ Planet::{target.enum_name:<7}, {jde:11.2f}, {right_ascension:14.10f}, {declination:14.10f} }},")


def main() -> None:
  all_rows = []
  conjunction_rows = []
  for target in TARGETS:
    base_rows = fetch_horizons(target, EPOCHS)
    time.sleep(0.15)
    scan_rows = fetch_horizons_batched(target, NEAR_SUN_SCAN_EPOCHS)
    directed = min(scan_rows, key=lambda row: (row.elongation_deg, row.jde))
    all_rows.append((target, base_rows))
    conjunction_rows.append((target, (directed,)))
    print(
      f"{target.horizons_name}: 2026 daily minimum JDE {directed.jde:.1f}, "
      f"elongation {directed.elongation_deg:.4f} deg",
      file=sys.stderr,
    )
    time.sleep(0.15)

  print(
    "// JPL Horizons API 1.2, geocenter, TT, apparent true ecliptic of date; target sources "
    "DE441/mar099/jup365_merged/sat441l/ura184_merged/nep098_merged; center DE441 except "
    "Neptune nep098_merged"
  )
  for target, rows in all_rows:
    emit_horizons(target, rows)

  print("\n// Earliest daily minimum elongation in 2026, selected from Horizons output only")
  for target, rows in conjunction_rows:
    emit_horizons(target, rows)

  print("\n// PyMeeus 0.5.12 geocentric_position, apparent RA/Dec")
  for target in TARGETS:
    emit_pymeeus(target)


if __name__ == "__main__":
  main()
