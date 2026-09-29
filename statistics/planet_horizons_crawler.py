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
import math
import sys
import time

from dataclasses import dataclass
from typing import Final

import requests


HORIZONS_URL: Final[str] = "https://ssd.jpl.nasa.gov/api/horizons.api"
HORIZONS_API_VERSION: Final[str] = "1.2"
# The API silently truncates explicit TLIST responses after 80 epochs. Forty also leaves URL margin.
HORIZONS_BATCH_SIZE: Final[int] = 40
# Horizons can quantize a six-decimal TLIST token by a few binary64 ulps when echoing JDTT.
EPOCH_ECHO_TOLERANCE_DAYS: Final[float] = 1e-8
QUANTITY_COLUMNS: Final[dict[str, tuple[str, ...]]] = {
  "10": ("Illu%",),
  "20": ("delta", "deldot"),
  "23": ("S-O-T",),
  "31": ("ObsEcLon", "ObsEcLat"),
  "43": ("phi",),
}

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
# This source-only daily scan supplies the 2026 minimum-elongation rows and complete retrograde
# intervals over 2025-2027. Selection never consults this library's output.
RETROGRADE_SCAN_EPOCHS: Final[tuple[float, ...]] = tuple(2460676.5 + day for day in range(1095))


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
SUN: Final[Target] = Target("SUN", "10", "Sun", "DE441", "DE441", "")


@dataclass(frozen=True)
class HorizonsRow:
  jde: float
  date: str
  lon_deg: float
  lat_deg: float
  range_au: float = math.nan
  range_rate_km_s: float = math.nan
  elongation_deg: float = math.nan
  illuminated_fraction: float = math.nan
  phase_angle_deg: float = math.nan


@dataclass(frozen=True)
class EventRow:
  planet: str
  year: int
  kind: str
  jde: float
  elongation_deg: float = math.nan
  phase_angle_deg: float = math.nan
  illuminated_fraction: float = math.nan


def horizons_params(
  target: Target,
  epochs: tuple[float, ...],
  quantities: str = "20,23,31",
) -> dict[str, str]:
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
    "QUANTITIES": f"'{quantities}'",
    "ANG_FORMAT": "'DEG'",
    "EXTRA_PREC": "'YES'",
    "CAL_FORMAT": "'BOTH'",
    "CSV_FORMAT": "'YES'",
    "APPARENT": "'AIRLESS'",
    "REF_SYSTEM": "'ICRF'",
    "CAL_TYPE": "'GREGORIAN'",
    "RANGE_UNITS": "'AU'",
  }


def parse_horizons_response(
  target: Target,
  text: str,
  required_columns: tuple[str, ...],
  expected_epochs: tuple[float, ...] | None = None,
) -> tuple[HorizonsRow, ...]:
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
  if "Center-site name: GEOCENTRIC" not in text:
    raise RuntimeError(f"Horizons response for {target.horizons_name} is not from the geocenter site")
  if "Atmos refraction: NO (AIRLESS)" not in text:
    raise RuntimeError(f"Horizons response for {target.horizons_name} applies atmospheric refraction")
  if "Calendar mode   : Gregorian" not in text:
    raise RuntimeError(f"Horizons response for {target.horizons_name} does not use the Gregorian calendar")
  if "Units conversion: 1 au= 149597870.700 km" not in text:
    raise RuntimeError(f"Horizons response for {target.horizons_name} does not report ranges in AU")
  if target.command == "999":
    if "Rel. light bend : Sun" not in text or "{source: plu060_merged}" not in text:
      raise RuntimeError("Horizons response for Pluto has an unexpected relativistic-light-bend identity")
    quantity_31_phrases = (
      "Observer-centered IAU76/80 ecliptic-of-date longitude and latitude",
      "with light-time, gravitational deflection of",
      "light, and stellar aberrations.",
    )
    if any(phrase not in text for phrase in quantity_31_phrases):
      raise RuntimeError("Horizons quantity-31 semantics changed for Pluto")

  lines = text.splitlines()
  if lines.count("$$SOE") != 1 or lines.count("$$EOE") != 1:
    raise RuntimeError(f"Horizons response for {target.horizons_name} has invalid data markers")
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
  if any(header.count(name) != 1 for name in required_columns):
    raise RuntimeError(f"Horizons columns differ for {target.horizons_name}: {header!r}")
  columns = {name: header.index(name) for name in header if name}

  def optional(cells: list[str], name: str) -> float:
    if name not in columns:
      return math.nan
    value = cells[columns[name]]
    if value == "n.a.":
      raise RuntimeError(f"Horizons returned n.a. for {name} at {cells[jd_i]}")
    return float(value)

  rows = []
  for line in lines[soe + 1 : eoe]:
    cells = [cell.strip() for cell in line.split(",")]
    rows.append(
      HorizonsRow(
        jde=float(cells[jd_i]),
        date=cells[0],
        lon_deg=float(cells[columns["ObsEcLon"]]),
        lat_deg=float(cells[columns["ObsEcLat"]]),
        range_au=optional(cells, "delta"),
        range_rate_km_s=optional(cells, "deldot"),
        elongation_deg=optional(cells, "S-O-T"),
        illuminated_fraction=optional(cells, "Illu%") / 100.0,
        phase_angle_deg=optional(cells, "phi"),
      )
    )
  returned_epochs = tuple(row.jde for row in rows)
  if not rows:
    raise RuntimeError(f"Horizons returned no rows for {target.horizons_name}")
  if any(not math.isfinite(value) for row in rows for value in (row.jde, row.lon_deg, row.lat_deg)):
    raise RuntimeError(f"Horizons returned a non-finite coordinate for {target.horizons_name}")
  if any(right <= left for left, right in zip(returned_epochs, returned_epochs[1:], strict=False)):
    raise RuntimeError(f"Horizons epochs are not strictly increasing for {target.horizons_name}")
  if expected_epochs is not None:
    if len(returned_epochs) != len(expected_epochs) or any(
      abs(returned - expected) > EPOCH_ECHO_TOLERANCE_DAYS
      for returned, expected in zip(returned_epochs, expected_epochs, strict=False)
    ):
      raise RuntimeError(
        f"Horizons epochs differ for {target.horizons_name}: expected {expected_epochs!r}, got {returned_epochs!r}"
      )
  return tuple(rows)


def fetch_horizons(
  target: Target,
  epochs: tuple[float, ...],
  quantities: str = "20,23,31",
) -> tuple[HorizonsRow, ...]:
  requested_epochs = tuple(round(jde, 6) for jde in epochs)
  response = requests.get(HORIZONS_URL, params=horizons_params(target, requested_epochs, quantities), timeout=60)
  response.raise_for_status()
  required_columns = tuple(column for quantity in quantities.split(",") for column in QUANTITY_COLUMNS[quantity])
  return parse_horizons_response(target, response.text, required_columns, requested_epochs)


def fetch_horizons_batched(
  target: Target,
  epochs: tuple[float, ...],
  quantities: str = "20,23,31",
) -> tuple[HorizonsRow, ...]:
  rows = []
  for offset in range(0, len(epochs), HORIZONS_BATCH_SIZE):
    rows.extend(fetch_horizons(target, epochs[offset : offset + HORIZONS_BATCH_SIZE], quantities))
    time.sleep(0.15)
  return tuple(rows)


def horizons_range_params(
  target: Target,
  start_jde: float,
  end_jde: float,
  step_minutes: int,
  quantities: str,
) -> dict[str, str]:
  params = horizons_params(target, (), quantities)
  del params["TLIST"]
  del params["TLIST_TYPE"]
  params.update(
    {
      "START_TIME": f"'JD{start_jde:.9f}'",
      "STOP_TIME": f"'JD{end_jde:.9f}'",
      "STEP_SIZE": f"'{step_minutes} m'",
    }
  )
  return params


def fetch_horizons_range(
  target: Target,
  start_jde: float,
  end_jde: float,
  step_minutes: int,
  quantities: str,
) -> tuple[HorizonsRow, ...]:
  response = requests.get(
    HORIZONS_URL,
    params=horizons_range_params(target, start_jde, end_jde, step_minutes, quantities),
    timeout=120,
  )
  response.raise_for_status()
  required_columns = tuple(column for quantity in quantities.split(",") for column in QUANTITY_COLUMNS[quantity])
  return parse_horizons_response(target, response.text, required_columns)


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


def select_retrograde_rows(rows: tuple[HorizonsRow, ...]) -> tuple[tuple[HorizonsRow, bool, str], ...]:
  rates = tuple(
    (row, math.remainder(rows[index + 1].lon_deg - rows[index - 1].lon_deg, 360.0) / 2.0)
    for index, row in enumerate(rows[1:-1], start=1)
  )
  transitions = []
  for (before_row, before_rate), (after_row, after_rate) in zip(rates, rates[1:], strict=False):
    if (before_rate < 0.0) != (after_rate < 0.0):
      transitions.append((before_row.jde, after_row.jde, before_rate < 0.0, after_rate < 0.0))

  start_index = next(index for index, transition in enumerate(transitions[:-1]) if transition[3])
  interval = transitions[start_index : start_index + 2]
  by_jde = {row.jde: row for row in rows}
  selected = []
  for left_jde, right_jde, before_retrograde, after_retrograde in interval:
    selected.extend(
      (
        (by_jde[left_jde - 3.0], before_retrograde, "station-before"),
        (by_jde[right_jde + 3.0], after_retrograde, "station-after"),
      )
    )
  direct_row, _direct_rate = max(rates, key=lambda item: item[1])
  retrograde_row, _retrograde_rate = min(rates, key=lambda item: item[1])
  selected.extend(
    (
      (direct_row, False, "clear-direct"),
      (retrograde_row, True, "clear-retrograde"),
    )
  )
  return tuple(selected)


def select_wrap_rows(rows: tuple[HorizonsRow, ...]) -> tuple[tuple[HorizonsRow, bool], ...]:
  selected = []
  for index, row in enumerate(rows[1:-1], start=1):
    before = rows[index - 1].lon_deg
    after = rows[index + 1].lon_deg
    if (before > 350.0 and after < 10.0) or (before < 10.0 and after > 350.0):
      selected.append((row, math.remainder(after - before, 360.0) < 0.0))
  return tuple(selected)


def emit_retrograde(target: Target, rows: tuple[tuple[HorizonsRow, bool, str], ...]) -> None:
  print(f"\n// {target.horizons_name}")
  for row, retrograde, label in rows:
    print(f"  {{ Planet::{target.enum_name:<7}, {row.jde:13.6f}, {str(retrograde).lower():5} }},  // {label}")


def angular_separation_deg(source: HorizonsRow, target: HorizonsRow) -> float:
  source_lon = math.radians(source.lon_deg)
  source_lat = math.radians(source.lat_deg)
  target_lon = math.radians(target.lon_deg)
  target_lat = math.radians(target.lat_deg)
  source_vector = (
    math.cos(source_lat) * math.cos(source_lon),
    math.cos(source_lat) * math.sin(source_lon),
    math.sin(source_lat),
  )
  target_vector = (
    math.cos(target_lat) * math.cos(target_lon),
    math.cos(target_lat) * math.sin(target_lon),
    math.sin(target_lat),
  )
  dot = sum(lhs * rhs for lhs, rhs in zip(source_vector, target_vector, strict=True))
  cross = (
    (source_vector[1] * target_vector[2]) - (source_vector[2] * target_vector[1]),
    (source_vector[2] * target_vector[0]) - (source_vector[0] * target_vector[2]),
    (source_vector[0] * target_vector[1]) - (source_vector[1] * target_vector[0]),
  )
  return math.degrees(math.atan2(math.sqrt(sum(value * value for value in cross)), dot))


def unwrap(values: tuple[float, ...]) -> tuple[float, ...]:
  result = [values[0]]
  for value in values[1:]:
    result.append(result[-1] + math.remainder(value - result[-1], 360.0))
  return tuple(result)


def interpolate(lhs: float, rhs: float, fraction: float) -> float:
  return lhs + (fraction * (rhs - lhs))


def parabolic_extremum_jde(
  before: HorizonsRow,
  center: HorizonsRow,
  after: HorizonsRow,
  before_value: float,
  center_value: float,
  after_value: float,
) -> float:
  denominator = before_value - (2.0 * center_value) + after_value
  if denominator == 0.0:
    raise RuntimeError(f"flat source extremum around JDE {center.jde}")
  offset_steps = 0.5 * (before_value - after_value) / denominator
  if not -1.0 <= offset_steps <= 1.0:
    raise RuntimeError(f"source extremum escaped its bracket around JDE {center.jde}")
  return center.jde + (offset_steps * (after.jde - center.jde))


def event_kind_for_outer_grid(level_index: int) -> str:
  match level_index % 4:
    case 0:
      return "CONJUNCTION"
    case 1:
      return "EASTERN_QUADRATURE"
    case 2:
      return "OPPOSITION"
    case 3:
      return "WESTERN_QUADRATURE"
  raise AssertionError("unreachable quadrature remainder")


def derive_events(
  target: Target,
  year: int,
  start_jde: float,
  end_jde: float,
  planet_rows: tuple[HorizonsRow, ...],
  sun_rows: tuple[HorizonsRow, ...],
) -> tuple[EventRow, ...]:
  planet_epochs = tuple(row.jde for row in planet_rows)
  sun_epochs = tuple(row.jde for row in sun_rows)
  if planet_epochs != sun_epochs:
    raise RuntimeError(f"Sun and {target.horizons_name} event grids differ")

  planet_lon = unwrap(tuple(row.lon_deg for row in planet_rows))
  sun_lon = unwrap(tuple(row.lon_deg for row in sun_rows))
  difference = tuple(planet - sun for planet, sun in zip(planet_lon, sun_lon, strict=True))
  separation = tuple(angular_separation_deg(sun, planet) for planet, sun in zip(planet_rows, sun_rows, strict=True))
  inner = target.enum_name in {"MERCURY", "VENUS"}
  spacing = 360.0 if inner else 90.0
  events = []

  for index, (lhs, rhs) in enumerate(zip(difference, difference[1:], strict=False)):
    low, high = sorted((lhs, rhs))
    first_level = math.ceil(low / spacing)
    last_level = math.floor(high / spacing)
    for level_index in range(first_level, last_level + 1):
      target_difference = spacing * level_index
      if rhs == lhs:
        continue
      fraction = (target_difference - lhs) / (rhs - lhs)
      if not 0.0 <= fraction <= 1.0:
        continue
      jde = interpolate(planet_rows[index].jde, planet_rows[index + 1].jde, fraction)
      if inner:
        phase_angle = interpolate(
          planet_rows[index].phase_angle_deg,
          planet_rows[index + 1].phase_angle_deg,
          fraction,
        )
        kind = "INFERIOR_CONJUNCTION" if phase_angle >= 90.0 else "SUPERIOR_CONJUNCTION"
      else:
        kind = event_kind_for_outer_grid(level_index)
      events.append(EventRow(target.enum_name, year, kind, jde))

  for index in range(1, len(planet_rows) - 1):
    if inner and separation[index] > separation[index - 1] and separation[index] > separation[index + 1]:
      jde = parabolic_extremum_jde(
        planet_rows[index - 1],
        planet_rows[index],
        planet_rows[index + 1],
        separation[index - 1],
        separation[index],
        separation[index + 1],
      )
      direction = math.remainder(difference[index], 360.0)
      kind = "GREATEST_EASTERN_ELONGATION" if direction > 0.0 else "GREATEST_WESTERN_ELONGATION"
      events.append(EventRow(target.enum_name, year, kind, jde))

    if planet_lon[index] > planet_lon[index - 1] and planet_lon[index] > planet_lon[index + 1]:
      events.append(
        EventRow(
          target.enum_name,
          year,
          "STATION_RETROGRADE",
          parabolic_extremum_jde(
            planet_rows[index - 1],
            planet_rows[index],
            planet_rows[index + 1],
            planet_lon[index - 1],
            planet_lon[index],
            planet_lon[index + 1],
          ),
        )
      )
    if planet_lon[index] < planet_lon[index - 1] and planet_lon[index] < planet_lon[index + 1]:
      events.append(
        EventRow(
          target.enum_name,
          year,
          "STATION_DIRECT",
          parabolic_extremum_jde(
            planet_rows[index - 1],
            planet_rows[index],
            planet_rows[index + 1],
            planet_lon[index - 1],
            planet_lon[index],
            planet_lon[index + 1],
          ),
        )
      )

  owned = sorted((event for event in events if start_jde <= event.jde < end_jde), key=lambda event: event.jde)
  deduplicated = []
  for event in owned:
    if deduplicated and event.kind == deduplicated[-1].kind and abs(event.jde - deduplicated[-1].jde) < 0.5:
      continue
    deduplicated.append(event)
  return tuple(deduplicated)


def require_same_census(
  target: Target,
  year: int,
  candidate: tuple[EventRow, ...],
  reference: tuple[EventRow, ...],
  label: str,
) -> None:
  candidate_kinds = tuple(event.kind for event in candidate)
  reference_kinds = tuple(event.kind for event in reference)
  if candidate_kinds != reference_kinds:
    raise RuntimeError(
      f"{target.horizons_name} {year} {label} census differs: {candidate_kinds!r} != {reference_kinds!r}"
    )


def annotate_events(target: Target, rows: tuple[EventRow, ...]) -> tuple[EventRow, ...]:
  epochs = tuple(round(row.jde, 6) for row in rows)
  planet_rows = fetch_horizons(target, epochs, "10,23,31,43")
  time.sleep(0.15)
  sun_rows = fetch_horizons(SUN, epochs, "31")
  if tuple(row.jde for row in planet_rows) != tuple(row.jde for row in sun_rows):
    raise RuntimeError(f"Sun and {target.horizons_name} refined epochs differ")
  annotated = []
  for event, planet, sun in zip(rows, planet_rows, sun_rows, strict=True):
    derived_separation = angular_separation_deg(sun, planet)
    if abs(derived_separation - planet.elongation_deg) > 0.0002:
      raise RuntimeError(f"quantity 23 differs from quantity 31 separation for {target.horizons_name} at {planet.jde}")
    expected_fraction = (1.0 + math.cos(math.radians(planet.phase_angle_deg))) / 2.0
    if abs(expected_fraction - planet.illuminated_fraction) > 0.000002:
      raise RuntimeError(f"quantity 10 differs from quantity 43 phase angle for {target.horizons_name} at {planet.jde}")
    annotated.append(
      EventRow(
        planet=event.planet,
        year=event.year,
        kind=event.kind,
        jde=planet.jde,
        elongation_deg=derived_separation,
        phase_angle_deg=planet.phase_angle_deg,
        illuminated_fraction=planet.illuminated_fraction,
      )
    )
  return tuple(annotated)


def emit_event_rows(rows: tuple[EventRow, ...]) -> None:
  print("\n// Complete JPL Horizons source-only event census; 3-hour mesh certified by 6-hour and 1-day meshes")
  for row in rows:
    print(
      f"  {{ Planet::{row.planet:<7}, {row.year}, Kind::{row.kind:<30}, {row.jde:.6f} }},"
      f"  // elongation {row.elongation_deg:.7f}, phase {row.phase_angle_deg:.4f}, "
      f"illuminated {row.illuminated_fraction:.7f}"
    )


def emit_geometry_rows() -> None:
  sun_rows = fetch_horizons(SUN, EPOCHS, "20,31")
  for target in TARGETS:
    time.sleep(0.15)
    planet_rows = fetch_horizons(target, EPOCHS, "10,20,23,31,43")
    print(f"\n// {target.horizons_name}")
    for planet, sun in zip(planet_rows, sun_rows, strict=True):
      derived_separation = angular_separation_deg(sun, planet)
      if abs(derived_separation - planet.elongation_deg) > 0.0002:
        raise RuntimeError(
          f"quantity 23 differs from quantity 31 separation for {target.horizons_name} at {planet.jde}"
        )
      print(
        f"  {{ Planet::{target.enum_name:<7}, {planet.jde:13.6f}, {derived_separation:12.7f}, "
        f"{planet.phase_angle_deg:10.4f}, {planet.illuminated_fraction:11.8f} }},"
      )


def crawl_events() -> None:
  # UTC year boundaries rendered in TT. Modern TT-UTC is 69.184 seconds throughout this year.
  year_bounds = {
    2025: (2460676.500800741, 2461041.500800741),
  }
  all_events = []
  for year, (start_jde, end_jde) in year_bounds.items():
    meshes = {}
    for step_minutes in (1440, 360, 180):
      scan_start = start_jde - 1.0
      scan_end = end_jde + 1.0
      sun_rows = fetch_horizons_range(SUN, scan_start, scan_end, step_minutes, "31")
      time.sleep(0.15)
      meshes[step_minutes] = (sun_rows, {})
      for target in TARGETS:
        planet_rows = fetch_horizons_range(target, scan_start, scan_end, step_minutes, "10,23,31,43")
        meshes[step_minutes][1][target.enum_name] = planet_rows
        time.sleep(0.15)

    for target in TARGETS:
      derived = {
        step_minutes: derive_events(
          target,
          year,
          start_jde,
          end_jde,
          planet_rows[target.enum_name],
          sun_rows,
        )
        for step_minutes, (sun_rows, planet_rows) in meshes.items()
      }
      require_same_census(target, year, derived[1440], derived[180], "1-day")
      require_same_census(target, year, derived[360], derived[180], "6-hour")
      max_spread = max(
        (abs(coarse.jde - fine.jde) for coarse, fine in zip(derived[360], derived[180], strict=True)),
        default=0.0,
      )
      print(
        f"{target.horizons_name} {year}: {len(derived[180])} events, 6h/3h max epoch spread {max_spread:.8f} d",
        file=sys.stderr,
      )
      all_events.extend(annotate_events(target, derived[180]))
      time.sleep(0.15)
  emit_event_rows(tuple(all_events))


def crawl_positions() -> None:
  all_rows = []
  conjunction_rows = []
  retrograde_rows = []
  wrap_rows = []
  for target in TARGETS:
    base_rows = fetch_horizons(target, EPOCHS)
    time.sleep(0.15)
    scan_rows = fetch_horizons_batched(target, RETROGRADE_SCAN_EPOCHS)
    near_sun_rows = scan_rows[365:730]
    directed = min(near_sun_rows, key=lambda row: (row.elongation_deg, row.jde))
    all_rows.append((target, base_rows))
    conjunction_rows.append((target, (directed,)))
    retrograde_rows.append((target, select_retrograde_rows(scan_rows)))
    wrap_rows.extend((target, row, expected) for row, expected in select_wrap_rows(scan_rows))
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

  print("\n// Horizons daily centered-longitude signs, 2025-2027")
  for target, rows in retrograde_rows:
    emit_retrograde(target, rows)

  print("\n// First direct and retrograde longitude-wrap rows selected from Horizons output")
  for expected in (False, True):
    target, row, _expected = next(item for item in wrap_rows if item[2] == expected)
    emit_retrograde(target, ((row, expected, "longitude-wrap"),))

  print("\n// PyMeeus 0.5.12 geocentric_position, apparent RA/Dec")
  for target in TARGETS:
    emit_pymeeus(target)


def main() -> None:
  parser = argparse.ArgumentParser(description="Generate planetary JPL Horizons golden datasets")
  parser.add_argument("mode", choices=("positions", "geometry", "events", "all"), nargs="?", default="all")
  args = parser.parse_args()
  if args.mode in {"positions", "all"}:
    crawl_positions()
  if args.mode in {"geometry", "all"}:
    print("\n// JPL Horizons quantities 23, 31, 43, and 10: separation, phase angle, illuminated fraction")
    emit_geometry_rows()
  if args.mode in {"events", "all"}:
    crawl_events()


if __name__ == "__main__":
  main()
