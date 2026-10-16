# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Owned chart values and pure decoding of the fixed V1 native snapshot."""

import math as _math
from dataclasses import dataclass as _dataclass
from enum import StrEnum as _StrEnum

from ._abi import ChartBodyV1 as _ChartBodyV1
from ._abi import ChartSnapshotV1 as _ChartSnapshotV1


class CivilScale(_StrEnum):
  """The explicitly selected time scale of a civil input."""

  UTC = "utc"
  UT1 = "ut1"


class ChartTarget(_StrEnum):
  """The fixed V1 body roster, including both directions of each lunar node."""

  SUN = "sun"
  MOON = "moon"
  MERCURY = "mercury"
  VENUS = "venus"
  MARS = "mars"
  JUPITER = "jupiter"
  SATURN = "saturn"
  URANUS = "uranus"
  NEPTUNE = "neptune"
  PLUTO = "pluto"
  MEAN_ASCENDING = "mean_ascending"
  MEAN_DESCENDING = "mean_descending"
  TRUE_ASCENDING = "true_ascending"
  TRUE_DESCENDING = "true_descending"


class HouseSystem(_StrEnum):
  """House geometry; all systems reject an Ascendant direction unresolved at round-off.

  Placidus also rejects its date-dependent polar domain.
  """

  EQUAL = "equal"
  WHOLE_SIGN = "whole_sign"
  PLACIDUS = "placidus"


# Native V1 codes, the CHART_* values in celestial.h.
_CIVIL_SCALE_CODE = {CivilScale.UTC: 0, CivilScale.UT1: 1}
_HOUSE_SYSTEM_CODE = {HouseSystem.EQUAL: 0, HouseSystem.WHOLE_SIGN: 1, HouseSystem.PLACIDUS: 2}
_CHART_TARGET_CODE = {
  ChartTarget.SUN: 0,
  ChartTarget.MOON: 1,
  ChartTarget.MERCURY: 2,
  ChartTarget.VENUS: 3,
  ChartTarget.MARS: 4,
  ChartTarget.JUPITER: 5,
  ChartTarget.SATURN: 6,
  ChartTarget.URANUS: 7,
  ChartTarget.NEPTUNE: 8,
  ChartTarget.PLUTO: 9,
  ChartTarget.MEAN_ASCENDING: 10,
  ChartTarget.MEAN_DESCENDING: 11,
  ChartTarget.TRUE_ASCENDING: 12,
  ChartTarget.TRUE_DESCENDING: 13,
}
_TARGET_BY_CODE = {code: target for target, code in _CHART_TARGET_CODE.items()}
_NODE_TARGETS = (
  ChartTarget.MEAN_ASCENDING,
  ChartTarget.MEAN_DESCENDING,
  ChartTarget.TRUE_ASCENDING,
  ChartTarget.TRUE_DESCENDING,
)
_PRESENT_LATITUDE = 1
_PRESENT_DISTANCE = 2


@_dataclass(frozen=True)
class GeoLocation:
  """North-positive latitude (-90, 90) and east-positive longitude [-180, 180], in degrees."""

  latitude_deg: float
  longitude_deg: float


@_dataclass(frozen=True)
class ChartTimes:
  """UT1-based Julian Day and TT-based Julian Ephemeris Day for the same instant."""

  jd_ut1: float
  jde_tt: float


@_dataclass(frozen=True)
class BodyState:
  """Apparent geocentric ecliptic state; negative longitude rate means retrograde.

  Nodes have known-zero latitude and no distance. Rates are degrees per TT day.
  """

  target: ChartTarget
  longitude_deg: float
  latitude_deg: float
  distance_au: float | None
  longitude_rate_deg_per_tt_day: float


@_dataclass(frozen=True)
class ChartHouses:
  """Four axes and twelve cusps, numbered from house 1, all in [0, 360) degrees."""

  ascendant_deg: float
  midheaven_deg: float
  descendant_deg: float
  imum_coeli_deg: float
  cusps_deg: tuple[float, ...]


@_dataclass(frozen=True)
class ChartSnapshot:
  """An owned same-instant snapshot; bodies follow ChartTarget's fixed V1 order."""

  times: ChartTimes
  bodies: tuple[BodyState, ...]
  houses: ChartHouses


def _finite_native(value: float, name: str) -> float:
  if not _math.isfinite(value):
    raise ValueError(f"{name} is not finite")
  return value


def _longitude_native(value: object, name: str) -> float:
  result = _finite_native(value, name)
  if not 0.0 <= result < 360.0:
    raise ValueError(f"{name} is outside [0, 360)")
  return result


def _body_from_native(body: _ChartBodyV1, expected: ChartTarget) -> BodyState:
  if _TARGET_BY_CODE.get(body.target_code) is not expected:
    raise ValueError(f"unexpected target code for {expected.name}")

  is_node = expected in _NODE_TARGETS
  expected_fields = _PRESENT_LATITUDE if is_node else _PRESENT_LATITUDE | _PRESENT_DISTANCE
  if body.present_fields != expected_fields:
    raise ValueError(f"unexpected presence bits for {expected.name}")

  latitude = _finite_native(body.latitude_deg, f"{expected.name}.latitude_deg")
  if not -90.0 <= latitude <= 90.0 or (is_node and latitude != 0.0):
    raise ValueError(f"unexpected latitude for {expected.name}")
  # Absent distance storage is deliberately not read: it has no value semantics.
  distance = None if is_node else _finite_native(body.distance_au, f"{expected.name}.distance_au")
  if distance is not None and distance <= 0.0:
    raise ValueError(f"nonpositive distance for {expected.name}")

  return BodyState(
    expected,
    _longitude_native(body.longitude_deg, f"{expected.name}.longitude_deg"),
    latitude,
    distance,
    _finite_native(body.longitude_rate_deg_per_tt_day, f"{expected.name}.longitude_rate_deg_per_tt_day"),
  )


def _from_native(result: _ChartSnapshotV1) -> ChartSnapshot:
  """Copy a successful V1 payload; malformed protocol fields raise decoding errors."""
  if not isinstance(result, _ChartSnapshotV1):
    raise TypeError("unexpected V1 snapshot record")

  times = ChartTimes(_finite_native(result.jd_ut1, "jd_ut1"), _finite_native(result.jde_tt, "jde_tt"))
  bodies = tuple(_body_from_native(body, target) for body, target in zip(result.bodies, ChartTarget, strict=True))

  native_houses = result.houses
  houses = ChartHouses(
    _longitude_native(native_houses.ascendant_deg, "ascendant_deg"),
    _longitude_native(native_houses.midheaven_deg, "midheaven_deg"),
    _longitude_native(native_houses.descendant_deg, "descendant_deg"),
    _longitude_native(native_houses.imum_coeli_deg, "imum_coeli_deg"),
    tuple(_longitude_native(cusp, "cusps_deg") for cusp in native_houses.cusps_deg),
  )

  return ChartSnapshot(times, bodies, houses)
