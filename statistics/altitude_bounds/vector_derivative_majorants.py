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

"""Norm and reciprocal derivative bounds for a separated smooth trajectory."""

from fractions import Fraction as F
from math import comb


def norm_bounds(vector: tuple, separation: F) -> tuple:
  """Bound derivatives of |x|, given uniform Euclidean bounds for x and a positive lower norm."""
  if not vector or separation <= 0 or vector[0] < separation or any(value < 0 for value in vector):
    raise ValueError("Require nonnegative vector bounds and 0 < separation <= vector magnitude bound")
  result = [vector[0]]
  if len(vector) > 1:
    result.append(vector[1])
  if len(vector) > 2:
    result.append(vector[2] + vector[1] ** 2 / separation)
  for k in range(3, len(vector)):
    cross = sum(comb(k, j) * (vector[j] * vector[k - j] + result[j] * result[k - j]) for j in range(1, k))
    result.append(vector[k] + cross / (2 * separation))
  return tuple(result)


def reciprocal_bounds(positive: tuple, lower: F) -> tuple:
  """Bound derivatives of 1/r using r>=lower and derivative bounds for r."""
  if not positive or lower <= 0 or positive[0] < lower or any(value < 0 for value in positive):
    raise ValueError("Require a separated positive scalar and nonnegative derivative bounds")
  result = [1 / lower]
  for k in range(1, len(positive)):
    result.append(sum(comb(k, j) * positive[j] * result[k - j] for j in range(1, k + 1)) / lower)
  return tuple(result)
