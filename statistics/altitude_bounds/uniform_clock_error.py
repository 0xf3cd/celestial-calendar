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

"""Fixed-clock value bounds over complete admitted TT intervals, including Gregorian and model seams."""

from fractions import Fraction as F

from .calendar_lattice import (
  END_JDE_TT,
  NANOSECONDS_PER_DAY,
  exact_jde_fraction,
  inverse_integer_calendar,
  midnight_jd,
  shifted_datetime_error,
  tt_datetime_error_upper_ns,
)
from .clock_branch_majorants import branch_records
from .clock_branch_mismatch import seam_collar_allowance
from .clock_model_evaluation_error import clock_model_branch_error
from .floating_operation_bounds import FloatingBound as B
from .raw_evaluation_error import rounded


def exact_decimal_year(observed: F) -> F:
  """Resolve the exact original-TT year coordinate before nanosecond conversion or model arithmetic."""
  resolved = exact_jde_fraction(observed)
  year, month, day = inverse_integer_calendar(resolved["julian_day_number"])
  past_days = midnight_jd(year, month, day) - midnight_jd(year, 1, 1)
  days = 366 if year % 4 == 0 and (year % 100 != 0 or year % 400 == 0) else 365
  return F(year) + (past_days + resolved["exact_fraction"]) / days


def uniform_clock_error(left: F, right: F, source: str) -> dict:
  """Cover every admitted stored TT value in a closed interval; this value bound does not assert seam smoothness."""
  if left > right:
    raise ValueError("The TT interval endpoints are reversed")
  year_left, year_right = exact_decimal_year(left), exact_decimal_year(right)
  maximum_year = year_right.numerator // year_right.denominator
  fraction = B(*rounded(F(1), F(0)))
  year_operations = B(F(maximum_year)) + ((fraction + B(F(365))) / 365)
  initial_quantization = tt_datetime_error_upper_ns() / NANOSECONDS_PER_DAY
  year_error = year_operations.error + initial_quantization / 365
  records = branch_records()
  if 2 * year_error >= min(row["end_year"] - row["start_year"] for row in records):
    raise ValueError("The input-year error could cross multiple model joins")
  bounds = []
  for index, row in enumerate(records):
    local_left = max(year_left, row["start_year"] - year_error)
    local_right = min(year_right, row["end_year"] + year_error)
    if local_left > local_right:
      continue
    selected = clock_model_branch_error(index, local_left, local_right, year_error, source)
    collars = []
    for join in (index - 1, index):
      if not 0 <= join < 12:
        continue
      boundary = records[join]["end_year"]
      if local_left <= boundary + year_error and local_right >= boundary - year_error:
        collars.append(seam_collar_allowance(join, year_error))
    mismatch = max((row["mismatch_upper_seconds"] for row in collars), default=F(0))
    bounds.append(
      {
        "branch": index,
        "reference_year_interval": (local_left, local_right),
        "computed_magnitude_seconds": selected.magnitude,
        "same_branch_error_seconds": selected.error,
        "mismatch_seconds": mismatch,
        "total_error_seconds": selected.error + mismatch,
        "collars": collars,
      }
    )
  if not bounds:
    raise ValueError("No retained clock branch covers the complete TT interval")
  model_magnitude = max(row["computed_magnitude_seconds"] for row in bounds)
  model_error = max(row["total_error_seconds"] for row in bounds)
  shift = shifted_datetime_error(model_magnitude, model_error)
  forward = B(END_JDE_TT) + B(*rounded(F(1), F(0)))
  ut1_error = initial_quantization + shift["total_error_days"] + forward.error
  reference_delta_magnitude = model_magnitude + model_error
  ut1_reference = (left - reference_delta_magnitude / 86400, right + reference_delta_magnitude / 86400)
  return {
    "tt_interval": (left, right),
    "reference_decimal_year_interval": (year_left, year_right),
    "year_error_upper": year_error,
    "model_bounds": bounds,
    "initial_tt_quantization_days": initial_quantization,
    "shift": shift,
    "ut1_jd_forward_error_days": forward.error,
    "ut1_jd_error_days": ut1_error,
    "reference_ut1_interval": ut1_reference,
  }
