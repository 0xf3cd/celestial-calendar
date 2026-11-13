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

"""GMST, fixed IAU1980 equinoxes and west-positive LAST operation bounds in circular degrees."""

from fractions import Fraction as F

from .floating_operation_bounds import FloatingBound as B
from .nutation_evaluation_error import nutation_error_from_centuries
from .raw_evaluation_error import SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF, rounded


def normalized_degrees(value: B) -> B:
  """Use exact IEEE remainder modulo360 and charge the negative wrap in the source's circular result metric."""
  return B(F(360), value.error + UNIT_ROUNDOFF * 360 + SUBNORMAL_ALLOWANCE)


def sidereal_error(
  tt_left: F,
  tt_right: F,
  reference_ut1_left: F,
  reference_ut1_right: F,
  ut1_error_days: F,
  longitude_west: F,
  earth_source: str,
) -> dict:
  """Bound circular stored LAST degrees over a complete TT/UT1 box, including all three normalizations."""
  if tt_left > tt_right or reference_ut1_left > reference_ut1_right or ut1_error_days < 0:
    raise ValueError("Require ordered TT/UT1 intervals and a nonnegative UT1 value allowance")
  if abs(longitude_west) > 180 or F(float(longitude_west)) != longitude_west:
    raise ValueError("The observer longitude must be an exact stored west-positive degree value in[-180,180]")
  d_magnitude = max(abs(reference_ut1_left - 2451545), abs(reference_ut1_right - 2451545)) + ut1_error_days
  d = B(*rounded(d_magnitude, ut1_error_days))
  t = d / 36525
  mean_unwrapped = (
    B.literal(280.46061837)
    + (B.literal(360.98564736629) * d)
    + (B.literal(0.000387933) * t * t)
    - ((t * t * t) / 38710000.0)
  )
  mean = normalized_degrees(mean_unwrapped)
  tt_magnitude = max(abs(tt_left - 2451545), abs(tt_right - 2451545))
  centuries = B(*rounded(tt_magnitude, F(0))) / 36525
  nutation = nutation_error_from_centuries(centuries, earth_source)
  correction = nutation["equation_of_equinoxes_degrees"]
  apparent = normalized_degrees(mean + correction)
  local = normalized_degrees(apparent - B(abs(longitude_west)))
  return {
    "mean_unwrapped": mean_unwrapped,
    "mean": mean,
    "equation_of_equinoxes": correction,
    "apparent": apparent,
    "local": local,
    "local_retrieved_radians": local.degrees_to_radians(),
  }
