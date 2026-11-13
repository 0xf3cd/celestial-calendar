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

"""Exact Gregorian/JDE lattice and datetime-rounding allowances for the fixed TT-to-UT1 path."""

from fractions import Fraction as F

from .floating_operation_bounds import FloatingBound as B
from .raw_evaluation_error import SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF, rounded


NANOSECONDS_PER_DAY = 86400000000000
START_JDE_TT = F(3735045, 2)
END_JDE_TT = F(27378651, 2)


def midnight_jd(year: int, month: int, day: int) -> F:
  """Exact proleptic-Gregorian integer day count, with the Julian noon/midnight half-day offset."""
  a = (14 - month) // 12
  y = year + 4800 - a
  m = month + 12 * a - 3
  jdn = day + (153 * m + 2) // 5 + 365 * y + y // 4 - y // 100 + y // 400 - 32045
  return F(jdn) - F(1, 2)


def tt_datetime_error_upper_ns() -> F:
  """Bound the final fraction-to-nanoseconds product rounding and truncation, independently of observations."""
  return 1 + UNIT_ROUNDOFF * NANOSECONDS_PER_DAY + SUBNORMAL_ALLOWANCE


def exact_jde_fraction(observed: F) -> dict:
  """Resolve one admitted stored JDE on its at-most32-fraction-bit lattice, before the nanosecond cast."""
  if not START_JDE_TT <= observed < END_JDE_TT or F(float(observed)) != observed:
    raise ValueError("Require an exact binary64 TT input in the existing inverse-calendar domain")
  if (observed * 2**32).denominator != 1:
    raise ArithmeticError("The admitted positive-JDE binary64 lattice premise is false")
  day_number = (observed + F(1, 2)).numerator // (observed + F(1, 2)).denominator
  fraction = observed - day_number + F(1, 2)
  if not 0 <= fraction < 1 or F(float(fraction)) != fraction:
    raise ArithmeticError("The compensated inverse-date fraction is not exactly representable")
  nanos = int(float(fraction * NANOSECONDS_PER_DAY))
  error = F(nanos) - fraction * NANOSECONDS_PER_DAY
  if abs(error) > tt_datetime_error_upper_ns() or not 0 <= nanos < NANOSECONDS_PER_DAY:
    raise ArithmeticError("The source nanosecond conversion exceeds its rounding/truncation theorem")
  return {
    "julian_day_number": day_number,
    "exact_fraction": fraction,
    "predicted_nanoseconds": nanos,
    "quantization_error_ns": error,
  }


def inverse_integer_calendar(day_number: int) -> tuple:
  """Evaluate the source's integral Gregorian inverse, whose400-year translation leaves all remainders unchanged."""
  offset = day_number + 68569
  n = (4 * offset) // 146097
  offset -= ((146097 * n) + 3) // 4
  i = (4000 * (offset + 1)) // 1461001
  offset -= ((1461 * i) // 4) - 31
  k = (80 * offset) // 2447
  day = offset - ((2447 * k) // 80)
  offset = k // 11
  month = k + 2 - 12 * offset
  year = 100 * (n - 49) + i + offset
  return year, month, day


def shifted_datetime_error(delta_magnitude_seconds: F, delta_error_seconds: F) -> dict:
  """Bound add_seconds relative to its stored input datetime, including either source day-carry path."""
  if min(delta_magnitude_seconds, delta_error_seconds) < 0 or delta_magnitude_seconds >= 2_000_000_000 * 86400:
    raise ValueError("Require finite nonnegative model bounds within the existing source offset guard")
  fraction = B(*rounded(F(1), F(0)))
  offset = B(delta_magnitude_seconds) / 86400
  shifted = fraction - offset
  model_error_days = delta_error_seconds / 86400
  carry_error = UNIT_ROUNDOFF + SUBNORMAL_ALLOWANCE
  quantization = tt_datetime_error_upper_ns() / NANOSECONDS_PER_DAY
  return {
    "fraction_operation_error_days": shifted.error,
    "model_error_days": model_error_days,
    "carry_error_days": carry_error,
    "nanosecond_quantization_error_days": quantization,
    "total_error_days": shifted.error + model_error_days + carry_error + quantization,
  }
