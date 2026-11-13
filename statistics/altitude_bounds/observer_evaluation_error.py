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

"""Local-geometry allowances with explicit direction, sidereal and distance premises."""

from fractions import Fraction as F

from .apparent_correction_error import positive_quotient
from .floating_operation_bounds import FloatingBound as B
from .rational_intervals import Interval as I
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE


def positive_sqrt(argument: B, reference_lower: F) -> B:
  """Bound sqrt with a positive reference radicand lower and relative libm allowance."""
  if reference_lower <= argument.error:
    raise ValueError("The rounded square-root argument is not certified positive")
  reference_root_lower = I.enclose(reference_lower).sqrt().lo
  observed_root_lower = I.enclose(reference_lower - argument.error).sqrt().lo
  magnitude = I.enclose(argument.magnitude).sqrt().hi
  elementary_error = LIBM_ALLOWANCE * magnitude + SUBNORMAL_ALLOWANCE
  propagated = argument.error / (reference_root_lower + observed_root_lower)
  return B(magnitude + elementary_error, propagated + elementary_error)


def site_error(latitude_degrees: B, elevation_km: B, equatorial_radius_km: F, polar_ratio_squared: F) -> dict:
  """Follow WGS84 normal/height operations using exact stored constants."""
  if equatorial_radius_km <= 0 or not 0 < polar_ratio_squared <= 1:
    raise ValueError("Require positive source radius and a unit-range polar-ratio square")
  wave = latitude_degrees.degrees_to_radians().sine_or_cosine()
  radicand = B.literal(1) - ((B.literal(1) - B(polar_ratio_squared)) * wave * wave)
  root = positive_sqrt(radicand, polar_ratio_squared)
  normal = positive_quotient(B.literal(1), root, I.enclose(polar_ratio_squared).sqrt().lo)
  height = elevation_km / equatorial_radius_km
  equatorial = (normal + height) * wave
  polar = ((B(polar_ratio_squared) * normal) + height) * wave
  return {"equatorial": equatorial, "polar": polar, "latitude_wave": wave}


def local_projection_error(
  latitude_degrees: B,
  elevation_km: B,
  stored_direction_error: F,
  sidereal_degree_error: F,
  distance_au: B,
  reference_distance_lower_au: F,
  equatorial_radius_km: F,
  polar_ratio_squared: F,
  au_km_scale: F,
) -> dict:
  """Bound local A/B/C and horizontal projection without requiring a positive norm lower."""
  if min(stored_direction_error, sidereal_degree_error) < 0 or au_km_scale <= 0:
    raise ValueError("Require nonnegative direction/sidereal errors and a positive source AU scale")
  site = site_error(latitude_degrees, elevation_km, equatorial_radius_km, polar_ratio_squared)
  radius_au = B(equatorial_radius_km) / au_km_scale
  parallax = positive_quotient(radius_au, distance_au, reference_distance_lower_au)
  site_norm_upper = sum(site[name].magnitude + site[name].error for name in ("equatorial", "polar"))
  reference_q_lower = 1 - (parallax.magnitude + parallax.error) * site_norm_upper

  # H subtracts stored degrees; the direction allowance already includes RA radian retrieval.
  hour_angle = (B(F(360), sidereal_degree_error) - B(F(360))).degrees_to_radians()
  hour_error = hour_angle.error + B(F(360)).degrees_to_radians().error
  wave = B(1 + LIBM_ALLOWANCE, LIBM_ALLOWANCE)
  product = wave * wave
  rotated_error = stored_direction_error + hour_error
  a = B(product.magnitude, product.error + rotated_error)
  b = B(product.magnitude, product.error + rotated_error) - site["equatorial"] * parallax
  c = B(wave.magnitude, wave.error + stored_direction_error) - site["polar"] * parallax
  latitude_wave = site["latitude_wave"]
  north = (c * latitude_wave) - (b * latitude_wave)
  up = (c * latitude_wave) + (b * latitude_wave)
  return {
    "A": a,
    "B": b,
    "C": c,
    "east": a,
    "north": north,
    "up": up,
    "reference_q_lower": reference_q_lower,
    "hour_angle_error_rad": hour_error,
    "site": site,
    "parallax": parallax,
  }


def local_geometry_error(
  latitude_degrees: B,
  elevation_km: B,
  stored_direction_error: F,
  sidereal_degree_error: F,
  distance_au: B,
  reference_distance_lower_au: F,
  equatorial_radius_km: F,
  polar_ratio_squared: F,
  au_km_scale: F,
) -> dict:
  """Bound full local geometry with positive separation and retrieved RA/Dec direction error."""
  projected = local_projection_error(
    latitude_degrees,
    elevation_km,
    stored_direction_error,
    sidereal_degree_error,
    distance_au,
    reference_distance_lower_au,
    equatorial_radius_km,
    polar_ratio_squared,
    au_km_scale,
  )
  if projected["reference_q_lower"] <= 0:
    raise ValueError(
      "The observer triangle bound does not certify positive local separation; refine or fail numerically"
    )
  a, b, c = (projected[name] for name in ("A", "B", "C"))
  q_elementary = LIBM_ALLOWANCE * (a.magnitude + b.magnitude + c.magnitude) + SUBNORMAL_ALLOWANCE
  q = B(
    (1 + LIBM_ALLOWANCE) * (a.magnitude + b.magnitude + c.magnitude) + SUBNORMAL_ALLOWANCE,
    a.error + b.error + c.error + q_elementary,
  )
  return {
    "A": a,
    "B": b,
    "C": c,
    "east": a,
    "north": projected["north"],
    "up": projected["up"],
    "q": q,
    "reference_q_lower": projected["reference_q_lower"],
    "hour_angle_error_rad": projected["hour_angle_error_rad"],
    "site": projected["site"],
    "parallax": projected["parallax"],
  }
