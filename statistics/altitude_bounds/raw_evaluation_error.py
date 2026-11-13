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

"""Absolute error bounds for the retained raw VSOP evaluator at an exact binary64 tau input."""

from fractions import Fraction as F


UNIT_ROUNDOFF = F(1, 2**53)
SUBNORMAL_ALLOWANCE = F(1, 2**1074)
LIBM_ALLOWANCE = F(1, 2**48)


def rounded(magnitude: F, error: F) -> tuple[F, F]:
  """Bound one binary64 arithmetic result relative to an already bounded exact-real expression."""
  allowance = UNIT_ROUNDOFF * magnitude + SUBNORMAL_ALLOWANCE
  return magnitude + allowance, error + allowance


def reduction_allowance(count: int, sum_magnitudes: F) -> F:
  """Bound a sequential sum under either normal rounding or gradual underflow."""
  nu = count * UNIT_ROUNDOFF
  if count < 0 or nu >= 1:
    raise ValueError("Sequential reduction needs count*unit_roundoff below one")
  return (nu * sum_magnitudes + count * SUBNORMAL_ALLOWANCE) / (1 - nu)


def table_error(terms: tuple, tau_max: F) -> tuple[F, F]:
  """Bound the computed magnitude and absolute error of evaluate_table, including its 1e8 division."""
  computed_magnitudes = F(0)
  errors = F(0)
  for amplitude, phase, frequency in terms:
    product_magnitude, product_error = rounded(abs(frequency) * tau_max, F(0))
    argument_magnitude, argument_error = rounded(abs(phase) + product_magnitude, product_error)
    if argument_magnitude > 2**40:
      raise ValueError("Cosine argument exceeds the retained elementary-function premise")
    cosine_error = argument_error + LIBM_ALLOWANCE
    term_magnitude, term_rounding = rounded(abs(amplitude) * (1 + LIBM_ALLOWANCE), F(0))
    computed_magnitudes += term_magnitude
    errors += abs(amplitude) * cosine_error + term_rounding
  summation = reduction_allowance(len(terms), computed_magnitudes)
  return rounded((computed_magnitudes + summation) / 100_000_000, (errors + summation) / 100_000_000)


def coordinate_error(tables: dict, tau_max: F) -> tuple[F, F]:
  """Follow reverse-table Horner evaluation; contraction can only remove intermediate rounding here."""
  if tau_max < 0 or sorted(tables) != list(range(len(tables))):
    raise ValueError("Require a nonnegative tau radius and consecutive powers starting at zero")
  magnitude, error = F(0), F(0)
  for power in reversed(range(len(tables))):
    table_magnitude, table_allowance = table_error(tables[power], tau_max)
    product_magnitude, product_allowance = rounded(magnitude * tau_max, error * tau_max)
    magnitude, error = rounded(product_magnitude + table_magnitude, product_allowance + table_allowance)
  return magnitude, error
