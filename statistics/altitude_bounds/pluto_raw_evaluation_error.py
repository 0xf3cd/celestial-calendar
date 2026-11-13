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

"""Chapter 37 Pluto J2000 arithmetic before precession, inverse-FK5 and light-time solves."""

from fractions import Fraction as F

from .apparent_correction_error import normalized_degrees
from .floating_operation_bounds import FloatingBound as B
from .poisson_majorants import array_rows, pi_interval
from .raw_evaluation_error import rounded


RAW_START_JDE_TT = F(4819085, 2)
RAW_END_JDE_TT = F(4976139, 2)


def pluto_raw_evaluation_error(left: F, right: F, source: str) -> dict:
  """Bound a closed TT interval inside the source's half-open raw window."""
  if not RAW_START_JDE_TT <= left <= right < RAW_END_JDE_TT:
    raise ValueError("The complete Pluto raw source envelope must lie in its existing half-open domain")
  time_magnitude = max(abs(left - 2451545), abs(right - 2451545))
  centuries = B(*rounded(time_magnitude, F(0))) / 36525
  arguments = tuple(
    (B.literal(origin) + B.literal(rate) * centuries).degrees_to_radians()
    for origin, rate in ((34.35, 3034.9057), (50.08, 1222.1138), (238.96, 144.96))
  )
  rows = array_rows(source, "TERMS", "Term", 43, 9)
  totals = [B(F(0)), B(F(0)), B(F(0))]
  for row in rows:
    phase = B(abs(row[0])) * arguments[0] + B(abs(row[1])) * arguments[1] + B(abs(row[2])) * arguments[2]
    wave = phase.sine_or_cosine()
    for index in range(3):
      totals[index] = totals[index] + (B(abs(row[3 + 2 * index])) * wave + B(abs(row[4 + 2 * index])) * wave)
  longitude = normalized_degrees(B.literal(238.958116) + B.literal(144.96) * centuries + totals[0])
  latitude = B.literal(-3.908239) + totals[1]
  radius = B.literal(40.7241346) + totals[2]
  radius_lower = F(40.7241346) - totals[2].magnitude - totals[2].error
  if radius_lower <= 0:
    raise ValueError("The complete retained Pluto raw source has no positive radius lower")
  latitude_upper = (latitude.magnitude + latitude.error) * pi_interval()[1] / 180
  return {
    "centuries": centuries,
    "arguments_radians": arguments,
    "longitude_sum_degrees": totals[0],
    "latitude_sum_degrees": totals[1],
    "radius_sum_au": totals[2],
    "longitude_degrees": longitude,
    "latitude_degrees": latitude,
    "radius_au": radius,
    "reference_radius_lower_au": radius_lower,
    "reference_latitude_cosine_lower": max(F(0), 1 - latitude_upper**2 / 2),
  }
