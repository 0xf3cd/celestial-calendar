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

"""Whole-tier VSOP radial/axial separation on the complete short-retardation band."""

from fractions import Fraction as F

from .heliocentric_evaluation_error import time_argument_error
from .light_time_majorants import LIGHT_TIME, distance_bounds, precession_bounds
from .rational_intervals import Interval as I, sincos
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF, rounded
from .rounded_light_time_reach import outward_binary64


def source_radius_lower(tables: dict, radius: F) -> F:
  """Keep constant-frequency R0 terms and bound every remaining term by its amplitude."""
  if radius < 0:
    raise ValueError("The algebraic source radius must be nonnegative")
  constant = I.enclose(0)
  variation = F(0)
  for name, terms in tables.items():
    if name[0] != "R":
      continue
    power = int(name[1:])
    for amplitude, phase, frequency in terms:
      if power == 0 and frequency == 0:
        _, cosine = sincos(I.enclose(phase))
        constant += cosine * amplitude
      else:
        variation += abs(amplitude) * radius**power
  lower = (constant.lo - variation) / 100_000_000
  if lower <= 0:
    raise ValueError("The whole source tier has no positive radial triangle lower")
  return lower


def geometry_on_retardation_band(
  target: dict, earth: dict, target_tables: dict, earth_tables: dict, selection_radius: F
) -> dict:
  """Prove rounded-map reach and source separation before solving distance derivatives."""
  proof_radius = F(target["tau_radius_exact"])
  if selection_radius <= 0 or proof_radius != F(earth["tau_radius_exact"]) or proof_radius <= selection_radius:
    raise ValueError("The two source records need one common proof radius beyond their observation-selection radius")
  target_vector, earth_vector = (
    tuple(F(value["exact"]) for value in row["cartesian_norm_derivatives_au"]) for row in (target, earth)
  )
  target_upper, earth_upper = target_vector[0], earth_vector[0]
  target_lower, earth_lower = (source_radius_lower(tables, proof_radius) for tables in (target_tables, earth_tables))
  left, right = F(2451545) - selection_radius * 365250, F(2451545) + selection_radius * 365250
  computed_radii = sum(
    F(row["evaluation_at_exact_binary64_tau"]["R"]["computed_magnitude"]["exact"]) for row in (target, earth)
  )
  component = computed_radii * (1 + LIBM_ALLOWANCE) ** 2 * (1 + UNIT_ROUNDOFF) ** 2 + 3 * SUBNORMAL_ALLOWANCE
  _, subtraction_error = rounded(component, F(0))
  vector_l1 = 3 * (component + subtraction_error)
  distance_upper = (1 + LIBM_ALLOWANCE) * vector_l1 + SUBNORMAL_ALLOWANCE
  product, _ = rounded(LIGHT_TIME * distance_upper, F(0))
  _, subtraction_rounding = rounded(max(abs(left), abs(right)) + product, F(0))
  reserve = product + subtraction_rounding
  endpoints = outward_binary64(left - reserve, False), outward_binary64(right, True)
  if any(abs((time - 2451545) / 365250) + time_argument_error(time) > proof_radius for time in endpoints):
    raise ValueError("The whole rounded-retardation band leaves the proved raw source radius")

  from_century = proof_radius * 10
  between_century = reserve / 36525
  rotation_rate = precession_bounds((from_century, F(0)), (between_century, F(1, 36525)))[1]
  # R(emission, emission)=I; integrate the observation-time rotation derivative over the complete delay.
  tilt = rotation_rate * reserve
  target_beta, earth_beta = (F(row["coordinate_derivatives"]["B"][0]["exact"]) for row in (target, earth))
  target_xy = target_lower * max(F(0), 1 - target_beta**2 / 2) - target_upper * tilt
  earth_xy = earth_lower * max(F(0), 1 - earth_beta**2 / 2)
  radial_lower = max(earth_lower - target_upper, target_lower - earth_upper)
  axial_lower = max(earth_xy - target_upper, target_xy - earth_upper)
  if min(radial_lower, axial_lower) <= 0:
    raise ValueError("The whole source tier has no positive geocentric radial/axial triangle certificate")
  emission_rotation = precession_bounds((from_century, F(1, 36525)), (between_century, F(1, 36525)))[1]
  emission_speed = target_vector[1] + emission_rotation * target_upper
  contraction = LIGHT_TIME * emission_speed
  if contraction >= 1:
    raise ValueError("The whole source tier does not certify a contractive light-time map")
  distance, audit = distance_bounds(target_vector, earth_vector, selection_radius * 10, radial_lower)
  return {
    "selection_tau_radius": selection_radius,
    "source_tau_radius": proof_radius,
    "observation_interval": (left, right),
    "rounded_delay_reserve_days": reserve,
    "actual_emission_interval": endpoints,
    "computed_vector_l1_upper_au": vector_l1,
    "target_radial_lower_au": target_lower,
    "earth_radial_lower_au": earth_lower,
    "radial_lower_au": radial_lower,
    "axial_lower_au": axial_lower,
    "short_date_precession_norm_deviation_upper": tilt,
    "emission_map_contraction_upper": contraction,
    "distance_derivatives": distance,
    "implicit_derivative_audit": audit,
  }
