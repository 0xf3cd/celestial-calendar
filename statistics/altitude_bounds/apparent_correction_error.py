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

"""Source-operation allowances for planetary aberration, FK5 and stored-degree additions."""

from fractions import Fraction as F

from .floating_operation_bounds import FloatingBound as B
from .heliocentric_evaluation_error import DEG_PER_RAD_FP
from .poisson_majorants import pi_interval
from .precession_evaluation_error import source_polynomial_bound
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF, rounded


def normalized_degrees(value: B) -> B:
  """Keep circular error through IEEE degree remainder and its optional negative wrap."""
  return B(F(360), value.error + UNIT_ROUNDOFF * 360 + SUBNORMAL_ALLOWANCE)


def positive_quotient(numerator: B, denominator: B, reference_lower: F) -> B:
  """Divide with a proved positive reference denominator and an allowance covering its actual evaluation."""
  if reference_lower <= denominator.error:
    raise ValueError("The rounded denominator is not certified positive")
  observed_lower = reference_lower - denominator.error
  magnitude, rounding = rounded(numerator.magnitude / observed_lower, F(0))
  propagated = numerator.error / observed_lower
  propagated += (numerator.magnitude + numerator.error) * denominator.error / (reference_lower * observed_lower)
  return B(magnitude, propagated + rounding)


def tangent_bound(argument: B, reference_cosine_lower: F) -> B:
  """Use a |tan_fl-tan|<=2^-48*(1+|tan|)+eta premise on the separated argument segment."""
  segment_cosine_lower = reference_cosine_lower - argument.error
  if segment_cosine_lower <= 0 or reference_cosine_lower > 1:
    raise ValueError("The complete tangent argument segment is not certified away from a pole")
  magnitude = 1 / segment_cosine_lower
  elementary_error = LIBM_ALLOWANCE * (1 + magnitude) + SUBNORMAL_ALLOWANCE
  return B(magnitude + elementary_error, argument.error / segment_cosine_lower**2 + elementary_error)


def degree_input_error(degrees: F, circular_radian_error: F) -> F:
  """Transport a radian allowance back to stored degrees; abs(degrees) may be a uniform magnitude upper."""
  if circular_radian_error < 0:
    raise ValueError("A radian observation allowance must be nonnegative")
  pi_lower, pi_upper = pi_interval()
  conversion_error = max(abs(DEG_PER_RAD_FP - 180 / pi_lower), abs(DEG_PER_RAD_FP - 180 / pi_upper))
  retrieval_error = UNIT_ROUNDOFF * abs(degrees / DEG_PER_RAD_FP) + SUBNORMAL_ALLOWANCE
  reference_radian_magnitude = abs(degrees / DEG_PER_RAD_FP) + retrieval_error + circular_radian_error
  return DEG_PER_RAD_FP * (circular_radian_error + retrieval_error) + conversion_error * reference_radian_magnitude


def correction_chain_from_centuries(
  centuries: B,
  longitude_degrees: B,
  latitude_degrees: B,
  earth_longitude_degrees: B,
  nutation_degrees: B,
  reference_cosine_lower: F,
  source_expressions: dict,
) -> dict:
  """Compose century/angle enclosures through the stored-degree source operations."""
  if not 0 < reference_cosine_lower <= 1:
    raise ValueError("Require a proved positive reference cos(beta)")
  variables = {"jc": centuries}
  eccentricity = source_polynomial_bound(source_expressions["eccentricity"], variables)
  perihelion = source_polynomial_bound(source_expressions["perihelion_degrees"], variables).degrees_to_radians()
  solar_longitude = normalized_degrees(earth_longitude_degrees + B.literal(180)).degrees_to_radians()
  longitude = longitude_degrees.degrees_to_radians()
  latitude = latitude_degrees.degrees_to_radians()
  solar_difference = (solar_longitude - longitude).sine_or_cosine()
  perihelion_difference = (perihelion - longitude).sine_or_cosine()
  beta_wave = latitude.sine_or_cosine()
  annual_longitude = (
    positive_quotient(
      B.literal(20.49552) * (-solar_difference + eccentricity * perihelion_difference),
      beta_wave,
      reference_cosine_lower,
    )
    / 3600
  )
  annual_latitude = B.literal(-20.49552) * beta_wave * (solar_difference - eccentricity * perihelion_difference) / 3600

  aberrated_longitude = longitude_degrees + annual_longitude
  aberrated_latitude = latitude_degrees + annual_latitude
  aberrated_cosine_lower = (
    reference_cosine_lower - (annual_latitude.magnitude + annual_latitude.error) * pi_interval()[1] / 180
  )
  if aberrated_cosine_lower <= 0:
    raise ValueError("The aberrated reference latitude has no positive cosine certificate")
  drift_degrees = source_polynomial_bound(source_expressions["fk5_drift_degrees"], variables)
  phase = (aberrated_longitude - drift_degrees).degrees_to_radians()
  phase_wave = phase.sine_or_cosine()
  tangent = tangent_bound(aberrated_latitude.degrees_to_radians(), aberrated_cosine_lower)
  fk5_longitude = (B.literal(-0.09033) + B.literal(0.03916) * (phase_wave + phase_wave) * tangent) / 3600
  fk5_latitude = B.literal(0.03916) * (phase_wave - phase_wave) / 3600

  final_longitude = normalized_degrees(aberrated_longitude + fk5_longitude + nutation_degrees)
  final_latitude = aberrated_latitude + fk5_latitude
  return {
    "annual_longitude_degrees": annual_longitude,
    "annual_latitude_degrees": annual_latitude,
    "aberrated_longitude_degrees": aberrated_longitude,
    "aberrated_latitude_degrees": aberrated_latitude,
    "fk5_longitude_degrees": fk5_longitude,
    "fk5_latitude_degrees": fk5_latitude,
    "final_longitude_radians": final_longitude.degrees_to_radians(),
    "final_latitude_radians": final_latitude.degrees_to_radians(),
  }
