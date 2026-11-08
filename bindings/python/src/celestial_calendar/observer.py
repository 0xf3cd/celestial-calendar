# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""WGS84 observer values and owned topocentric query results."""

from dataclasses import dataclass
from enum import StrEnum

from . import _abi
from ._codes import constant, enum_code
from ._models import ModelKey, ModelKind, Target
from ._protocol import finite, longitude
from .chart import ChartTimes


class RefractionModel(StrEnum):
  """Bennett apparent-altitude inverse or Saemundsson geometric-altitude formula."""

  BENNETT = "bennett"
  SAEMUNDSSON = "saemundsson"


@dataclass(frozen=True)
class Observer:
  """WGS84 latitude [-90, 90]°, east longitude [-180, 180]°, and signed ellipsoidal height in metres."""

  latitude_deg: float
  longitude_deg: float
  elevation_m: float


@dataclass(frozen=True)
class RefractionParams:
  """Explicit refraction model; supported weather is [-90, 60]°C and [300, 1100] hPa."""

  model: RefractionModel
  temperature_c: float = 15.0
  pressure_hpa: float = 1013.25


@dataclass(frozen=True)
class TopocentricPosition:
  """Centre direction in degrees and observer distance in AU, using fixed IAU1980 axes.

  Azimuth is north-zero/east-positive, absent at exact geographic poles or an unresolved
  horizontal projection. RA is absent at an unresolved equatorial projection. Geometric
  altitude remains available; apparent_altitude_deg is separate and optional.
  """

  times: ChartTimes
  target: Target
  source: ModelKey
  delta_t_model: ModelKey
  right_ascension_deg: float | None
  declination_deg: float
  distance_au: float
  altitude_deg: float
  azimuth_deg: float | None
  apparent_altitude_deg: float | None


def decode_position(
  native: _abi.CelestialTopocentricPosition, target: Target, refraction_requested: bool, geographic_pole: bool
) -> TopocentricPosition:
  """Validate the native presence mask before reading optional storage."""
  if not isinstance(native, _abi.CelestialTopocentricPosition):
    raise TypeError("unexpected topocentric record")
  if native.target_code != enum_code("TARGET", target):
    raise ValueError("unexpected topocentric target")
  ra_bit, az_bit, apparent_bit = (
    constant("PRESENT_RIGHT_ASCENSION"),
    constant("PRESENT_AZIMUTH"),
    constant("PRESENT_APPARENT_ALTITUDE"),
  )
  mask = native.present_fields
  if mask & ~(ra_bit | az_bit | apparent_bit):
    raise ValueError("unknown topocentric presence bits")
  if (not refraction_requested and mask & apparent_bit) or (geographic_pole and mask & az_bit):
    raise ValueError("unexpected topocentric optional field")

  declination = finite(native.declination_deg, "declination_deg")
  altitude = finite(native.altitude_deg, "altitude_deg")
  distance = finite(native.distance_au, "distance_au")
  if not -90.0 <= declination <= 90.0 or not -90.0 <= altitude <= 90.0 or distance <= 0.0:
    raise ValueError("invalid topocentric declination, altitude or distance")
  apparent = finite(native.apparent_altitude_deg, "apparent_altitude_deg") if mask & apparent_bit else None
  if apparent is not None and not -1e-9 <= apparent <= 90.0:
    raise ValueError("invalid apparent altitude")
  return TopocentricPosition(
    times=ChartTimes(finite(native.jd_ut1, "jd_ut1"), finite(native.jde_tt, "jde_tt")),
    target=target,
    source=ModelKey(ModelKind.POSITION_SOURCE, native.source_code),
    delta_t_model=ModelKey(ModelKind.DELTA_T, native.delta_t_identity_code),
    right_ascension_deg=longitude(native.right_ascension_deg, "right_ascension_deg") if mask & ra_bit else None,
    declination_deg=declination,
    distance_au=distance,
    altitude_deg=altitude,
    azimuth_deg=longitude(native.azimuth_deg, "azimuth_deg") if mask & az_bit else None,
    apparent_altitude_deg=apparent,
  )
