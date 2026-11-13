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

"""Whole-box Pluto light-time, apparent-correction and equatorial value-error composition."""

from fractions import Fraction as F

from .apparent_correction_error import correction_chain_from_centuries, degree_input_error
from .equatorial_evaluation_error import equatorial_error, rotated_axial_lower
from .floating_operation_bounds import FloatingBound as B
from .heliocentric_evaluation_error import DEG_PER_RAD_FP
from .light_time_majorants import LIGHT_TIME
from .nutation_evaluation_error import nutation_error_from_centuries
from .pluto_provider_error import pluto_provider_error
from .poisson_majorants import array_rows, pi_interval, pluto_bounds
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF, rounded
from .retarded_angular_error import angular_allowance_from_errors
from .rounded_light_time_reach import uniform_raw_error
from .uniform_apparent_error import accepted_fixed_point_errors


def pluto_apparent_error(
  left: F,
  right: F,
  earth_record: dict,
  pluto_source: str,
  precession_source: str,
  earth_source: str,
  correction_expressions: dict,
) -> dict:
  """Prove the one-day past reserve and bound every accepted old outer iterate."""
  emission_left, emission_right = left - 1, right
  provider = pluto_provider_error(
    emission_left,
    emission_right,
    left,
    right,
    pluto_source,
    precession_source,
    correction_expressions["fk5_drift_degrees"],
  )
  earth = uniform_raw_error(earth_record, left, right)
  earth_radius = F(earth_record["cartesian_norm_derivatives_au"][0]["exact"])
  target_radius = provider["raw"]["radius_au"].magnitude + provider["raw"]["radius_au"].error
  target_radius_lower = provider["raw"]["reference_radius_lower_au"]
  radial_lower = target_radius_lower - earth_radius
  axial_lower = target_radius_lower * provider["dynamic_reference_cosine_lower"] - earth_radius
  if min(radial_lower, axial_lower) <= 0:
    raise ValueError("The complete Pluto rectangle has no positive geocentric radial/axial separation")
  # (r*c-e)/(r+e) increases with r: its derivative is e*(1+c)/(r+e)^2.
  geocentric_cosine_lower = axial_lower / (target_radius_lower + earth_radius)
  component_sum = (target_radius + earth_radius) * (1 + LIBM_ALLOWANCE) ** 2 * (
    1 + UNIT_ROUNDOFF
  ) ** 2 + 3 * SUBNORMAL_ALLOWANCE
  subtraction_rounding = UNIT_ROUNDOFF * component_sum + SUBNORMAL_ALLOWANCE
  vector_error = provider["cartesian_norm_error_au"] + earth["cartesian_norm_error_au"] + 3 * subtraction_rounding
  computed_l1 = 3 * (component_sum + subtraction_rounding)
  distance_error = vector_error + LIBM_ALLOWANCE * computed_l1 + SUBNORMAL_ALLOWANCE
  product_magnitude, product_rounding = rounded(LIGHT_TIME * (target_radius + earth_radius + distance_error), F(0))
  subtraction_error = UNIT_ROUNDOFF * (max(abs(left), abs(right)) + product_magnitude) + SUBNORMAL_ALLOWANCE
  image_delay = product_magnitude + subtraction_error
  if image_delay > 1 or distance_error >= radial_lower:
    raise ValueError("The rounded Pluto map is not proved positive and inside its existing one-day past reserve")
  uniform_map = {
    "distance_error_upper_au": distance_error,
    "vector_error_upper_au": vector_error,
    "rounded_map_error_upper_days": LIGHT_TIME * distance_error + product_rounding + subtraction_error,
    "computed_vector_l1_upper_au": computed_l1,
    "rounded_image_delay_upper_days": image_delay,
  }
  source_radius = max(abs(emission_left - 2451545), abs(emission_right - 2451545)) / 36525
  raw_smooth = pluto_bounds(array_rows(pluto_source, "TERMS", "Term", 43, 9), source_radius, 1)
  raw_unit_speed = raw_smooth["longitude_rad"][1] + raw_smooth["latitude_rad"][1]
  # Fixed-observation precession is orthogonal; the inverse's max-degree contraction bounds both angles.
  speed = raw_smooth["radius_au"][1] + 2 * raw_smooth["radius_au"][0] * raw_unit_speed / (
    provider["precession_reference_axial_lower"] * (1 - provider["inverse_fk5"]["map_contraction"])
  )
  geometry = {
    "radial_lower_au": radial_lower,
    "axial_lower_au": axial_lower,
    "emission_speed_au_per_day": speed,
    "emission_map_contraction_upper": LIGHT_TIME * speed,
  }
  fixed = accepted_fixed_point_errors(uniform_map, geometry, left, right)
  angular = angular_allowance_from_errors(
    fixed["fixed_point_vector_error_upper_au"],
    LIBM_ALLOWANCE * computed_l1 + SUBNORMAL_ALLOWANCE,
    radial_lower,
    axial_lower,
  )
  beta_magnitude, _ = rounded((pi_interval()[1] / 2 + LIBM_ALLOWANCE) * DEG_PER_RAD_FP, F(0))
  longitude = B(F(360), degree_input_error(F(360), angular["longitude_circular_error_rad"]))
  latitude = B(beta_magnitude, degree_input_error(beta_magnitude, angular["latitude_error_rad"]))
  earth_longitude = B(F(360), degree_input_error(F(360), earth["longitude_circular_error_rad"]))
  centuries = provider["observation_centuries"]
  nutation = nutation_error_from_centuries(centuries, earth_source)
  corrections = correction_chain_from_centuries(
    centuries,
    longitude,
    latitude,
    earth_longitude,
    nutation["longitude_degrees"],
    geocentric_cosine_lower,
    correction_expressions,
  )
  final_longitude = B(F(360), degree_input_error(F(360), corrections["final_longitude_radians"].error))
  final_latitude = B(
    beta_magnitude + corrections["annual_latitude_degrees"].magnitude + corrections["fk5_latitude_degrees"].magnitude,
    degree_input_error(
      beta_magnitude + corrections["annual_latitude_degrees"].magnitude + corrections["fk5_latitude_degrees"].magnitude,
      corrections["final_latitude_radians"].error,
    ),
  )
  latitude_correction = (
    sum(
      corrections[name].magnitude + corrections[name].error
      for name in ("annual_latitude_degrees", "fk5_latitude_degrees")
    )
    * pi_interval()[1]
    / 180
  )
  epsilon = nutation["true_obliquity_degrees"]
  equatorial_axial_lower = rotated_axial_lower(
    geocentric_cosine_lower, latitude_correction, (epsilon.magnitude + epsilon.error) * pi_interval()[1] / 180
  )
  equatorial = equatorial_error(final_longitude, final_latitude, epsilon, equatorial_axial_lower)
  return {
    "analysis_interval": (left, right),
    "provider": provider,
    "uniform_map": uniform_map,
    "geometry": geometry,
    "geocentric_cosine_lower": geocentric_cosine_lower,
    "fixed_point": fixed,
    "geometric_angles": angular,
    "corrections": corrections,
    "true_obliquity_degrees": epsilon,
    "equatorial": equatorial,
  }
