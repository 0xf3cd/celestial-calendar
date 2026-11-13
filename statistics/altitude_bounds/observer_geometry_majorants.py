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

"""Latitude-sensitive altitude derivatives using q^2=1+p^2|k|^2-2p(k dot w)."""

from fractions import Fraction as F

from .series_bounds import product_bounds, rotation_bounds


def projected_objective_bounds(
  equatorial_direction: tuple,
  rotation_angle: tuple,
  parallax: tuple,
  site_equatorial: F,
  site_polar: F,
  cos_latitude: F,
  sin_latitude: F,
  separation: F,
  sine_level: F,
) -> dict:
  """Bound three derivatives of fixed-observer up-s*q using the translated unit-vector norm."""
  if not all(
    len(values) == 4 and all(value >= 0 for value in values)
    for values in (equatorial_direction, rotation_angle, parallax)
  ):
    raise ValueError("Require nonnegative derivative magnitudes through order3")
  if separation <= 0 or equatorial_direction[0] != 1 or not 0 <= sine_level <= 1:
    raise ValueError("Require a unit reference direction, positive separation and |sin(level)|<=1")
  c, s = abs(cos_latitude), abs(sin_latitude)
  if c > 1 or s > 1:
    raise ValueError("Latitude sine/cosine magnitudes must not exceed one")
  a, b = abs(site_equatorial), abs(site_polar)
  rotated = product_bounds(rotation_bounds(rotation_angle), equatorial_direction)
  projected_site = c * a + s * b
  up = tuple(c * rotated[k] + s * equatorial_direction[k] + projected_site * parallax[k] for k in range(4))
  site_dot = tuple(a * rotated[k] + b * equatorial_direction[k] for k in range(4))
  p_squared = product_bounds(parallax, parallax)
  p_dot = product_bounds(parallax, site_dot)
  squared_norm = tuple((a * a + b * b) * p_squared[k] + 2 * p_dot[k] for k in range(4))
  _, first, second, third = squared_norm
  norm = (
    F(0),
    first / (2 * separation),
    second / (2 * separation) + first**2 / (4 * separation**3),
    third / (2 * separation) + 3 * first * second / (4 * separation**3) + 3 * first**3 / (8 * separation**5),
  )
  return {
    "objective_derivatives": tuple(up[k] + sine_level * norm[k] for k in range(1, 4)),
    "norm_derivatives": norm[1:],
    "up_derivatives": up[1:],
  }
