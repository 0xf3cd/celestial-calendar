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

"""Pluto derivatives with separate inverse-FK5 and outer-light-time Jacobian contractions."""

from fractions import Fraction as F

from .apparent_correction_majorants import fk5_bounds
from .inverse_fk5_error import fk5_map_lipschitz
from .light_time_majorants import LIGHT_TIME, compose_bounds, precession_bounds
from .poisson_majorants import pi_interval
from .series_bounds import product_bounds, rotation_bounds
from .spherical_coordinate_majorants import dyadic_upper, spherical_bounds
from .vector_derivative_majorants import norm_bounds


def inverse_fk5_derivative_bounds(
  longitude: tuple, latitude: tuple, centuries: tuple, dynamic_latitude_cap: F, dynamic_cosine_lower: F
) -> dict:
  """Separate Dg*x^(k) from derivative k, then solve the max-radian-norm inequality."""
  if len(longitude) != len(latitude) or len(longitude) != len(centuries) or len(longitude) < 2:
    raise ValueError("The inverse-FK5 input and observation arguments must cover identical positive derivative orders")
  if (
    any(value < 0 for row in (longitude, latitude, centuries) for value in row)
    or not 0 < dynamic_latitude_cap < pi_interval()[0] / 2
  ):
    raise ValueError("Require nonnegative derivative bounds and an open-pole dynamic latitude cap")
  contraction = fk5_map_lipschitz(dynamic_cosine_lower)
  if not 0 <= contraction < 1:
    raise ValueError("The smooth inverse-FK5 reference requires a strict Jacobian contraction")
  derivatives = []
  audit = []
  for order in range(1, len(longitude)):
    # Zero only the unknown highest derivative; retain all lower chain/product terms.
    dynamic_longitude = (2 * pi_interval()[1], *derivatives, F(0))
    dynamic_latitude = (dynamic_latitude_cap, *derivatives, F(0))
    correction = fk5_bounds(dynamic_longitude, dynamic_latitude, centuries[: order + 1], dynamic_cosine_lower)
    remainder = max(correction["delta_longitude_rad"][order], correction["delta_latitude_rad"][order])
    forcing = max(longitude[order], latitude[order]) + remainder
    upper = dyadic_upper((forcing / (1 - contraction),))[0]
    if forcing + contraction * upper > upper:
      raise ArithmeticError("The inverse-FK5 highest-derivative inequality did not close outward")
    derivatives.append(upper)
    audit.append(
      {
        "order": order,
        "forcing_without_highest_derivative": forcing,
        "jacobian_contraction_upper": contraction,
        "upper": upper,
      }
    )
  return {
    "longitude_rad": (2 * pi_interval()[1], *derivatives),
    "latitude_rad": (dynamic_latitude_cap, *derivatives),
    "audit": audit,
  }


def pluto_curve_bounds(raw: dict, emission: tuple, centuries: tuple, value_proof: dict) -> dict:
  """Compose the real provider along emission and observation TT derivatives inside its proved box."""
  size = len(emission)
  if (
    size < 2
    or len(centuries) != size
    or any(len(raw[name]) < size for name in ("longitude_rad", "latitude_rad", "radius_au"))
  ):
    raise ValueError("The source and both time arguments must cover the requested derivative orders")
  raw_direction = product_bounds(
    rotation_bounds(raw["longitude_rad"][:size]), rotation_bounds(raw["latitude_rad"][:size])
  )
  raw_vector = product_bounds(raw["radius_au"][:size], raw_direction)
  retarded = compose_bounds(raw_vector, emission)
  frame = precession_bounds((F(0),) * size, centuries)
  precessed = dyadic_upper(product_bounds(frame, retarded))
  radial_lower = value_proof["raw"]["reference_radius_lower_au"]
  axial_lower = radial_lower * value_proof["precession_reference_axial_lower"]
  angular = spherical_bounds(precessed, radial_lower, axial_lower)
  dynamic_cap = value_proof["dynamic_reference_latitude_cap_degrees"] * pi_interval()[1] / 180
  inverse = inverse_fk5_derivative_bounds(
    angular["longitude_rad"],
    angular["latitude_rad"],
    centuries,
    dynamic_cap,
    value_proof["dynamic_reference_cosine_lower"],
  )
  direction = product_bounds(rotation_bounds(inverse["longitude_rad"]), rotation_bounds(inverse["latitude_rad"]))
  radius = compose_bounds(raw["radius_au"][:size], emission)
  return {
    "cartesian_norm_derivatives_au": dyadic_upper(product_bounds(radius, direction)),
    "precessed_angles": angular,
    "inverse": inverse,
    "radius_au": dyadic_upper(radius),
  }


def pluto_distance_bounds(
  raw: dict, earth: tuple, observation_century_radius: F, value_proof: dict, geometry: dict
) -> dict:
  """Solve each highest distance derivative using the proved emission Jacobian."""
  size = len(earth)
  if size < 2 or observation_century_radius < 0 or any(value < 0 for value in earth):
    raise ValueError("Require nonnegative Earth/epoch bounds through at least the first TT derivative")
  contraction = geometry["emission_map_contraction_upper"]
  if not 0 <= contraction < 1:
    raise ValueError("The smooth Pluto light-time reference requires a strict emission contraction")
  distance = [raw["radius_au"][0] + earth[0]]
  audit = []
  for order in range(1, size):
    trial = (*distance, F(0))
    emission = (F(0), 1 + LIGHT_TIME * trial[1], *(LIGHT_TIME * value for value in trial[2:]))
    centuries = (observation_century_radius, F(1, 36525), *((F(0),) * (order - 1)))
    target = pluto_curve_bounds(raw, emission, centuries, value_proof)
    geocentric = tuple(a + b for a, b in zip(target["cartesian_norm_derivatives_au"], earth[: order + 1], strict=True))
    forcing = norm_bounds(geocentric, geometry["radial_lower_au"])[order]
    upper = dyadic_upper((forcing / (1 - contraction),))[0]
    if forcing + contraction * upper > upper:
      raise ArithmeticError("The outer Pluto highest-derivative inequality did not close outward")
    distance.append(upper)
    audit.append(
      {
        "order": order,
        "forcing_without_highest_derivative": forcing,
        "emission_contraction_upper": contraction,
        "upper": upper,
      }
    )
  emission = (F(0), 1 + LIGHT_TIME * distance[1], *(LIGHT_TIME * value for value in distance[2:]))
  centuries = (observation_century_radius, F(1, 36525), *((F(0),) * (size - 2)))
  target = pluto_curve_bounds(raw, emission, centuries, value_proof)
  geocentric = dyadic_upper(tuple(a + b for a, b in zip(target["cartesian_norm_derivatives_au"], earth, strict=True)))
  return {
    "distance_au": tuple(distance),
    "emission_tt_days": emission,
    "target": target,
    "geocentric_cartesian_norm": geocentric,
    "audit": audit,
  }
