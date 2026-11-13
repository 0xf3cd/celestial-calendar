#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Whole-region provider, fixed-clock and local observer-geometry error composition."""

from fractions import Fraction as F

from .floating_operation_bounds import FloatingBound as B
from .observer_evaluation_error import local_geometry_error
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE
from .sidereal_evaluation_error import sidereal_error


def uniform_observer_error(
  apparent: dict, equatorial: dict, clock: dict, site: tuple, earth_source: str, constants: tuple
) -> dict:
  """Compose provider and clock envelopes covering the same TT interval and stored WGS84 site."""
  interval = tuple(map(F, apparent["analysis_interval_exact"]))
  if (
    apparent["planet"] != equatorial["planet"]
    or interval != tuple(map(F, equatorial["analysis_interval_exact"]))
    or interval != clock["tt_interval"]
  ):
    raise ValueError("Provider direction/distance and clock certificates must cover the identical full TT envelope")
  latitude, east_longitude, elevation = site
  if any(F(float(value)) != value for value in site) or abs(latitude) > 90 or abs(east_longitude) > 180:
    raise ValueError("Require an exact stored site with latitude/longitude in the existing observer domain")
  sidereal = sidereal_error(
    *interval, *clock["reference_ut1_interval"], clock["ut1_jd_error_days"], -east_longitude, earth_source
  )
  physical = apparent["proof"]
  distance_magnitude = (1 + LIBM_ALLOWANCE) * F(
    physical["uniform_map"]["computed_vector_l1_upper_au"]
  ) + SUBNORMAL_ALLOWANCE
  distance_error = F(physical["fixed_point"]["fixed_point_distance_error_upper_au"])
  direction_error = F(equatorial["proof"]["stored_angle_direction_error"])
  geometry = local_geometry_error(
    B(abs(latitude)),
    B(abs(elevation)),
    direction_error,
    sidereal["local"].error,
    B(distance_magnitude, distance_error),
    F(physical["geometry"]["radial_lower_au"]),
    *constants,
  )
  return {
    "stored_unit_direction_error": direction_error,
    "distance_au": B(distance_magnitude, distance_error),
    "sidereal_degree_error": sidereal["local"].error,
    "geometry": geometry,
  }
