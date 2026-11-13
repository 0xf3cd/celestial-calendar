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

"""Source-enclosed centres and a two-time Lipschitz tube for VSOP light-time separation."""

from fractions import Fraction as F

from .light_time_majorants import LIGHT_TIME, precession_bounds
from .rational_intervals import Interval as I, sincos


def raw_cartesian_enclosure(tables: dict, jde_tt: F) -> tuple:
  """Enclose the stored VSOP source at one TT time using Taylor elementary bounds."""
  tau = I.enclose((jde_tt - 2451545) / 365250)
  coordinates = []
  for family in "LBR":
    powers = {int(name[1:]): rows for name, rows in tables.items() if name[0] == family}
    if sorted(powers) != list(range(len(powers))):
      raise ValueError("VSOP source powers must be consecutive from zero")
    value = I.enclose(0)
    for power in reversed(range(len(powers))):
      table = I.enclose(0)
      for amplitude, phase, frequency in powers[power]:
        _, cosine = sincos(I.enclose(phase) + tau * frequency)
        table += cosine * amplitude
      value = value * tau + table / 100_000_000
    coordinates.append(value)
  lon, beta, radial = coordinates
  sin_lon, cos_lon = sincos(lon)
  sin_beta, cos_beta = sincos(beta)
  return radial * cos_beta * cos_lon, radial * cos_beta * sin_lon, radial * sin_beta


def norm_enclosure(components: tuple) -> I:
  return sum((value.square() for value in components), I.enclose(0)).sqrt()


def separation_for_reserve(
  centre: tuple, target: tuple, earth: tuple, centre_jde_tt: F, half_width_days: F, source_tau_radius: F, delay: F
) -> dict:
  """Certify a rectangle's geometry; the caller separately proves sample/fixed-point reach."""
  if len(centre) != 3 or len(target) < 2 or len(target) != len(earth) or half_width_days < 0 or delay < 0:
    raise ValueError("Require a Cartesian centre, matching derivatives and nonnegative rectangle dimensions")
  time_radius = half_width_days + delay
  if (abs(centre_jde_tt - 2451545) + time_radius) / 365250 > source_tau_radius:
    raise ValueError("The source derivative records do not cover the whole two-time rectangle")
  from_century = (abs(centre_jde_tt - 2451545) + time_radius) / 36525
  between_century = (2 * half_width_days + delay) / 36525
  emission_rotation = precession_bounds((from_century, F(1, 36525)), (between_century, F(1, 36525)))[1]
  observation_rotation = precession_bounds((from_century, F(0)), (between_century, F(1, 36525)))[1]
  emission_speed = target[1] + emission_rotation * target[0]
  observation_speed = earth[1] + observation_rotation * target[0]
  tube = emission_speed * time_radius + observation_speed * half_width_days
  radial = norm_enclosure(centre)
  cylindrical = norm_enclosure(centre[:2])
  radial_lower = radial.lo - tube
  axial_lower = cylindrical.lo - tube
  contraction = LIGHT_TIME * emission_speed
  if radial_lower <= 0 or contraction >= 1:
    raise ValueError("Local source enclosure cannot certify separated contractive light-time geometry")
  return {
    "radial_lower_au": radial_lower,
    "axial_lower_au": axial_lower,
    "tube_radius_au": tube,
    "delay_upper_days": delay,
    "emission_speed_au_per_day": emission_speed,
    "observation_speed_au_per_day": observation_speed,
    "emission_map_contraction_upper": contraction,
    "centre_radial_interval": (radial.lo, radial.hi),
    "centre_axial_interval": (cylindrical.lo, cylindrical.hi),
  }
