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

"""Whole-tier planetary apparent values and derivatives from proved retardation geometry."""

from fractions import Fraction as F

from .apparent_correction_error import correction_chain_from_centuries, degree_input_error
from .apparent_correction_majorants import apparent_equatorial_bounds
from .equatorial_evaluation_error import equatorial_error, rotated_axial_lower
from .floating_operation_bounds import FloatingBound as B
from .heliocentric_evaluation_error import DEG_PER_RAD_FP
from .light_time_majorants import LIGHT_TIME, compose_bounds, precession_bounds
from .nutation_evaluation_error import nutation_error_from_centuries
from .nutation_majorants import nutation_bounds, nutation_rows
from .poisson_majorants import pi_interval
from .rational_intervals import Interval as I
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, rounded
from .retarded_angular_error import angular_allowance_from_errors
from .rounded_light_time_reach import uniform_map_error, uniform_raw_error
from .series_bounds import product_bounds
from .spherical_coordinate_majorants import dyadic_upper, spherical_bounds
from .uniform_apparent_error import accepted_fixed_point_errors


def whole_tier_certificate(
  target: dict, earth: dict, geometry: dict, precession_source: str, earth_source: str, expressions: dict
) -> dict:
  """Bind both source records to the complete observation tier and rounded emission reserve."""
  proof_radius = F(geometry["source_tau_radius"])
  if any(F(record["tau_radius_exact"]) != proof_radius for record in (target, earth)):
    raise ValueError("The apparent certificate source records do not match the geometry proof radius")
  left, right = map(F, geometry["observation_interval"])
  if not 0 < left < right:
    raise ValueError("The fixed-point value certificate requires positive TT endpoints")
  reserve = F(geometry["rounded_delay_reserve_days"])
  radial_lower, axial_lower = (F(geometry[name]) for name in ("radial_lower_au", "axial_lower_au"))
  raw_target, raw_earth = (
    tuple(F(item["exact"]) for item in row["cartesian_norm_derivatives_au"]) for row in (target, earth)
  )
  r = tuple(map(F, geometry["distance_derivatives"]))
  radius_upper = raw_target[0] + raw_earth[0]
  century_radius = F(geometry["selection_tau_radius"]) * 10
  emission = (F(0), 1 + LIGHT_TIME * r[1]) + tuple(LIGHT_TIME * value for value in r[2:])
  origin = (century_radius + LIGHT_TIME * r[0] / 36525,) + tuple(value / 36525 for value in emission[1:])
  delay = tuple(LIGHT_TIME * value / 36525 for value in r)
  frame = precession_bounds(origin, delay)
  retarded = product_bounds(frame, compose_bounds(raw_target, emission))
  vector = dyadic_upper(tuple(a + b for a, b in zip(retarded, raw_earth, strict=True)))

  target_beta, earth_beta = (F(record["coordinate_derivatives"]["B"][0]["exact"]) for record in (target, earth))
  z_upper = (
    raw_target[0] * (target_beta + F(geometry["short_date_precession_norm_deviation_upper"]))
    + raw_earth[0] * earth_beta
  )
  cosine_lower = axial_lower / radius_upper
  if z_upper < radial_lower:
    cosine_lower = max(cosine_lower, I.enclose(1 - (z_upper / radial_lower) ** 2).sqrt().lo)
  angles = spherical_bounds(vector, radial_lower, axial_lower)
  centuries = (century_radius, F(1, 36525)) + (F(0),) * (len(r) - 2)
  iau = nutation_bounds(nutation_rows(earth_source), century_radius, len(r) - 1)
  longitude = tuple(F(value["exact"]) for value in earth["coordinate_derivatives"]["L"])
  apparent = apparent_equatorial_bounds(
    angles["longitude_rad"],
    angles["latitude_rad"],
    longitude,
    centuries,
    cosine_lower,
    iau["longitude_rad"],
    iau["true_obliquity_rad"],
  )

  uniform = uniform_map_error(target, earth, (left + right) / 2, (right - left) / 2, reserve, precession_source)
  if uniform["rounded_image_delay_upper_days"] > reserve:
    raise ValueError("The rounded whole-tier map does not preserve its complete delay reserve")
  emission_speed = F(geometry["emission_map_contraction_upper"]) / LIGHT_TIME
  fixed = accepted_fixed_point_errors(
    uniform,
    {
      "emission_map_contraction_upper": F(geometry["emission_map_contraction_upper"]),
      "emission_speed_au_per_day": emission_speed,
    },
    left,
    right,
  )
  angular = angular_allowance_from_errors(
    fixed["fixed_point_vector_error_upper_au"],
    LIBM_ALLOWANCE * uniform["computed_vector_l1_upper_au"] + SUBNORMAL_ALLOWANCE,
    radial_lower,
    axial_lower,
  )
  century_value = B(*rounded(century_radius * 36525, F(0))) / 36525
  earth_value = uniform_raw_error(earth, left, right)
  beta_magnitude, _ = rounded((pi_interval()[1] / 2 + LIBM_ALLOWANCE) * DEG_PER_RAD_FP, F(0))
  nutation = nutation_error_from_centuries(century_value, earth_source)
  corrections = correction_chain_from_centuries(
    century_value,
    B(F(360), degree_input_error(F(360), angular["longitude_circular_error_rad"])),
    B(beta_magnitude, degree_input_error(beta_magnitude, angular["latitude_error_rad"])),
    B(F(360), degree_input_error(F(360), earth_value["longitude_circular_error_rad"])),
    nutation["longitude_degrees"],
    cosine_lower,
    expressions,
  )
  final_latitude = corrections["aberrated_latitude_degrees"] + corrections["fk5_latitude_degrees"]
  latitude_correction = (
    (
      corrections["annual_latitude_degrees"].magnitude
      + corrections["annual_latitude_degrees"].error
      + corrections["fk5_latitude_degrees"].magnitude
      + corrections["fk5_latitude_degrees"].error
    )
    * pi_interval()[1]
    / 180
  )
  obliquity = nutation["true_obliquity_degrees"]
  equatorial_axis = rotated_axial_lower(
    cosine_lower, latitude_correction, (obliquity.magnitude + obliquity.error) * pi_interval()[1] / 180
  )
  equatorial = equatorial_error(
    B(F(360), degree_input_error(F(360), corrections["final_longitude_radians"].error)),
    B(
      final_latitude.magnitude,
      degree_input_error(final_latitude.magnitude, corrections["final_latitude_radians"].error),
    ),
    obliquity,
    equatorial_axis,
  )
  distance = B(
    (1 + LIBM_ALLOWANCE) * uniform["computed_vector_l1_upper_au"] + SUBNORMAL_ALLOWANCE,
    fixed["fixed_point_distance_error_upper_au"],
  )
  return {
    "observation_interval": (left, right),
    "geometry": geometry,
    "reference_cosine_lower": cosine_lower,
    "reference_equatorial_axis_lower": equatorial_axis,
    "vector_derivatives_au": vector,
    "distance_derivatives_au": r,
    "apparent": apparent,
    "uniform_map": uniform,
    "fixed_point": fixed,
    "geometric_angles": angular,
    "equatorial": equatorial,
    "distance_value": distance,
    "final_longitude_radian_error": corrections["final_longitude_radians"].error,
    "final_latitude_radian_error": corrections["final_latitude_radians"].error,
  }
