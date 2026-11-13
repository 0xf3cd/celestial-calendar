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

"""Outward powers-of-two encoding for nonnegative source-certificate fields."""

from fractions import Fraction as F


ZERO = -128
MIN_EXPONENT = -127
MAX_EXPONENT = 127


def decode(exponent: int) -> F:
  if type(exponent) is not int or not ZERO <= exponent <= MAX_EXPONENT:
    raise ValueError("A certificate exponent must fit one signed byte")
  return F(0) if exponent == ZERO else F(2) ** exponent


def encode(value: F, upper: bool) -> int:
  """Round to a power of two in the required direction; out-of-range positive bounds fail."""
  value = F(value)
  if value < 0:
    raise ValueError("Certificate magnitudes must be nonnegative")
  if value == 0:
    return ZERO
  exponent = value.numerator.bit_length() - value.denominator.bit_length()
  if F(2) ** exponent > value:
    exponent -= 1
  if upper and F(2) ** exponent < value:
    exponent += 1
  if not MIN_EXPONENT <= exponent <= MAX_EXPONENT:
    raise ValueError("The nonzero certificate bound is outside the signed-byte exponent range")
  if decode(exponent) < value if upper else decode(exponent) > value:
    raise ArithmeticError("Certificate serialization rounded in the wrong direction")
  return exponent
