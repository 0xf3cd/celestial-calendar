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

"""Exact Leibniz majorants for the raw VSOP87D series, before frame or apparent-place corrections."""

from fractions import Fraction
from math import comb, factorial


def term_derivative_bound(
  amplitude: Fraction,
  frequency: Fraction,
  power: int,
  order: int,
  tau_max: Fraction,
) -> Fraction:
  """Bound d^order/dtau^order of A*tau^power*cos(B+C*tau), uniformly on |tau|<=tau_max."""
  if power < 0 or order < 0 or tau_max < 0:
    raise ValueError("Power, derivative order and tau radius must be nonnegative")
  return abs(amplitude) * sum(
    Fraction(comb(order, j) * factorial(power), factorial(power - j))
    * tau_max ** (power - j)
    * abs(frequency) ** (order - j)
    for j in range(min(power, order) + 1)
  )


def coordinate_derivative_bounds(tables, tau_max: Fraction, order: int = 7) -> tuple[Fraction, ...]:
  """Apply the source's 1e8 amplitude scale and 365250 TT-days/millennium to every retained term."""
  if order < 0 or tau_max < 0:
    raise ValueError("Derivative order and tau radius must be nonnegative")
  result = [Fraction(0) for _ in range(order + 1)]
  for power, terms in tables.items():
    for amplitude, _phase, frequency in terms:
      for k in range(order + 1):
        result[k] += term_derivative_bound(amplitude, frequency, power, k, tau_max)
  return tuple(value / (100_000_000 * 365_250**k) for k, value in enumerate(result))
