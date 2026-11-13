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

"""Raw VSOP provider error through TT conversion, angle storage and Cartesian reconstruction."""

import math
from fractions import Fraction as F
from math import ceil

from .poisson_majorants import pi_interval
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF, rounded


TWO_PI_FP = F.from_float(2 * math.pi)
DEG_PER_RAD_FP = F.from_float(180 / math.pi)


def time_argument_error(jde_tt: F) -> F:
  """Enclose (JDE-J2000)/365250 at one exact binary64 JDE, including both source operations."""
  if F.from_float(float(jde_tt)) != jde_tt:
    raise ValueError("The source JDE observation must be an exact binary64 value")
  difference = abs(jde_tt - 2451545)
  difference_magnitude, subtraction_error = rounded(difference, F(0))
  _, total = rounded(difference_magnitude / 365250, subtraction_error / 365250)
  return total


def normalized_radian_error(magnitude: F, input_error: F) -> F:
  """Bound circular error after IEEE remainder, negative wrap and the source's zero canonicalization."""
  if magnitude < 0 or input_error < 0:
    raise ValueError("Require nonnegative magnitude and input error")
  pi_lo, pi_hi = pi_interval()
  period_error = max(abs(TWO_PI_FP - 2 * pi_lo), abs(TWO_PI_FP - 2 * pi_hi))
  quotient_bound = ceil(magnitude / TWO_PI_FP + F(1, 2)) + 2
  return input_error + quotient_bound * period_error + UNIT_ROUNDOFF * TWO_PI_FP + SUBNORMAL_ALLOWANCE


def angle_roundtrip_error(radian_magnitude: F) -> F:
  """Bound the source's radian-to-degree storage and subsequent degree-to-radian retrieval."""
  if radian_magnitude < 0:
    raise ValueError("Radian magnitude must be nonnegative")
  storage_error = UNIT_ROUNDOFF * radian_magnitude * DEG_PER_RAD_FP + SUBNORMAL_ALLOWANCE
  restored_magnitude = radian_magnitude + storage_error / DEG_PER_RAD_FP
  return storage_error / DEG_PER_RAD_FP + UNIT_ROUNDOFF * restored_magnitude + SUBNORMAL_ALLOWANCE


def provider_error(record: dict, jde_tt: F) -> dict:
  """Bound the raw heliocentric provider, excluding date-frame precession and later corrections."""
  tau_error = time_argument_error(jde_tt)
  if abs((jde_tt - 2451545) / 365250) + tau_error > F(record["tau_radius_exact"]):
    raise ValueError("The derivative/evaluation record does not cover the rounded source time")
  errors, magnitudes = {}, {}
  for family in "LBR":
    source = record["evaluation_at_exact_binary64_tau"][family]
    magnitudes[family] = F(source["computed_magnitude"]["exact"])
    derivative = F(record["coordinate_derivatives"][family][1]["exact"])
    errors[family] = F(source["absolute_error"]["exact"]) + derivative * tau_error * 365250
  errors["L"] = normalized_radian_error(magnitudes["L"], errors["L"]) + angle_roundtrip_error(TWO_PI_FP)
  errors["B"] += angle_roundtrip_error(magnitudes["B"])
  radial = magnitudes["R"]
  first_magnitude, first_rounding = rounded(radial * (1 + LIBM_ALLOWANCE), F(0))
  _, second_rounding = rounded(first_magnitude * (1 + LIBM_ALLOWANCE), first_rounding * (1 + LIBM_ALLOWANCE))
  xy_error = radial * (2 * LIBM_ALLOWANCE + LIBM_ALLOWANCE**2) + second_rounding
  z_error = radial * LIBM_ALLOWANCE + first_rounding
  vector_error = errors["R"] + radial * (errors["L"] + errors["B"]) + 2 * xy_error + z_error
  return {
    "longitude_circular_error_rad": errors["L"],
    "latitude_error_rad": errors["B"],
    "radius_error_au": errors["R"],
    "cartesian_norm_error_au": vector_error,
    "tau_error": tau_error,
  }
