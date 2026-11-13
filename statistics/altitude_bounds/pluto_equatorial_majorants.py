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

"""Pluto apparent equatorial direction bounds after its two implicit source solves."""

from fractions import Fraction as F

from .apparent_correction_majorants import apparent_equatorial_bounds
from .nutation_majorants import nutation_bounds, nutation_rows
from .spherical_coordinate_majorants import spherical_bounds


def pluto_equatorial_bounds(
  smooth: dict, physical: dict, earth_record: dict, century_radius: F, earth_source: str
) -> dict:
  """Compose angular geometry, annual/FK5/IAU1980 corrections and true obliquity."""
  vector = smooth["geocentric_cartesian_norm"]
  geometry = physical["geometry"]
  angular = spherical_bounds(vector, geometry["radial_lower_au"], geometry["axial_lower_au"])
  earth_longitude = tuple(F(value["exact"]) for value in earth_record["coordinate_derivatives"]["L"])
  if len(vector) != len(earth_longitude):
    raise ValueError("The complete Pluto and Earth derivative orders must match")
  centuries = (century_radius, F(1, 36525)) + (F(0),) * (len(vector) - 2)
  iau = nutation_bounds(nutation_rows(earth_source), century_radius, len(vector) - 1)
  apparent = apparent_equatorial_bounds(
    angular["longitude_rad"],
    angular["latitude_rad"],
    earth_longitude,
    centuries,
    physical["geocentric_cosine_lower"],
    iau["longitude_rad"],
    iau["true_obliquity_rad"],
  )
  return {"geometric_angles": angular, "apparent": apparent, "nutation": iau}
