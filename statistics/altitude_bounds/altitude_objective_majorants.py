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

"""Cartesian altitude-objective bounds for a positively separated local trajectory."""

from fractions import Fraction as F


def objective_bounds(vector: tuple, separation: F, sine_magnitude: F) -> tuple:
  """Bound the first three TT derivatives of up-s*q, with constant s and q=|v|>=separation."""
  if len(vector) != 4 or separation <= 0 or not 0 <= sine_magnitude <= 1 or any(v < 0 for v in vector):
    raise ValueError("Require four nonnegative vector bounds, positive separation and |s|<=1")
  if vector[0] < separation:
    raise ValueError("The vector bound cannot be smaller than its lower norm")
  _, first, second, third = vector
  s = sine_magnitude
  return (
    (1 + s) * first,
    (1 + s) * second + s * first**2 / separation,
    (1 + s) * third + 3 * s * first * second / separation + 3 * s * first**3 / separation**2,
  )
