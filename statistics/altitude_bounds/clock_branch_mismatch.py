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

"""Exact-source jump and derivative allowances across adjacent fixed ALGO5 branches."""

from fractions import Fraction as F

from .clock_branch_majorants import branch_records, join_jumps, translated
from .poisson_majorants import polynomial_bounds, stored


def extended_branch_slope(index: int, left: F, right: F) -> F:
  """Bound the selected source formula's year derivative throughout a complete seam collar, including its extension."""
  if index not in range(13) or left > right:
    raise ValueError("Require a retained source branch and ordered collar")
  row = branch_records()[index]
  lo, hi = (left - row["origin"]) / row["scale"], (right - row["origin"]) / row["scale"]
  centre, radius = (lo + hi) / 2, (hi - lo) / 2
  slope = polynomial_bounds(translated(row["polynomial"], centre), radius, row["scale"], 1)[1]
  if index == 11:
    if lo <= 0:
      raise ValueError("The complete fitted-branch extension is not separated from its rational pole")
    slope += stored("34469.54162338588") / (row["scale"] * lo**2)
  if index == 12:
    slope += stored("284.8435805251424") * stored("0.4487989505128276") / row["scale"]
  return slope


def seam_collar_allowance(join_index: int, year_error: F) -> dict:
  """Bound the exact two-formula difference within year_error of their retained join, before evaluation error."""
  if join_index not in range(12) or year_error < 0:
    raise ValueError("Require a retained join and nonnegative decimal-year allowance")
  jump = join_jumps(branch_records())[join_index]
  boundary = jump["year"]
  left, right = boundary - year_error, boundary + year_error
  slopes = tuple(extended_branch_slope(index, left, right) for index in (join_index, join_index + 1))
  jump_upper = max(abs(jump["jump_lower_seconds"]), abs(jump["jump_upper_seconds"]))
  return {
    "boundary_year": boundary,
    "jump_absolute_upper_seconds": jump_upper,
    "collar_years": year_error,
    "extended_slopes_seconds_per_year": slopes,
    "mismatch_upper_seconds": jump_upper + sum(slopes) * year_error,
  }
