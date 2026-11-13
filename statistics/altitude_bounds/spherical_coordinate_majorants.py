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

"""Smooth angular derivatives from a Cartesian curve with certified radial and axial separation."""

from fractions import Fraction as F
from math import ceil, comb

from .poisson_majorants import pi_interval
from .vector_derivative_majorants import norm_bounds


def dyadic_upper(values: tuple) -> tuple:
  """Round nonnegative rational bounds upward to a fixed dyadic grid between compositions."""
  if any(value < 0 for value in values):
    raise ValueError("Magnitude bounds must be nonnegative")
  return tuple(F(ceil(value * 2**192), 2**192) for value in values)


def argument_bounds(vector: tuple, lower: F) -> tuple:
  """Bound a continuous local arg(z), using |z|>=lower and complex derivative norms."""
  if not vector or lower <= 0 or vector[0] < lower or any(value < 0 for value in vector):
    raise ValueError("A complex argument needs certified positive separation and nonnegative bounds")
  inverse = [1 / lower]
  for k in range(1, len(vector) - 1):
    inverse.append(sum(comb(k, j) * vector[j] * inverse[k - j] for j in range(1, k + 1)) / lower)
  result = [2 * pi_interval()[1]]
  for k in range(1, len(vector)):
    result.append(sum(comb(k - 1, j) * vector[j + 1] * inverse[k - j - 1] for j in range(k)))
  return dyadic_upper(tuple(result))


def spherical_bounds(vector: tuple, radial_lower: F, axial_lower: F) -> dict:
  """Bound atan2(y,x), atan2(z,hypot(x,y)) and cos(beta)>=axial_lower/vector[0]."""
  if not vector or radial_lower <= 0 or axial_lower <= 0 or max(radial_lower, axial_lower) > vector[0]:
    raise ValueError("Radial and cylindrical separations must be proved positive")
  longitude = argument_bounds(vector, axial_lower)
  cylindrical = norm_bounds(vector, axial_lower)
  meridional = (vector[0],) + tuple(a + b for a, b in zip(vector[1:], cylindrical[1:], strict=True))
  latitude = argument_bounds(meridional, radial_lower)
  latitude = (dyadic_upper((pi_interval()[1] / 2,))[0],) + latitude[1:]
  return {"longitude_rad": longitude, "latitude_rad": latitude, "cos_latitude_lower": axial_lower / vector[0]}
