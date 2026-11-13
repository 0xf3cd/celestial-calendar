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

"""Derivative product and rotation rules, independent VSOP moments and outward serialization."""

import math
from fractions import Fraction
from math import comb, factorial


def outward(value: Fraction) -> dict:
  """Retain the exact nonnegative bound and its least binary64 upper neighbour."""
  if value < 0:
    raise ValueError("An upper magnitude must be nonnegative")
  rounded = float(value)
  if not math.isfinite(rounded):
    raise ValueError("A bound cannot be represented by a finite binary64 value")
  if Fraction.from_float(rounded) < value:
    rounded = math.nextafter(rounded, math.inf)
  if Fraction.from_float(rounded) < value:
    raise ArithmeticError("Binary64 upper serialization failed")
  if rounded > 0 and Fraction.from_float(math.nextafter(rounded, -math.inf)) >= value:
    raise ArithmeticError("Binary64 upper serialization is not minimal")
  return {"exact": str(value), "hex": rounded.hex()}


def product_bounds(left: tuple, right: tuple) -> tuple:
  """Apply the derivative product rule to two uniform nonnegative bound sequences."""
  if len(left) != len(right):
    raise ValueError("Derivative sequences must cover the same orders")
  return tuple(sum(comb(k, j) * left[j] * right[k - j] for j in range(k + 1)) for k in range(len(left)))


def rotation_bounds(angle: tuple) -> tuple:
  """Bound derivatives of a fixed-axis rotation from its angle derivatives, with operator norm one."""
  result = [Fraction(1)]
  for k in range(1, len(angle)):
    result.append(sum(comb(k - 1, j - 1) * angle[j] * result[k - j] for j in range(1, k + 1)))
  return tuple(result)


def moment_check(tables: dict, radius: Fraction, order: int) -> tuple:
  """Independently convolve (T+x)^n with exp(|C|x) in Taylor-coefficient form."""
  coefficients = [Fraction(0)] * (order + 1)
  for power, terms in tables.items():
    moments = [
      sum(abs(amplitude) * abs(frequency) ** k for amplitude, _phase, frequency in terms) for k in range(order + 1)
    ]
    polynomial = [Fraction(comb(power, j)) * radius ** (power - j) for j in range(power + 1)]
    for k in range(order + 1):
      coefficients[k] += sum(polynomial[j] * moments[k - j] / factorial(k - j) for j in range(min(power, k) + 1))
  return tuple(value * factorial(k) / (100_000_000 * 365_250**k) for k, value in enumerate(coefficients))
