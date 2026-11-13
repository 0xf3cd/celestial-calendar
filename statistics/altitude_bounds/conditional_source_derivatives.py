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

"""Source derivatives for conditional altitude isolation, before observer composition."""

from fractions import Fraction as F

from .apparent_correction_majorants import add, apparent_equatorial_bounds, fk5_bounds, scale
from .light_time_majorants import LIGHT_TIME, compose_bounds, distance_bounds, precession_bounds
from .poisson_majorants import DEGREE_RAD_UPPER, moon_bounds, polynomial_bounds, stored
from .series_bounds import product_bounds, rotation_bounds
from .spherical_coordinate_majorants import dyadic_upper, spherical_bounds


def vsop_source_derivatives(target: dict, earth: dict, geometry: dict, century_radius: F, nutation: dict) -> dict:
  """Compose three derivative orders over a certified retarded rectangle with radial/axial lowers."""
  target_vector, earth_vector = (
    tuple(F(value["exact"]) for value in record["cartesian_norm_derivatives_au"][:4]) for record in (target, earth)
  )
  lower, axis = (F(geometry[name]) for name in ("radial_lower_au", "axial_lower_au"))
  distance, audit = distance_bounds(target_vector, earth_vector, century_radius, lower)
  emission = (F(0), 1 + LIGHT_TIME * distance[1]) + tuple(LIGHT_TIME * value for value in distance[2:])
  from_centuries = (century_radius + LIGHT_TIME * distance[0] / 36525,) + tuple(value / 36525 for value in emission[1:])
  between_centuries = tuple(LIGHT_TIME * value / 36525 for value in distance)
  vector = add(
    product_bounds(precession_bounds(from_centuries, between_centuries), compose_bounds(target_vector, emission)),
    earth_vector,
  )
  upper = F(geometry["centre_radial_interval"][1]) + F(geometry["tube_radius_au"])
  vector = (min(vector[0], upper),) + vector[1:]
  angles = spherical_bounds(vector, lower, axis)
  centuries = (century_radius, F(1, 36525), F(0), F(0))
  earth_longitude = tuple(F(value["exact"]) for value in earth["coordinate_derivatives"]["L"][:4])
  apparent = apparent_equatorial_bounds(
    angles["longitude_rad"],
    angles["latitude_rad"],
    earth_longitude,
    centuries,
    angles["cos_latitude_lower"],
    nutation["longitude_rad"],
    nutation["true_obliquity_rad"],
  )
  return {"direction": apparent["equatorial_direction_norm"], "distance": distance, "implicit_derivative_audit": audit}


def solar_variation_derivatives(terms: tuple, millennia_radius: F) -> tuple:
  """Bound Meeus 25.11 harmonics in arcseconds per day and TT-day derivatives."""
  total = (stored("3548.330"), F(0), F(0), F(0))
  for amplitude, phase, rate, power in terms:
    if power.denominator != 1 or not 0 <= power <= 3:
      raise ValueError("Solar variation powers must be integers from0 through3")
    polynomial = (F(0),) * int(power) + (F(1),)
    amplitude_bounds = polynomial_bounds(polynomial, millennia_radius, F(365250), 3)
    phase_bounds = scale(polynomial_bounds((phase, rate), millennia_radius, F(365250), 3), DEGREE_RAD_UPPER)
    total = add(total, scale(product_bounds(amplitude_bounds, rotation_bounds(phase_bounds)), abs(amplitude)))
  return dyadic_upper(total)


def solar_source_derivatives(earth: dict, century_radius: F, nutation: dict, terms: tuple) -> dict:
  """Compose Earth VSOP, FK5, variable aberration, IAU1980 and true obliquity."""
  longitude, latitude, distance = (
    tuple(F(value["exact"]) for value in earth["coordinate_derivatives"][family][:4]) for family in "LBR"
  )
  centuries = (century_radius, F(1, 36525), F(0), F(0))
  cosine_lower = 1 - latitude[0] ** 2 / 2
  fk5 = fk5_bounds(longitude, latitude, centuries, cosine_lower)
  variation = solar_variation_derivatives(terms, century_radius / 10)
  aberration = scale(product_bounds(distance, variation), LIGHT_TIME * DEGREE_RAD_UPPER / 3600)
  apparent_longitude = add(longitude, fk5["delta_longitude_rad"], nutation["longitude_rad"], aberration)
  apparent_latitude = add(latitude, fk5["delta_latitude_rad"])
  direction = product_bounds(
    rotation_bounds(nutation["true_obliquity_rad"]),
    product_bounds(rotation_bounds(apparent_longitude), rotation_bounds(apparent_latitude)),
  )
  return {
    "direction": dyadic_upper(direction),
    "distance": distance,
    "daily_variation": variation,
    "aberration_rad": dyadic_upper(aberration),
  }


def lunar_source_derivatives(lr: tuple, latitude_terms: tuple, century_radius: F, nutation: dict, au_km: F) -> dict:
  """Compose lunar nutation and equatorial rotation; distance derivatives use AU and TT days."""
  if au_km <= 0:
    raise ValueError("Require a positive source astronomical-unit constant")
  moon = moon_bounds(lr, latitude_terms, century_radius, 3)
  longitude = add(moon["longitude_rad"], nutation["longitude_rad"])
  direction = product_bounds(
    rotation_bounds(nutation["true_obliquity_rad"]),
    product_bounds(rotation_bounds(longitude), rotation_bounds(moon["latitude_rad"])),
  )
  return {
    "direction": dyadic_upper(direction),
    "distance": dyadic_upper(tuple(value / au_km for value in moon["radius_km"])),
    "distance_lower_au": moon["radius_lower_km"] / au_km,
  }
