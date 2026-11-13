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

"""Meeus 13.3/13.4 arithmetic, angle storage and reconstructed equatorial direction errors."""

from fractions import Fraction as F

from .floating_operation_bounds import FloatingBound as B
from .heliocentric_evaluation_error import angle_roundtrip_error
from .poisson_majorants import pi_interval
from .precession_evaluation_error import degree_normalized_storage_error
from .rational_intervals import Interval as I
from .raw_evaluation_error import LIBM_ALLOWANCE, rounded
from .spherical_coordinate_majorants import dyadic_upper
from .spherical_reconstruction_error import reconstruction_error


def equatorial_error(
  longitude_degrees: B, latitude_degrees: B, obliquity_degrees: B, reference_axial_lower: F = F(0)
) -> dict:
  """Bound stored RA/Dec after unit-direction reconstruction, including clamped asin."""
  if not 0 <= reference_axial_lower <= 1:
    raise ValueError("The optional smooth equatorial xy-norm lower must lie in[0,1]")
  longitude, latitude, obliquity = tuple(
    value.degrees_to_radians() for value in (longitude_degrees, latitude_degrees, obliquity_degrees)
  )
  longitude_wave, latitude_wave, obliquity_wave = tuple(
    value.sine_or_cosine() for value in (longitude, latitude, obliquity)
  )
  x = longitude_wave * latitude_wave
  y = (longitude_wave * obliquity_wave * latitude_wave) - (latitude_wave * obliquity_wave)
  z = (latitude_wave * obliquity_wave) + (latitude_wave * obliquity_wave * longitude_wave)

  angular_error = 2 * LIBM_ALLOWANCE
  angular_error += degree_normalized_storage_error(pi_interval()[1] + LIBM_ALLOWANCE)
  angular_error += angle_roundtrip_error(pi_interval()[1] / 2 + LIBM_ALLOWANCE)
  stored_direction = reconstruction_error(x.error + y.error, z.error, angular_error, reference_axial_lower)
  first_magnitude, first_rounding = rounded(1 + LIBM_ALLOWANCE, F(0))
  _, second_rounding = rounded(first_magnitude * (1 + LIBM_ALLOWANCE), first_rounding * (1 + LIBM_ALLOWANCE))
  xy_error = 2 * LIBM_ALLOWANCE + LIBM_ALLOWANCE**2 + second_rounding
  reconstructed = stored_direction + 2 * xy_error + LIBM_ALLOWANCE
  return {
    "intermediate_xy_error": dyadic_upper((x.error + y.error,))[0],
    "intermediate_z_error": dyadic_upper((z.error,))[0],
    "stored_angle_direction_error": dyadic_upper((stored_direction,))[0],
    "reconstructed_direction_error": dyadic_upper((reconstructed,))[0],
    "reference_axial_lower": reference_axial_lower,
  }


def rotated_axial_lower(ecliptic_cosine_lower: F, latitude_correction_upper_rad: F, obliquity_upper_rad: F) -> F:
  """Bound equatorial xy norm from whole-region latitude and obliquity enclosures."""
  if not 0 <= ecliptic_cosine_lower <= 1 or min(latitude_correction_upper_rad, obliquity_upper_rad) < 0:
    raise ValueError("Require a unit-range ecliptic cosine lower and nonnegative angle magnitude uppers")
  apparent_cosine_lower = max(F(0), ecliptic_cosine_lower - latitude_correction_upper_rad)
  sine_upper = I.enclose(1 - apparent_cosine_lower**2).sqrt().hi
  cosine_obliquity_lower = max(F(0), 1 - obliquity_upper_rad**2 / 2)
  return max(F(0), apparent_cosine_lower * cosine_obliquity_lower - sine_upper * obliquity_upper_rad)
