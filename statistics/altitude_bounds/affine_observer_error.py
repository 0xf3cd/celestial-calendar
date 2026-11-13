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

"""Nonnegative affine-in-|height_km| allowances for stored observer expressions."""

from dataclasses import dataclass
from fractions import Fraction as F
from math import ceil

from .apparent_correction_error import positive_quotient
from .floating_operation_bounds import FloatingBound as B
from .observer_evaluation_error import positive_sqrt
from .rational_intervals import Interval as I
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF


GRID = F(1, 2**192)


@dataclass(frozen=True)
class Affine:
  constant: F = F(0)
  height: F = F(0)

  def __post_init__(self):
    if min(self.constant, self.height) < 0:
      raise ValueError("Affine magnitude coefficients must be nonnegative")

  def __call__(self, height: F) -> F:
    if height < 0:
      raise ValueError("The affine argument is an absolute height in km")
    return self.constant + self.height * height

  def __add__(self, other):
    other = other if isinstance(other, Affine) else Affine(F(other))
    return Affine(self.constant + other.constant, self.height + other.height)

  __radd__ = __add__

  def __mul__(self, other):
    other = other if isinstance(other, Affine) else Affine(F(other))
    if self.height * other.height:
      raise ValueError("A product of height-dependent allowances is not affine")
    return Affine(self.constant * other.constant, self.constant * other.height + self.height * other.constant)

  __rmul__ = __mul__

  def enclose_grid(self):
    constant = F(ceil(self.constant / GRID)) * GRID
    height = F(ceil(self.height / GRID)) * GRID
    # ceil(c+s*H) <= c+s*H+GRID for every H>=0, including fractional heights.
    return Affine(constant + (GRID if self.height else 0), height)


def rounded(magnitude: Affine, error: Affine) -> tuple:
  allowance = magnitude * UNIT_ROUNDOFF + SUBNORMAL_ALLOWANCE
  return magnitude + allowance, error + allowance


@dataclass(frozen=True)
class HeightBound:
  magnitude: Affine
  error: Affine = Affine()

  def __post_init__(self):
    object.__setattr__(self, "magnitude", self.magnitude.enclose_grid())
    object.__setattr__(self, "error", self.error.enclose_grid())

  @staticmethod
  def fixed(value):
    value = value if isinstance(value, B) else B.literal(value)
    return HeightBound(Affine(value.magnitude), Affine(value.error))

  def __add__(self, other):
    other = other if isinstance(other, HeightBound) else HeightBound.fixed(other)
    return HeightBound(*rounded(self.magnitude + other.magnitude, self.error + other.error))

  __sub__ = __add__

  def __mul__(self, other):
    other = other if isinstance(other, HeightBound) else HeightBound.fixed(other)
    error = self.magnitude * other.error + other.magnitude * self.error + self.error * other.error
    return HeightBound(*rounded(self.magnitude * other.magnitude, error))

  def __truediv__(self, denominator):
    denominator = F(float(denominator))
    if denominator <= 0:
      raise ValueError("Height division needs a positive stored scalar")
    return HeightBound(*rounded(self.magnitude * (1 / denominator), self.error * (1 / denominator)))


def affine_observer_error(
  latitude: B, direction_error: F, sidereal_degree_error: F, distance: B, distance_lower: F, constants: tuple
) -> dict:
  """Bound WGS84 projection and hypot as an affine function of absolute elevation in km."""
  if min(direction_error, sidereal_degree_error) < 0:
    raise ValueError("Direction and sidereal allowances must be nonnegative")
  radius, polar, au = constants
  if radius <= 0 or au <= 0 or not 0 < polar <= 1:
    raise ValueError("Require positive WGS84 radius/AU and a unit-range polar ratio square")
  wave = latitude.degrees_to_radians().sine_or_cosine()
  radicand = B.literal(1) - ((B.literal(1) - B(polar)) * wave * wave)
  normal = positive_quotient(B.literal(1), positive_sqrt(radicand, polar), I.enclose(polar).sqrt().lo)
  height = HeightBound(Affine(0, 1)) / radius
  equatorial = (HeightBound.fixed(normal) + height) * wave
  polar_site = (HeightBound.fixed(B(polar) * normal) + height) * wave
  p = positive_quotient(B(radius) / au, distance, distance_lower)
  hour_angle = (B(F(360), sidereal_degree_error) - B(F(360))).degrees_to_radians()
  hour_error = hour_angle.error + B(F(360)).degrees_to_radians().error
  trig = B(1 + LIBM_ALLOWANCE, LIBM_ALLOWANCE)
  product = trig * trig
  a = HeightBound.fixed(B(product.magnitude, product.error + direction_error + hour_error))
  b = HeightBound.fixed(B(product.magnitude, product.error + direction_error + hour_error)) - equatorial * p
  c = HeightBound.fixed(B(trig.magnitude, trig.error + direction_error)) - polar_site * p
  up = (c * wave) + (b * wave)
  q_elementary = (a.magnitude + b.magnitude + c.magnitude) * LIBM_ALLOWANCE + SUBNORMAL_ALLOWANCE
  q = HeightBound(
    (a.magnitude + b.magnitude + c.magnitude) * (1 + LIBM_ALLOWANCE) + SUBNORMAL_ALLOWANCE,
    a.error + b.error + c.error + q_elementary,
  )
  residual = up - HeightBound.fixed(B(F(90)).degrees_to_radians().sine_or_cosine()) * q
  return {
    "up": up,
    "q": q,
    "A": a,
    "B": b,
    "C": c,
    "residual": residual,
    "site_equatorial": equatorial,
    "site_polar": polar_site,
    "parallax": p,
  }
