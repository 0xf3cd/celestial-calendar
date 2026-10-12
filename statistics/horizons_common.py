# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import math

from collections.abc import Iterator, Sequence
from decimal import Decimal
from typing import TypeVar

import requests


HORIZONS_URL = "https://ssd.jpl.nasa.gov/api/horizons.api"
# Conservative GET URL budget, not the API's 10,000-token TLIST limit.
TLIST_BATCH_SIZE = 40
# Nine-decimal JD echoes can differ by a few binary64 ulps.
EPOCH_ECHO_TOLERANCE_DAYS = 1e-8
SEPARATION_TOLERANCE_DEG = 0.0002
ILLUMINATION_TOLERANCE = 0.000002
T = TypeVar("T")


def get_text(params: dict[str, str], *, timeout: int, url: str = HORIZONS_URL) -> str:
  """One HTTP attempt; retry and pacing belong to the caller."""
  response = requests.get(url, params=params, timeout=timeout)
  response.raise_for_status()
  return response.text


def tlist(tokens: Sequence[str]) -> str:
  # https://ssd-api.jpl.nasa.gov/doc/horizons.html#tlist: quote each token, not the whole list.
  return " ".join(f"'{token}'" for token in tokens)


def batches(values: Sequence[T], size: int = TLIST_BATCH_SIZE) -> Iterator[Sequence[T]]:
  for offset in range(0, len(values), size):
    yield values[offset : offset + size]


def range_epochs(start_token: str, stop_token: str, step_minutes: int) -> tuple[float, ...]:
  """Inclusive fixed-minute grid bounded by the serialized JD endpoints."""
  start, stop = Decimal(start_token), Decimal(stop_token)
  if not start.is_finite() or not stop.is_finite() or stop < start:
    raise RuntimeError("Horizons range endpoints must be finite and ordered")
  if type(step_minutes) is not int or step_minutes <= 0:
    raise RuntimeError("Horizons range cadence must be a positive integer number of minutes")
  steps = int((stop - start) * 1440 // step_minutes)
  return tuple(float(start + Decimal(index * step_minutes) / 1440) for index in range(steps + 1))


def parse_csv_block(text: str) -> tuple[list[str], list[list[str]]]:
  lines = text.splitlines()
  if lines.count("$$SOE") != 1 or lines.count("$$EOE") != 1:
    raise RuntimeError("Horizons response has invalid data markers")
  soe, eoe = lines.index("$$SOE"), lines.index("$$EOE")
  if eoe <= soe:
    raise RuntimeError("Horizons response has invalid data markers")
  header = next(
    (
      [cell.strip() for cell in line.split(",")]
      for line in reversed(lines[:soe])
      if "," in line and not line.startswith("**")
    ),
    None,
  )
  if header is None:
    raise RuntimeError("Horizons column header line not found")
  rows = [[cell.strip() for cell in line.split(",")] for line in lines[soe + 1 : eoe] if line.strip()]
  if any(len(row) != len(header) for row in rows):
    raise RuntimeError("Horizons row width differs from the column header")
  return header, rows


def column_indices(header: list[str], required: tuple[str, ...], time_scale: str) -> tuple[int, dict[str, int]]:
  jd_columns = [index for index, name in enumerate(header) if "JD" in name]
  if len(jd_columns) != 1 or not header[jd_columns[0]].endswith(f"JD{time_scale}"):
    raise RuntimeError(f"Horizons JD column is not {time_scale}: {header!r}")
  if any(header.count(name) != 1 for name in required):
    raise RuntimeError(f"Horizons columns differ: {header!r}; required {required!r}")
  return jd_columns[0], {name: index for index, name in enumerate(header) if name}


def finite_number(cell: str, name: str, epoch: str = "") -> float:
  try:
    value = float(cell)
  except ValueError as error:
    raise RuntimeError(f"Horizons returned {cell} for {name} at {epoch}") from error
  if not math.isfinite(value):
    raise RuntimeError(f"Horizons returned a non-finite {name} at {epoch}")
  return value


def require_epochs(
  returned: tuple[float, ...],
  expected: tuple[float, ...] | None,
  label: str,
  tolerance: float = EPOCH_ECHO_TOLERANCE_DAYS,
) -> None:
  if not returned:
    raise RuntimeError(f"Horizons returned no rows for {label}")
  if any(not math.isfinite(epoch) for epoch in returned):
    raise RuntimeError(f"Horizons returned a non-finite epoch for {label}")
  if any(right <= left for left, right in zip(returned, returned[1:], strict=False)):
    raise RuntimeError(f"Horizons epochs are not strictly increasing for {label}")
  if expected is not None and (
    len(returned) != len(expected)
    or any(
      not math.isfinite(epoch) or abs(actual - epoch) > tolerance
      for actual, epoch in zip(returned, expected, strict=False)
    )
  ):
    raise RuntimeError(f"Horizons epochs differ for {label}: expected {expected!r}, got {returned!r}")


def angular_separation_deg(source: tuple[float, float], target: tuple[float, float]) -> float:
  vectors = []
  for lon_deg, lat_deg in (source, target):
    if not all(math.isfinite(value) for value in (lon_deg, lat_deg)):
      raise RuntimeError("Horizons separation requires finite coordinates")
    lon, lat = math.radians(lon_deg), math.radians(lat_deg)
    vectors.append((math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat)))
  lhs, rhs = vectors
  dot = sum(left * right for left, right in zip(lhs, rhs, strict=True))
  cross = (
    lhs[1] * rhs[2] - lhs[2] * rhs[1],
    lhs[2] * rhs[0] - lhs[0] * rhs[2],
    lhs[0] * rhs[1] - lhs[1] * rhs[0],
  )
  return math.degrees(math.atan2(math.sqrt(sum(value * value for value in cross)), dot))


def checked_separation(
  source: tuple[float, float],
  target: tuple[float, float],
  elongation_deg: float,
  label: str,
) -> float:
  separation = angular_separation_deg(source, target)
  if not math.isfinite(elongation_deg) or abs(separation - elongation_deg) > SEPARATION_TOLERANCE_DEG:
    raise RuntimeError(f"quantity 23 differs from quantity 31 separation for {label}")
  return separation


def require_illumination(phase_angle_deg: float, illuminated_fraction: float, label: str) -> None:
  if not all(math.isfinite(value) for value in (phase_angle_deg, illuminated_fraction)):
    raise RuntimeError(f"quantity 10 differs from quantity 43 phase angle for {label}: non-finite quantity")
  expected = (1.0 + math.cos(math.radians(phase_angle_deg))) / 2.0
  if abs(expected - illuminated_fraction) > ILLUMINATION_TOLERANCE:
    raise RuntimeError(f"quantity 10 differs from quantity 43 phase angle for {label}")
