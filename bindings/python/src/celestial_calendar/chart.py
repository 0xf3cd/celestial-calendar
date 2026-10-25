# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Owned chart values and decoding of the fixed native snapshot."""

from dataclasses import dataclass as _dataclass
from enum import StrEnum as _StrEnum

from . import _abi
from ._codes import constant, enum_code
from ._models import ModelKey, ModelKind, Target, is_node
from ._protocol import finite, longitude


class CivilScale(_StrEnum):
  """The explicitly selected time scale of a civil input."""

  UTC = "utc"
  UT1 = "ut1"


class HouseSystem(_StrEnum):
  """House geometry; all systems reject an Ascendant direction unresolved at round-off.

  Placidus also rejects its date-dependent polar domain.
  """

  EQUAL = "equal"
  WHOLE_SIGN = "whole_sign"
  PLACIDUS = "placidus"


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
  """Apparent geocentric ecliptic longitude/latitude of date, in degrees.

  The instantaneous longitude rate is degrees per TT day; a negative rate means decreasing
  longitude. Distances are AU for every body, including the Moon; nodes have known-zero
  latitude and no distance. source identifies the position provider.
  """

  target: Target
  longitude_deg: float
  latitude_deg: float
  distance_au: float | None
  longitude_rate_deg_per_tt_day: float
  source: ModelKey


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
  """An owned same-instant snapshot, including resolved Delta T and house-nutation keys.

  Bodies follow Target order and retain their fixed IAU1980 position convention.
  """

  times: ChartTimes
  bodies: tuple[BodyState, ...]
  houses: ChartHouses
  delta_t_model: ModelKey
  nutation_model: ModelKey


def _body_from_native(body: _abi.CelestialChartBody, expected: Target) -> BodyState:
  if enum_code("TARGET", expected) != body.target_code:
    raise ValueError(f"unexpected target code for {expected.name}")

  node = is_node(expected)
  expected_fields = constant("PRESENT_LATITUDE") | (0 if node else constant("PRESENT_DISTANCE"))
  if body.present_fields != expected_fields:
    raise ValueError(f"unexpected presence bits for {expected.name}")

  latitude = finite(body.latitude_deg, f"{expected.name}.latitude_deg")
  if not -90.0 <= latitude <= 90.0 or (node and latitude != 0.0):
    raise ValueError(f"unexpected latitude for {expected.name}")
  # Absent distance storage is deliberately not read: it has no value semantics.
  distance = None if node else finite(body.distance_au, f"{expected.name}.distance_au")
  if distance is not None and distance <= 0.0:
    raise ValueError(f"nonpositive distance for {expected.name}")

  return BodyState(
    expected,
    longitude(body.longitude_deg, f"{expected.name}.longitude_deg"),
    latitude,
    distance,
    finite(body.longitude_rate_deg_per_tt_day, f"{expected.name}.longitude_rate_deg_per_tt_day"),
    ModelKey(ModelKind.POSITION_SOURCE, body.source_code),
  )


def decode_houses(native: _abi.CelestialChartHouses) -> ChartHouses:
  return ChartHouses(
    longitude(native.ascendant_deg, "ascendant_deg"),
    longitude(native.midheaven_deg, "midheaven_deg"),
    longitude(native.descendant_deg, "descendant_deg"),
    longitude(native.imum_coeli_deg, "imum_coeli_deg"),
    tuple(longitude(cusp, "cusps_deg") for cusp in native.cusps_deg),
  )


def decode_snapshot(result: _abi.CelestialChartSnapshot) -> ChartSnapshot:
  """Copy a successful payload; the caller translates decoding failures."""
  if not isinstance(result, _abi.CelestialChartSnapshot):
    raise TypeError("unexpected snapshot record")
  times = ChartTimes(finite(result.jd_ut1, "jd_ut1"), finite(result.jde_tt, "jde_tt"))
  bodies = tuple(_body_from_native(body, target) for body, target in zip(result.bodies, Target, strict=True))
  return ChartSnapshot(
    times,
    bodies,
    decode_houses(result.houses),
    ModelKey(ModelKind.DELTA_T, result.delta_t_identity_code),
    ModelKey(ModelKind.NUTATION, result.nutation_identity_code),
  )
