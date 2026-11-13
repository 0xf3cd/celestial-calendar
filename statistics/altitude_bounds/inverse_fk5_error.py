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

"""Source-map and accepted-updated-iterate errors for Pluto's inverse-FK5 solve."""

from fractions import Fraction as F

from .apparent_correction_error import normalized_degrees, tangent_bound
from .floating_operation_bounds import FloatingBound as B
from .poisson_majorants import pi_interval
from .precession_evaluation_error import source_polynomial_bound
from .raw_evaluation_error import SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF


def fk5_map_lipschitz(cosine_lower: F) -> F:
  """Bound the correction Jacobian in max-degree norm over a separated latitude strip."""
  if not 0 < cosine_lower <= 1:
    raise ValueError("Require a positive unit-range cosine lower throughout the inverse-map latitude strip")
  k = F(0.03916) / 3600 * pi_interval()[1] / 180
  return max(2 * k * (1 / cosine_lower + 1 / cosine_lower**2), 2 * k)


def accepted_updated_error(map_error: F, contraction: F, accepted_step: F) -> F:
  """Bound the updated iterate from map error and the old-to-updated gap in the same max norm."""
  if min(map_error, accepted_step) < 0 or not 0 <= contraction < 1:
    raise ValueError("Require nonnegative allowances and a strict source-map contraction")
  return (map_error + contraction * accepted_step) / (1 - contraction)


def inverse_fk5_error(
  centuries: B, input_longitude_error_deg: F, input_latitude: B, latitude_cap_deg: F, drift_expression: str
) -> dict:
  """Require an actual/reference invariant strip and bound every accepted updated result."""
  if input_longitude_error_deg < 0 or not 0 < latitude_cap_deg < 90:
    raise ValueError("Require a nonnegative circular input error and a positive open-pole latitude proof cap")
  latitude_upper = latitude_cap_deg * pi_interval()[1] / 180
  cosine_lower = 1 - latitude_upper**2 / 2
  if cosine_lower <= 0:
    raise ValueError("The proposed inverse-FK5 strip has no positive analytic cosine certificate")
  longitude = B(F(360))
  latitude = B(latitude_cap_deg)
  drift = source_polynomial_bound(drift_expression, {"jc": centuries})
  wave = (longitude - drift).degrees_to_radians().sine_or_cosine()
  tangent = tangent_bound(latitude.degrees_to_radians(), cosine_lower)
  correction_longitude = (B.literal(-0.09033) + B.literal(0.03916) * (wave + wave) * tangent) / 3600
  correction_latitude = B.literal(0.03916) * (wave - wave) / 3600
  updated_longitude = normalized_degrees(B(F(360), input_longitude_error_deg) - correction_longitude)
  updated_latitude = input_latitude - correction_latitude
  reference_latitude_upper = (
    input_latitude.magnitude + input_latitude.error + correction_latitude.magnitude + correction_latitude.error
  )
  if max(updated_latitude.magnitude, reference_latitude_upper) > latitude_cap_deg:
    raise ValueError("The proposed inverse-FK5 latitude strip is not invariant for both actual and reference maps")
  contraction = fk5_map_lipschitz(cosine_lower)
  accepted_step = F(1e-15) + UNIT_ROUNDOFF * max(F(720), 2 * latitude_cap_deg) + SUBNORMAL_ALLOWANCE
  map_error = max(updated_longitude.error, updated_latitude.error)
  return {
    "latitude_cap_degrees": latitude_cap_deg,
    "reference_cosine_lower": cosine_lower,
    "map_contraction": contraction,
    "source_correction_longitude": correction_longitude,
    "source_correction_latitude": correction_latitude,
    "updated_longitude": updated_longitude,
    "updated_latitude": updated_latitude,
    "actual_accepted_step_upper_degrees": accepted_step,
    "map_error_upper_degrees": map_error,
    "returned_updated_error_upper_degrees": accepted_updated_error(map_error, contraction, accepted_step),
  }
