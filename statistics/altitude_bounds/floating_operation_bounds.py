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

"""Magnitude/error propagation for the source's unreassociated binary64 arithmetic expressions."""

from dataclasses import dataclass
from fractions import Fraction as F
from math import ceil

from .heliocentric_evaluation_error import DEG_PER_RAD_FP
from .poisson_majorants import pi_interval
from .raw_evaluation_error import LIBM_ALLOWANCE, rounded


@dataclass(frozen=True)
class FloatingBound:
  """Computed magnitude and absolute gap to the matching exact-real source expression."""

  magnitude: F
  error: F = F(0)

  def __post_init__(self):
    if self.magnitude < 0 or self.error < 0:
      raise ValueError("Computed magnitudes and reference errors must be nonnegative")
    object.__setattr__(self, "magnitude", F(ceil(self.magnitude * 2**192), 2**192))
    object.__setattr__(self, "error", F(ceil(self.error * 2**192), 2**192))

  @staticmethod
  def literal(value):
    return FloatingBound(abs(F.from_float(float(value))))

  def __neg__(self):
    return self

  def __add__(self, other):
    other = other if isinstance(other, FloatingBound) else FloatingBound.literal(other)
    return FloatingBound(*rounded(self.magnitude + other.magnitude, self.error + other.error))

  __radd__ = __add__

  def __sub__(self, other):
    return self + other

  def __rsub__(self, other):
    return self + other

  def __mul__(self, other):
    other = other if isinstance(other, FloatingBound) else FloatingBound.literal(other)
    propagated = self.magnitude * other.error + other.magnitude * self.error + self.error * other.error
    return FloatingBound(*rounded(self.magnitude * other.magnitude, propagated))

  __rmul__ = __mul__

  def __truediv__(self, denominator):
    if isinstance(denominator, FloatingBound):
      raise ValueError("This source-operation bound only divides by an exact stored scalar")
    denominator = abs(F.from_float(float(denominator)))
    if denominator == 0:
      raise ValueError("Cannot divide by zero")
    return FloatingBound(*rounded(self.magnitude / denominator, self.error / denominator))

  def sine_or_cosine(self):
    if self.magnitude > 2**40:
      raise ValueError("Elementary argument exceeds the retained source premise")
    return FloatingBound(1 + LIBM_ALLOWANCE, self.error + LIBM_ALLOWANCE)

  def degrees_to_radians(self):
    lower, upper = (v / 180 for v in pi_interval())
    factor_error = max(abs(1 / DEG_PER_RAD_FP - lower), abs(1 / DEG_PER_RAD_FP - upper))
    magnitude, rounding = rounded(self.magnitude / DEG_PER_RAD_FP, F(0))
    error = self.error * upper + self.magnitude * factor_error + rounding
    return FloatingBound(magnitude, error)
