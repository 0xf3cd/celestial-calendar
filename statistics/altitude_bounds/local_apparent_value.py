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

"""Floating apparent-place errors on source-enclosed local light-time rectangles."""

from fractions import Fraction as F

from .apparent_correction_error import correction_chain_from_centuries, degree_input_error
from .floating_operation_bounds import FloatingBound as B
from .heliocentric_evaluation_error import DEG_PER_RAD_FP
from .local_vector_separation import separation_for_reserve
from .nutation_evaluation_error import nutation_error_from_centuries
from .poisson_majorants import pi_interval
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, rounded
from .retarded_angular_error import angular_allowance_from_errors
from .rounded_light_time_reach import rounded_reserve, uniform_map_error, uniform_raw_error
from .uniform_apparent_error import accepted_fixed_point_errors


def local_rounded_geometry(
  target: dict, earth: dict, centre: F, half_width: F, point: tuple, precession: str, tighten: bool = True
) -> dict:
  """Shrink only to an image proved on the preceding complete invariant rectangle."""
  radius = F(target["tau_radius_exact"])
  if radius != F(earth["tau_radius_exact"]):
    raise ValueError("Local source records must have the same proved radius")
  derivatives = [tuple(F(item["exact"]) for item in row["cartesian_norm_derivatives_au"]) for row in (target, earth)]
  initial = rounded_reserve(target, earth, centre, half_width, precession)
  reserve = initial["rounded_delay_reserve_days"]
  audit = []
  for _ in range(4 if tighten else 0):
    geometry = separation_for_reserve(point, *derivatives, centre, half_width, radius, reserve)
    distance_upper = min(
      derivatives[0][0] + derivatives[1][0], geometry["centre_radial_interval"][1] + geometry["tube_radius_au"]
    )
    image = uniform_map_error(target, earth, centre, half_width, reserve, precession, distance_upper)
    next_reserve = image["rounded_image_delay_upper_days"]
    if next_reserve > reserve:
      raise ValueError("The locally bounded rounded map leaves its previous invariant rectangle")
    audit.append({"input_reserve_days": reserve, "distance_upper_au": distance_upper, "image": image})
    if next_reserve == reserve:
      break
    reserve = next_reserve
  geometry = separation_for_reserve(point, *derivatives, centre, half_width, radius, reserve)
  distance_upper = (
    None
    if not tighten
    else min(derivatives[0][0] + derivatives[1][0], geometry["centre_radial_interval"][1] + geometry["tube_radius_au"])
  )
  image = uniform_map_error(target, earth, centre, half_width, reserve, precession, distance_upper)
  if image["rounded_image_delay_upper_days"] > reserve:
    raise ValueError("The final local rounded-map rectangle is not invariant")
  return {"initial": initial, "audit": audit, "reserve_days": reserve, "geometry": geometry, "uniform_map": image}


def local_apparent_errors(
  target: dict,
  earth: dict,
  centre: F,
  half_width: F,
  point: tuple,
  precession: str,
  earth_source: str,
  expressions: dict,
  tighten: bool = True,
) -> dict:
  """Transport local map errors through angular, correction and IAU1980 operation bounds."""
  local = local_rounded_geometry(target, earth, centre, half_width, point, precession, tighten)
  geometry, image = local["geometry"], local["uniform_map"]
  if geometry["axial_lower_au"] <= 0:
    raise ValueError("The complete local rectangle has no positive ecliptic axial certificate")
  left, right = centre - half_width, centre + half_width
  fixed = accepted_fixed_point_errors(image, geometry, left, right)
  angular = angular_allowance_from_errors(
    fixed["fixed_point_vector_error_upper_au"],
    LIBM_ALLOWANCE * image["computed_vector_l1_upper_au"] + SUBNORMAL_ALLOWANCE,
    geometry["radial_lower_au"],
    geometry["axial_lower_au"],
  )
  centuries = B(*rounded(max(abs(left - 2451545), abs(right - 2451545)), F(0))) / 36525
  earth_raw = uniform_raw_error(earth, left, right)
  beta_magnitude, _ = rounded((pi_interval()[1] / 2 + LIBM_ALLOWANCE) * DEG_PER_RAD_FP, F(0))
  longitude = B(F(360), degree_input_error(F(360), angular["longitude_circular_error_rad"]))
  latitude = B(beta_magnitude, degree_input_error(beta_magnitude, angular["latitude_error_rad"]))
  earth_longitude = B(F(360), degree_input_error(F(360), earth_raw["longitude_circular_error_rad"]))
  nutation = nutation_error_from_centuries(centuries, earth_source)
  radial_upper = geometry["centre_radial_interval"][1] + geometry["tube_radius_au"]
  cosine_lower = geometry["axial_lower_au"] / radial_upper
  corrections = correction_chain_from_centuries(
    centuries, longitude, latitude, earth_longitude, nutation["longitude_degrees"], cosine_lower, expressions
  )
  final_latitude = B(corrections["aberrated_latitude_degrees"].magnitude) + B(
    corrections["fk5_latitude_degrees"].magnitude
  )
  return {
    "local_map": local,
    "reserve": local["initial"],
    "geometry": geometry,
    "uniform_map": image,
    "fixed_point": fixed,
    "geometric_angles": angular,
    "reference_cosine_lower": cosine_lower,
    "correction_errors": {name: value.error for name, value in corrections.items()},
    "correction_magnitudes": {name: value.magnitude for name, value in corrections.items()},
    "latitude_degree_magnitude": final_latitude.magnitude,
    "true_obliquity_degree_error": nutation["true_obliquity_degrees"].error,
    "true_obliquity_degree_magnitude": nutation["true_obliquity_degrees"].magnitude,
  }
