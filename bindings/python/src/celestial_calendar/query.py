# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Owned ecliptic, equatorial and civil-house query results."""

from __future__ import annotations

from dataclasses import dataclass as _dataclass

from . import _abi
from ._codes import constant, enum_code
from ._models import ModelKey, ModelKind, Target, is_node
from ._protocol import finite, longitude
from .chart import ChartHouses, ChartTimes, HouseSystem, decode_houses


@_dataclass(frozen=True)
class Position:
  """Apparent geocentric ecliptic longitude/latitude of date, in degrees.

  Physical distances are AU; mathematical nodes have no distance. source identifies
  the position provider, including for a single-target selection.
  """

  target: Target
  longitude_deg: float
  latitude_deg: float
  distance_au: float | None
  source: ModelKey


@_dataclass(frozen=True)
class EquatorialPosition:
  """Apparent geocentric RA [0, 360) and declination [-90, 90], in degrees of date.

  The projection uses fixed IAU1980 longitude nutation and true obliquity. Distances
  are AU where physically defined; mathematical nodes have no distance.
  """

  target: Target
  right_ascension_deg: float
  declination_deg: float
  distance_au: float | None
  source: ModelKey


@_dataclass(frozen=True)
class HouseQueryResult:
  """House geometry and both resolved model keys at one civil instant.

  times contains JD(UT1) and JDE(TT); houses contains axes/cusps in degrees.
  """

  times: ChartTimes
  houses: ChartHouses
  house_system: HouseSystem
  delta_t_model: ModelKey
  nutation_model: ModelKey


def _position_fields(record: object, expected: Target) -> tuple[float | None, ModelKey]:
  if record.target_code != enum_code("TARGET", expected):
    raise ValueError(f"unexpected target code for {expected.name}")
  node = is_node(expected)
  expected_fields = constant("PRESENT_LATITUDE") | (0 if node else constant("PRESENT_DISTANCE"))
  if record.present_fields != expected_fields:
    raise ValueError(f"unexpected presence bits for {expected.name}")
  # Absent storage has no value semantics and is deliberately not read.
  distance = None if node else finite(record.distance_au, "distance_au")
  if distance is not None and distance <= 0.0:
    raise ValueError(f"nonpositive distance for {expected.name}")
  return distance, ModelKey(ModelKind.POSITION_SOURCE, record.source_code)


def decode_positions(records, expected_targets: tuple[Target, ...]) -> tuple[Position, ...]:
  if len(records) != len(expected_targets):
    raise ValueError("unexpected position count")
  positions = []
  for record, target in zip(records, expected_targets, strict=True):
    if not isinstance(record, _abi.CelestialPositionRecord):
      raise TypeError("unexpected position record")
    distance, source = _position_fields(record, target)
    latitude = finite(record.latitude_deg, "latitude_deg")
    if not -90.0 <= latitude <= 90.0 or (is_node(target) and latitude != 0.0):
      raise ValueError(f"unexpected latitude for {target.name}")
    positions.append(Position(target, longitude(record.longitude_deg, "longitude_deg"), latitude, distance, source))
  return tuple(positions)


def decode_equatorial_positions(records, expected_targets: tuple[Target, ...]) -> tuple[EquatorialPosition, ...]:
  if len(records) != len(expected_targets):
    raise ValueError("unexpected equatorial position count")
  positions = []
  for record, target in zip(records, expected_targets, strict=True):
    if not isinstance(record, _abi.CelestialEquatorialPositionRecord):
      raise TypeError("unexpected equatorial position record")
    distance, source = _position_fields(record, target)
    declination = finite(record.declination_deg, "declination_deg")
    if not -90.0 <= declination <= 90.0:
      raise ValueError(f"unexpected declination for {target.name}")
    positions.append(
      EquatorialPosition(
        target, longitude(record.right_ascension_deg, "right_ascension_deg"), declination, distance, source
      )
    )
  return tuple(positions)


def decode_house_query(result: _abi.CelestialHouseQueryResult, system: HouseSystem) -> HouseQueryResult:
  if not isinstance(result, _abi.CelestialHouseQueryResult):
    raise TypeError("unexpected house query record")
  if result.house_system_code != enum_code("HOUSE_SYSTEM", system):
    raise ValueError("unexpected house system")
  return HouseQueryResult(
    ChartTimes(finite(result.jd_ut1, "jd_ut1"), finite(result.jde_tt, "jde_tt")),
    decode_houses(result.houses),
    system,
    ModelKey(ModelKind.DELTA_T, result.delta_t_identity_code),
    ModelKey(ModelKind.NUTATION, result.nutation_identity_code),
  )
