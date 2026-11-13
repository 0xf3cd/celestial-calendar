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

"""Meeus23.2/32.3 correction derivatives and the final equatorial rotation, for separated angles."""

from fractions import Fraction as F

from .poisson_majorants import DEGREE_RAD_UPPER, stored as s
from .series_bounds import product_bounds, rotation_bounds
from .spherical_coordinate_majorants import dyadic_upper
from .vector_derivative_majorants import reciprocal_bounds


def add(*sequences: tuple) -> tuple:
  if not sequences or len({len(values) for values in sequences}) != 1:
    raise ValueError("Derivative sequences must cover the same orders")
  return tuple(sum(values) for values in zip(*sequences, strict=True))


def scale(sequence: tuple, coefficient: F) -> tuple:
  return tuple(abs(coefficient) * value for value in sequence)


def polynomial(coefficients: tuple, argument: tuple) -> tuple:
  """Compose absolute polynomial coefficients with an argument derivative enclosure."""
  if not argument or any(value < 0 for value in argument):
    raise ValueError("Polynomial argument bounds must be nonnegative")
  result = (F(0),) * len(argument)
  power = (F(1),) + (F(0),) * (len(argument) - 1)
  for coefficient in coefficients:
    result = add(result, scale(power, coefficient))
    power = product_bounds(power, argument)
  return result


def aberration_bounds(
  longitude: tuple, latitude: tuple, earth_longitude: tuple, centuries: tuple, cos_latitude_lower: F
) -> dict:
  """Bound source23.2 with a proved lower cos(beta), in radians and TT-day derivatives."""
  if not 0 < cos_latitude_lower <= 1:
    raise ValueError("Annual aberration requires a certified positive cos(beta)")
  eccentricity = polynomial((s("0.016708634"), -s("0.000042037"), -s("0.0000001267")), centuries)
  perihelion = scale(polynomial((s("102.93735"), s("1.71946"), s("0.00046")), centuries), DEGREE_RAD_UPPER)
  earth_difference = rotation_bounds(add(earth_longitude, longitude))
  perihelion_difference = rotation_bounds(add(perihelion, longitude))
  wave = add(earth_difference, product_bounds(eccentricity, perihelion_difference))
  latitude_wave = rotation_bounds(latitude)
  secant = reciprocal_bounds(latitude_wave, cos_latitude_lower)
  factor = s("20.49552") * DEGREE_RAD_UPPER / 3600
  return {
    "delta_longitude_rad": dyadic_upper(scale(product_bounds(wave, secant), factor)),
    "delta_latitude_rad": dyadic_upper(scale(product_bounds(latitude_wave, wave), factor)),
  }


def fk5_bounds(longitude: tuple, latitude: tuple, centuries: tuple, cos_latitude_lower: F) -> dict:
  """Bound source32.3 at its input angles, retaining the source's polynomial drift and tangent."""
  if not 0 < cos_latitude_lower <= 1:
    raise ValueError("FK5 correction requires a certified positive cos(beta)")
  drift = scale(polynomial((F(0), s("1.397"), s("0.00031")), centuries), DEGREE_RAD_UPPER)
  phase = rotation_bounds(add(longitude, drift))
  latitude_wave = rotation_bounds(latitude)
  tangent = product_bounds(latitude_wave, reciprocal_bounds(latitude_wave, cos_latitude_lower))
  factor = s("0.03916") * DEGREE_RAD_UPPER / 3600
  lon = scale(product_bounds(scale(phase, F(2)), tangent), factor)
  lon = (lon[0] + s("0.09033") * DEGREE_RAD_UPPER / 3600,) + lon[1:]
  return {"delta_longitude_rad": dyadic_upper(lon), "delta_latitude_rad": dyadic_upper(scale(phase, 2 * factor))}


def apparent_equatorial_bounds(
  longitude: tuple,
  latitude: tuple,
  earth_longitude: tuple,
  centuries: tuple,
  cos_latitude_lower: F,
  nutation: tuple,
  true_obliquity: tuple,
) -> dict:
  """Compose the planet source's aberration, FK5, nutation and true-obliquity rotation in order."""
  aberration = aberration_bounds(longitude, latitude, earth_longitude, centuries, cos_latitude_lower)
  aberrated_lon = dyadic_upper(add(longitude, aberration["delta_longitude_rad"]))
  aberrated_lat = dyadic_upper(add(latitude, aberration["delta_latitude_rad"]))
  aberrated_cos_lower = cos_latitude_lower - aberration["delta_latitude_rad"][0]
  if aberrated_cos_lower <= 0:
    raise ValueError("The aberrated latitude cannot be certified away from its coordinate pole")
  fk5 = fk5_bounds(aberrated_lon, aberrated_lat, centuries, aberrated_cos_lower)
  apparent_lon = dyadic_upper(add(aberrated_lon, fk5["delta_longitude_rad"], nutation))
  apparent_lat = dyadic_upper(add(aberrated_lat, fk5["delta_latitude_rad"]))
  direction = product_bounds(rotation_bounds(apparent_lon), rotation_bounds(apparent_lat))
  equatorial = dyadic_upper(product_bounds(rotation_bounds(true_obliquity), direction))
  return {
    "equatorial_direction_norm": equatorial,
    "apparent_longitude_rad": apparent_lon,
    "apparent_latitude_rad": apparent_lat,
    "aberrated_cos_latitude_lower": aberrated_cos_lower,
  }
