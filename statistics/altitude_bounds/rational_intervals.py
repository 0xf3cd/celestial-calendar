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

"""Outward rational intervals with Taylor sin/cos for source-bound point enclosures."""

from dataclasses import dataclass
from fractions import Fraction as F
from math import ceil, factorial, floor, isqrt

from .poisson_majorants import pi_interval


GRID = 2**192


@dataclass(frozen=True)
class Interval:
  """Closed real enclosure; arithmetic endpoints round outward to a 192-bit dyadic grid."""

  lo: F
  hi: F

  def __post_init__(self):
    if self.lo > self.hi:
      raise ValueError("Interval endpoints are reversed")

  @staticmethod
  def enclose(lo, hi=None):
    lo, hi = F(lo), F(lo if hi is None else hi)
    if lo > hi:
      raise ValueError("Interval endpoints are reversed")
    return Interval(F(floor(lo * GRID), GRID), F(ceil(hi * GRID), GRID))

  def __add__(self, other):
    other = other if isinstance(other, Interval) else Interval.enclose(other)
    return Interval.enclose(self.lo + other.lo, self.hi + other.hi)

  __radd__ = __add__

  def __neg__(self):
    return Interval(-self.hi, -self.lo)

  def __sub__(self, other):
    return self + -(other if isinstance(other, Interval) else Interval.enclose(other))

  def __rsub__(self, other):
    return -self + other

  def __mul__(self, other):
    other = other if isinstance(other, Interval) else Interval.enclose(other)
    products = (self.lo * other.lo, self.lo * other.hi, self.hi * other.lo, self.hi * other.hi)
    return Interval.enclose(min(products), max(products))

  __rmul__ = __mul__

  def __truediv__(self, other):
    other = other if isinstance(other, Interval) else Interval.enclose(other)
    if other.lo <= 0 <= other.hi:
      raise ValueError("Cannot divide by an interval containing zero")
    return self * Interval.enclose(1 / other.hi, 1 / other.lo)

  def square(self):
    values = (self.lo**2, self.hi**2)
    return Interval.enclose(0 if self.lo <= 0 <= self.hi else min(values), max(values))

  def sqrt(self):
    if self.lo < 0:
      raise ValueError("Square-root interval includes negative values")
    lower = isqrt((self.lo.numerator * GRID**2) // self.lo.denominator)
    upper = isqrt((self.hi.numerator * GRID**2) // self.hi.denominator)
    if F(upper**2, GRID**2) < self.hi:
      upper += 1
    return Interval(F(lower, GRID), F(upper, GRID))


PI = Interval.enclose(*pi_interval())


def sincos(argument: Interval) -> tuple:
  """Reduce with a proved pi interval, then retain the degree47/46 Taylor remainders."""
  centre = (argument.lo + argument.hi) / 2
  quadrant = floor(2 * centre / ((PI.lo + PI.hi) / 2) + F(1, 2))
  reduced = argument - (PI * quadrant / 2)
  magnitude = max(abs(reduced.lo), abs(reduced.hi))
  if magnitude > 1:
    raise ValueError("Argument interval is too wide for the bounded Taylor reduction")
  squared = reduced.square()
  sine = Interval.enclose(0)
  cosine = Interval.enclose(0)
  for j in reversed(range(24)):
    sine = sine * squared + F((-1) ** j, factorial(2 * j + 1))
    cosine = cosine * squared + F((-1) ** j, factorial(2 * j))
  sine = sine * reduced
  sin_error = magnitude**49 / factorial(49)
  cos_error = magnitude**48 / factorial(48)
  sine += Interval.enclose(-sin_error, sin_error)
  cosine += Interval.enclose(-cos_error, cos_error)
  return ((sine, cosine), (cosine, -sine), (-sine, -cosine), (-cosine, sine))[quadrant % 4]
