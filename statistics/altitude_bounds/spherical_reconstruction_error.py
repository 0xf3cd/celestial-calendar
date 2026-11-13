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

"""Value-error transport for the source's atan2/asin-clamp unit-vector reconstruction."""

from fractions import Fraction as F

from .rational_intervals import Interval as I


def reconstruction_error(xy_error: F, z_error: F, angular_error: F, axial_lower: F = F(0)) -> F:
  """Bound unit direction error; axial_lower is a proved lower reference xy norm, not an admission rule."""
  if min(xy_error, z_error, angular_error, axial_lower) < 0 or axial_lower > 1:
    raise ValueError("Errors must be nonnegative and the optional unit-vector xy lower bound must be in[0,1]")
  radial_error = I.enclose(2 * z_error).sqrt().hi
  if axial_lower > 0:
    radial_error = min(radial_error, 2 * z_error / axial_lower)
  return 2 * xy_error + z_error + radial_error + angular_error
