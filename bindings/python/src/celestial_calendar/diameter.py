# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Owned scalar disk sizes, with adopted radius and actual provider/observer distance."""

from dataclasses import dataclass
from enum import StrEnum

from . import _abi
from ._codes import enum_code, enum_member
from ._models import ModelKey, ModelKind, Target
from ._protocol import finite
from .chart import ChartTimes

# NIST SP 330 (2019), Table 8; ERFA v2.0.1 erfam.h, ERFA_DAU, converted to km.
# Retained material boundary (R34): this exact NIST/ERFA AU definition remains under its source terms
# and outside the project MIT grant; the validation wrapper is project-authored.
AU_KM = 149597870.7


class DiameterCentre(StrEnum):
  """Origin of the distance used for the disk."""

  GEOCENTRIC = "geocentric"
  TOPOCENTRIC = "topocentric"


class RadiusDefinition(StrEnum):
  """Adopted radius source and convention, separate from the ephemeris source."""

  IAU_2015_SOLAR_NOMINAL = "iau_2015_solar_nominal"
  JPL_SSD_SATELLITE_MEAN = "jpl_ssd_satellite_mean"
  JPL_SSD_PLANET_EQUATORIAL = "jpl_ssd_planet_equatorial"


@dataclass(frozen=True)
class ApparentDiameter:
  """Full spherical disk in arcseconds; planets use equatorial radii and Saturn excludes rings."""

  times: ChartTimes
  target: Target
  centre: DiameterCentre
  source: ModelKey
  delta_t_model: ModelKey
  radius_definition: RadiusDefinition
  radius_km: float
  distance_au: float
  diameter_arcsec: float


def decode_diameter(native: _abi.CelestialApparentDiameter, target: Target, centre: DiameterCentre) -> ApparentDiameter:
  """Check request identities and all required physical fields before taking an owned copy."""
  if not isinstance(native, _abi.CelestialApparentDiameter) or not native.valid:
    raise ValueError("unexpected apparent-diameter record")
  if native.target_code != enum_code("TARGET", target) or native.centre_code != enum_code("DIAMETER_CENTRE", centre):
    raise ValueError("unexpected apparent-diameter target or centre")

  radius = finite(native.radius_km, "radius_km")
  distance = finite(native.distance_au, "distance_au")
  angle = finite(native.diameter_arcsec, "diameter_arcsec")
  if radius <= 0.0 or distance <= radius / AU_KM or not 0.0 < angle < 648000.0:
    raise ValueError("invalid apparent-diameter radius, distance or angle")

  return ApparentDiameter(
    times=ChartTimes(finite(native.jd_ut1, "jd_ut1"), finite(native.jde_tt, "jde_tt")),
    target=target,
    centre=centre,
    source=ModelKey(ModelKind.POSITION_SOURCE, native.source_code),
    delta_t_model=ModelKey(ModelKind.DELTA_T, native.delta_t_identity_code),
    radius_definition=enum_member(RadiusDefinition, "RADIUS_DEFINITION", native.radius_definition_code),
    radius_km=radius,
    distance_au=distance,
    diameter_arcsec=angle,
  )
