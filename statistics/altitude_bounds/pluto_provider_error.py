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

"""Emission/observation-box errors for Pluto's raw, precession and inverse-FK5 route."""

import re
from fractions import Fraction as F

from .apparent_correction_error import normalized_degrees
from .floating_operation_bounds import FloatingBound as B
from .heliocentric_evaluation_error import DEG_PER_RAD_FP
from .inverse_fk5_error import inverse_fk5_error
from .pluto_raw_evaluation_error import RAW_END_JDE_TT, RAW_START_JDE_TT, pluto_raw_evaluation_error
from .poisson_majorants import pi_interval
from .precession_evaluation_error import source_polynomial_bound
from .rational_intervals import Interval as I
from .raw_evaluation_error import LIBM_ALLOWANCE, rounded


def stored_degrees(argument: B) -> B:
  """Charge rad_to_deg multiplication against the exact radian-to-degree conversion."""
  pi_lower, pi_upper = pi_interval()
  factor_error = max(abs(DEG_PER_RAD_FP - 180 / pi_lower), abs(DEG_PER_RAD_FP - 180 / pi_upper))
  magnitude, rounding = rounded(argument.magnitude * DEG_PER_RAD_FP, F(0))
  return B(magnitude, argument.error * 180 / pi_lower + argument.magnitude * factor_error + rounding)


def pluto_provider_error(
  emission_left: F,
  emission_right: F,
  observation_left: F,
  observation_right: F,
  pluto_source: str,
  precession_source: str,
  drift_expression: str,
) -> dict:
  """Bound every returned provider result over the full admitted rectangle."""
  if not RAW_START_JDE_TT + 1 <= observation_left <= observation_right < RAW_END_JDE_TT:
    raise ValueError("The observation box must remain in Pluto's existing half-open apparent domain")
  raw = pluto_raw_evaluation_error(emission_left, emission_right, pluto_source)
  maximum_difference = max(abs(observation_left - 2451545), abs(observation_right - 2451545))
  centuries = B(*rounded(maximum_difference, F(0))) / 36525
  expressions = {
    name: re.search(r"const double " + name + r"\s*=\s*(.*?);", precession_source, re.DOTALL)[1]
    for name in ("η_arcsec", "Π_var_arcsec", "p_arcsec")
  }
  variables = {"t0": B(F(0)), "t": centuries}
  eta_degrees = source_polynomial_bound(expressions["η_arcsec"], variables) / 3600
  pi_degrees = B.literal(174.876384) + source_polynomial_bound(expressions["Π_var_arcsec"], variables) / 3600
  p_degrees = source_polynomial_bound(expressions["p_arcsec"], variables) / 3600
  eta, pi, p = (value.degrees_to_radians() for value in (eta_degrees, pi_degrees, p_degrees))
  longitude, latitude = (raw[name].degrees_to_radians() for name in ("longitude_degrees", "latitude_degrees"))
  phase = pi - longitude
  phase_wave, beta_wave, eta_wave = (value.sine_or_cosine() for value in (phase, latitude, eta))
  a = eta_wave * beta_wave * phase_wave - eta_wave * beta_wave
  b = beta_wave * phase_wave
  c = eta_wave * beta_wave + eta_wave * beta_wave * phase_wave

  # Latitude changes by at most |eta|; the two longitude rotations preserve latitude.
  reference_latitude_cap_degrees = (
    raw["latitude_degrees"].magnitude + raw["latitude_degrees"].error + eta_degrees.magnitude + eta_degrees.error
  )
  reference_latitude_cap = reference_latitude_cap_degrees * pi_interval()[1] / 180
  if reference_latitude_cap + c.error >= 1:
    raise ValueError("The complete precession asin segment has no positive cosine certificate")
  axial_lower = 1 - reference_latitude_cap**2 / 2
  xy_error = a.error + b.error
  if xy_error >= axial_lower:
    raise ValueError("The complete precession atan2 segment is not separated from its axis")
  asin_cosine_lower = I.enclose(1 - (reference_latitude_cap + c.error) ** 2).sqrt().lo
  longitude_transport = xy_error / (axial_lower - xy_error)
  latitude_transport = c.error / asin_cosine_lower
  phase_sum = p + pi - B(pi_interval()[1] + LIBM_ALLOWANCE, longitude_transport + LIBM_ALLOWANCE)
  precessed_longitude = normalized_degrees(stored_degrees(phase_sum))
  beta_radians = B(pi_interval()[1] / 2 + LIBM_ALLOWANCE, latitude_transport + LIBM_ALLOWANCE)
  precessed_latitude_error = stored_degrees(beta_radians).error
  precessed_latitude = B(reference_latitude_cap_degrees + precessed_latitude_error, precessed_latitude_error)
  inverse = inverse_fk5_error(centuries, precessed_longitude.error, precessed_latitude, F(60), drift_expression)
  error_degrees = inverse["returned_updated_error_upper_degrees"]
  correction_latitude = inverse["source_correction_latitude"]
  dynamic_latitude_cap_degrees = (
    reference_latitude_cap_degrees + correction_latitude.magnitude + correction_latitude.error
  )
  dynamic_longitude = B(F(360), error_degrees).degrees_to_radians()
  dynamic_latitude = B(dynamic_latitude_cap_degrees + error_degrees, error_degrees).degrees_to_radians()
  wave_error = dynamic_longitude.error + dynamic_latitude.error
  radius = raw["radius_au"]
  first_magnitude, first_rounding = rounded(radius.magnitude * (1 + LIBM_ALLOWANCE), F(0))
  _, second_rounding = rounded(first_magnitude * (1 + LIBM_ALLOWANCE), first_rounding * (1 + LIBM_ALLOWANCE))
  xy_rounding = radius.magnitude * (2 * LIBM_ALLOWANCE + LIBM_ALLOWANCE**2) + second_rounding
  z_rounding = radius.magnitude * LIBM_ALLOWANCE + first_rounding
  vector_error = radius.error + (radius.magnitude + radius.error) * wave_error + 2 * xy_rounding + z_rounding
  dynamic_latitude_cap = dynamic_latitude_cap_degrees * pi_interval()[1] / 180
  return {
    "emission_interval": (emission_left, emission_right),
    "observation_interval": (observation_left, observation_right),
    "raw": raw,
    "observation_centuries": centuries,
    "precession_eta_degrees": eta_degrees,
    "precession_xy_error": xy_error,
    "precession_z_error": c.error,
    "precession_reference_axial_lower": axial_lower,
    "precession_asin_segment_cosine_lower": asin_cosine_lower,
    "precessed_reference_latitude_cap_degrees": reference_latitude_cap_degrees,
    "precessed_longitude_degrees": precessed_longitude,
    "precessed_latitude_degrees": precessed_latitude,
    "inverse_fk5": inverse,
    "dynamic_reference_latitude_cap_degrees": dynamic_latitude_cap_degrees,
    "dynamic_reference_cosine_lower": 1 - dynamic_latitude_cap**2 / 2,
    "longitude_circular_error_rad": dynamic_longitude.error,
    "latitude_error_rad": dynamic_latitude.error,
    "radius_error_au": radius.error,
    "cartesian_norm_error_au": B(F(0), vector_error).error,
  }
