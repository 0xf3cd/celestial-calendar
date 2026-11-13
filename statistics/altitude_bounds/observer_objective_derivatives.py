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

"""Fixed-observer altitude derivatives from direction, distance and clock source bounds."""

from fractions import Fraction as F

from .altitude_objective_majorants import objective_bounds
from .observer_geometry_majorants import projected_objective_bounds
from .rational_intervals import Interval as I, PI, sincos
from .series_bounds import product_bounds, rotation_bounds
from .spherical_coordinate_majorants import dyadic_upper
from .vector_derivative_majorants import reciprocal_bounds


def reference_site_bounds(site: tuple, constants: tuple) -> dict:
  """Enclose the stored-constant WGS84 site; poles retain an exact zero equatorial component."""
  latitude, _, elevation = site
  radius, polar, _ = constants
  if not -90 <= latitude <= 90 or radius <= 0 or not 0 < polar <= 1:
    raise ValueError("Require an admitted latitude and positive WGS84 source constants")
  if abs(latitude) == 90:
    sine, cosine = I.enclose(1 if latitude > 0 else -1), I.enclose(0)
  elif latitude == 0:
    sine, cosine = I.enclose(0), I.enclose(1)
  else:
    sine, cosine = sincos(I.enclose(latitude) * PI / 180)
  normal = I.enclose(1) / (1 - (1 - polar) * sine.square()).sqrt()
  height = I.enclose(elevation) / radius
  equatorial = (normal + height) * cosine
  axial = (polar * normal + height) * sine
  magnitude = lambda value: max(abs(value.lo), abs(value.hi))  # noqa: E731 - Local interval-magnitude projection.
  return {
    "equatorial_upper": magnitude(equatorial),
    "polar_upper": magnitude(axial),
    "cos_latitude_upper": min(F(1), magnitude(cosine)),
    "sin_latitude_upper": min(F(1), magnitude(sine)),
  }


def local_vector_derivatives(
  direction: tuple, distance: tuple, rotation: tuple, distance_lower: F, site: tuple, constants: tuple
) -> dict:
  """Bound the translated vector without assuming a positive observer-to-body norm lower."""
  if (
    any(len(values) != 4 or any(value < 0 for value in values) for values in (direction, distance, rotation))
    or direction[0] != 1
  ):
    raise ValueError("Require source sequences through order3 and an exactly unit reference direction")
  if distance_lower <= 0:
    raise ValueError("Require positive geocentric distance separation")
  source_site = reference_site_bounds(site, constants)
  parallax = tuple(constants[0] / constants[2] * value for value in reciprocal_bounds(distance, distance_lower))
  rotated = product_bounds(rotation_bounds(rotation), direction)
  site_l1 = source_site["equatorial_upper"] + source_site["polar_upper"]
  vector = dyadic_upper(tuple(value + site_l1 * p for value, p in zip(rotated, parallax, strict=True)))
  return {"site": source_site, "parallax_derivatives": parallax, "local_vector_derivatives": vector}


def observer_objective_derivatives(
  direction: tuple,
  distance: tuple,
  rotation: tuple,
  distance_lower: F,
  q_lower: F,
  site: tuple,
  constants: tuple,
  sine_level_upper: F,
) -> dict:
  """Bound TT derivatives 1..3; q is dimensionless, distance is AU and rotation is in radians."""
  local = local_vector_derivatives(direction, distance, rotation, distance_lower, site, constants)
  if q_lower <= 0 or not 0 <= sine_level_upper <= 1:
    raise ValueError("Require positive local separation and a unit-range level sine bound")
  source_site, parallax, vector = (local[name] for name in ("site", "parallax_derivatives", "local_vector_derivatives"))
  generic = objective_bounds(vector, q_lower, sine_level_upper)
  projected = projected_objective_bounds(
    direction,
    rotation,
    parallax,
    source_site["equatorial_upper"],
    source_site["polar_upper"],
    source_site["cos_latitude_upper"],
    source_site["sin_latitude_upper"],
    q_lower,
    sine_level_upper,
  )
  tight = dyadic_upper(tuple(min(a, b) for a, b in zip(generic, projected["objective_derivatives"], strict=True)))
  return {
    "site": source_site,
    "parallax_derivatives": dyadic_upper(parallax),
    "local_vector_derivatives": vector,
    "generic_objective_derivatives": dyadic_upper(generic),
    "projected": projected,
    "objective_derivatives": tight,
  }
