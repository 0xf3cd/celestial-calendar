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

"""Uniform rounded-map reserves for VSOP light-time iteration, before local separation refinement."""

import math
from fractions import Fraction as F

from .floating_operation_bounds import FloatingBound as B
from .heliocentric_evaluation_error import provider_error
from .light_time_majorants import LIGHT_TIME
from .precession_evaluation_error import precession_error_from_centuries
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF, rounded
from .spherical_coordinate_majorants import dyadic_upper


def outward_binary64(value: F, upward: bool) -> F:
  """Enclose a finite rational endpoint by an actual binary64 value without narrowing its interval."""
  result = float(value)
  if not math.isfinite(result):
    raise ValueError("A source-domain endpoint cannot be enclosed by finite binary64")
  if (upward and F(result) < value) or (not upward and F(result) > value):
    result = math.nextafter(result, math.inf if upward else -math.inf)
  return F(result)


def uniform_raw_error(record: dict, left: F, right: F) -> dict:
  """Use the maximum TT-to-tau subtraction magnitude over all represented times in the interval."""
  if left > right:
    raise ValueError("Source interval endpoints are reversed")
  endpoints = (outward_binary64(left, False), outward_binary64(right, True))
  extreme = max(endpoints, key=lambda value: abs(value - 2451545))
  return provider_error(record, extreme)


def uniform_map_error(
  target_record: dict,
  earth_record: dict,
  centre: F,
  half_width: F,
  delay_reserve: F,
  source: str,
  proved_distance_upper=None,
) -> dict:
  """Bound the whole rounded-map rectangle; a supplied distance upper must already hold throughout it."""
  if half_width < 0 or delay_reserve < 0:
    raise ValueError("Cell half-width and the proposed delay reserve must be nonnegative")
  observation_left, observation_right = centre - half_width, centre + half_width
  emission_left, emission_right = observation_left - delay_reserve, observation_right
  target_raw = uniform_raw_error(target_record, emission_left, emission_right)
  earth_raw = uniform_raw_error(earth_record, observation_left, observation_right)
  from_magnitude = max(abs(emission_left - 2451545), abs(emission_right - 2451545))
  between_magnitude = 2 * half_width + delay_reserve
  from_centuries = B(*rounded(from_magnitude, F(0))) / 36525
  between_centuries = B(*rounded(between_magnitude, F(0))) / 36525
  target = precession_error_from_centuries(target_record, target_raw, from_centuries, between_centuries, source)
  radii = [
    F(row["evaluation_at_exact_binary64_tau"]["R"]["computed_magnitude"]["exact"])
    for row in (target_record, earth_record)
  ]
  component_sum = sum(radii) * (1 + LIBM_ALLOWANCE) ** 2 * (1 + UNIT_ROUNDOFF) ** 2 + 3 * SUBNORMAL_ALLOWANCE
  subtraction_rounding = UNIT_ROUNDOFF * component_sum + SUBNORMAL_ALLOWANCE
  vector_error = target["cartesian_norm_error_au"] + earth_raw["cartesian_norm_error_au"] + 3 * subtraction_rounding
  computed_l1_upper = 3 * (component_sum + subtraction_rounding)
  distance_error = vector_error + LIBM_ALLOWANCE * computed_l1_upper + SUBNORMAL_ALLOWANCE
  reference_radius_upper = sum(
    F(row["cartesian_norm_derivatives_au"][0]["exact"]) for row in (target_record, earth_record)
  )
  if proved_distance_upper is not None:
    if not 0 <= proved_distance_upper <= reference_radius_upper:
      raise ValueError(
        "A local distance upper must be proved on this whole rectangle and not exceed the source-radius bound"
      )
    reference_radius_upper = proved_distance_upper
  product_magnitude, product_rounding = rounded(LIGHT_TIME * (reference_radius_upper + distance_error), F(0))
  observation_magnitude = max(abs(observation_left), abs(observation_right))
  subtraction_map_error = UNIT_ROUNDOFF * (observation_magnitude + product_magnitude) + SUBNORMAL_ALLOWANCE
  image_delay_upper = product_magnitude + subtraction_map_error
  map_error = LIGHT_TIME * distance_error + product_rounding + subtraction_map_error
  return {
    "distance_error_upper_au": dyadic_upper((distance_error,))[0],
    "vector_error_upper_au": dyadic_upper((vector_error,))[0],
    "rounded_map_error_upper_days": dyadic_upper((map_error,))[0],
    "computed_vector_l1_upper_au": dyadic_upper((computed_l1_upper,))[0],
    "rounded_image_delay_upper_days": dyadic_upper((image_delay_upper,))[0],
    "precessed_vector_error_upper_au": target["cartesian_norm_error_au"],
    "emission_interval": (emission_left, emission_right),
    "observation_interval": (observation_left, observation_right),
  }


def rounded_reserve(target_record: dict, earth_record: dict, centre: F, half_width: F, source: str) -> dict:
  """Verify a rounded-map invariant, then retain only proof-backed smaller dyadic reserves."""
  source_radius = sum(F(row["cartesian_norm_derivatives_au"][0]["exact"]) for row in (target_record, earth_record))
  exact_delay = LIGHT_TIME * source_radius
  reserve = dyadic_upper((1 + 2 * exact_delay,))[0]
  audit = []
  for _attempt in range(4):
    proof = uniform_map_error(target_record, earth_record, centre, half_width, reserve, source)
    image = proof["rounded_image_delay_upper_days"]
    if image > reserve:
      raise ValueError("The proposed rectangle does not certify the rounded light-time map invariant")
    audit.append(
      {
        "input_reserve_days": reserve,
        "image_reserve_days": image,
        "distance_error_upper_au": proof["distance_error_upper_au"],
      }
    )
    if image == reserve:
      break
    reserve = image
  # The last proven image is invariant as a subset of its input rectangle; no sampled maxima are used.
  return {"exact_source_delay_days": exact_delay, "rounded_delay_reserve_days": reserve, "audit": audit}
