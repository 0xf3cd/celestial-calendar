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

"""Solar Earth-VSOP, FK5, nutation and Meeus 25.11 variable-aberration value allowances."""

from fractions import Fraction as F

from .apparent_correction_error import degree_input_error, normalized_degrees, tangent_bound
from .clock_model_evaluation_error import integer_power_error
from .floating_operation_bounds import FloatingBound as B
from .heliocentric_evaluation_error import DEG_PER_RAD_FP
from .nutation_evaluation_error import nutation_error_from_centuries
from .poisson_majorants import array_rows
from .precession_evaluation_error import source_polynomial_bound
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, rounded
from .rounded_light_time_reach import uniform_raw_error


def solar_power_error(argument: B, power: int) -> B:
  """Bound solar powers 0..3, with exact power zero and a separate power-one allowance."""
  if power == 0:
    return B(F(1))
  if power == 1:
    allowance = LIBM_ALLOWANCE * (1 + argument.magnitude) + SUBNORMAL_ALLOWANCE
    return B(argument.magnitude + allowance, argument.error + allowance)
  if power in (2, 3):
    return integer_power_error(argument, power)
  raise ValueError("The retained solar daily-variation source uses only integer powers0 through3")


def solar_variation_error(millennia: B, earth_source: str) -> B:
  """Bound all 21 harmonics, including pow, phase conversion and sequential folds."""
  terms = array_rows(earth_source, "MEEUS_DAILY_VARIATION_TERMS", "DailyVariationTerm", 21, 4)
  total = B(F(0))
  for amplitude, phase, rate, power in terms:
    if power.denominator != 1:
      raise ValueError("A solar daily-variation power is not an integer")
    theta = B(abs(phase)) + B(abs(rate)) * millennia
    term = B(abs(amplitude)) * solar_power_error(millennia, int(power)) * theta.degrees_to_radians().sine_or_cosine()
    total = total + term
  return B.literal(3548.330) + total


def solar_evaluation_error(earth_record: dict, left: F, right: F, earth_source: str, fk5_drift_expression: str) -> dict:
  """Bound the complete solar interval using Earth VSOP and variable aberration."""
  if earth_record["planet"] != "earth" or left > right:
    raise ValueError("Require an Earth source/evaluation record and ordered TT interval")
  raw = uniform_raw_error(earth_record, left, right)
  time_magnitude = max(abs(left - 2451545), abs(right - 2451545))
  difference = B(*rounded(time_magnitude, F(0)))
  centuries, millennia = difference / 36525, difference / 365250
  earth_longitude = B(F(360), degree_input_error(F(360), raw["longitude_circular_error_rad"]))
  solar_longitude = normalized_degrees(earth_longitude + B.literal(180))
  raw_beta_magnitude = F(earth_record["evaluation_at_exact_binary64_tau"]["B"]["computed_magnitude"]["exact"])
  stored_beta_magnitude = rounded(raw_beta_magnitude * DEG_PER_RAD_FP, F(0))[0]
  solar_latitude = B(stored_beta_magnitude, degree_input_error(stored_beta_magnitude, raw["latitude_error_rad"]))
  beta_reference_upper = F(earth_record["coordinate_derivatives"]["B"][0]["exact"])
  cosine_lower = max(F(0), 1 - beta_reference_upper**2 / 2)
  if cosine_lower <= 0:
    raise ValueError("The full solar latitude source box has no positive cosine certificate")

  drift = source_polynomial_bound(fk5_drift_expression, {"jc": centuries})
  phase = (solar_longitude - drift).degrees_to_radians().sine_or_cosine()
  tangent = tangent_bound(solar_latitude.degrees_to_radians(), cosine_lower)
  fk5_longitude = (B.literal(-0.09033) + B.literal(0.03916) * (phase + phase) * tangent) / 3600
  fk5_latitude = B.literal(0.03916) * (phase - phase) / 3600
  nutation = nutation_error_from_centuries(centuries, earth_source)
  distance = B(
    F(earth_record["evaluation_at_exact_binary64_tau"]["R"]["computed_magnitude"]["exact"]), raw["radius_error_au"]
  )
  variation = solar_variation_error(millennia, earth_source)
  aberration = (B.literal(0.0057755183) * distance * variation) / 3600
  final_longitude = normalized_degrees(solar_longitude + fk5_longitude + nutation["longitude_degrees"] - aberration)
  final_latitude = solar_latitude + fk5_latitude
  return {
    "geometric_longitude_degrees": solar_longitude,
    "geometric_latitude_degrees": solar_latitude,
    "fk5_longitude_degrees": fk5_longitude,
    "fk5_latitude_degrees": fk5_latitude,
    "daily_variation_arcsec_per_day": variation,
    "aberration_degrees": aberration,
    "final_longitude_degrees": final_longitude,
    "final_latitude_degrees": final_latitude,
    "distance_au": distance,
    "true_obliquity_degrees": nutation["true_obliquity_degrees"],
    "geometric_cosine_lower": cosine_lower,
  }
