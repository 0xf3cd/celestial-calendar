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

"""Whole-interval transport from accepted rounded light-time estimates to the reference fixed point."""

import math
from fractions import Fraction as F

from .rounded_light_time_reach import outward_binary64
from .spherical_coordinate_majorants import dyadic_upper


def accepted_fixed_point_errors(
  uniform_map: dict, certificate: dict, observation_left: F, observation_right: F
) -> dict:
  """Bound all accepted old estimates using an observation interval and strict map contraction."""
  contraction = certificate["emission_map_contraction_upper"]
  if not 0 < observation_left <= observation_right or not 0 <= contraction < 1:
    raise ValueError("Require positive admitted TT endpoints and a proved strict source contraction")
  maximum_observation = outward_binary64(observation_right, True)
  maximum_ulp = F(math.ulp(float(maximum_observation)))
  emission = (maximum_ulp + uniform_map["rounded_map_error_upper_days"]) / (1 - contraction)
  vector = uniform_map["vector_error_upper_au"] + certificate["emission_speed_au_per_day"] * emission
  distance = uniform_map["distance_error_upper_au"] + certificate["emission_speed_au_per_day"] * emission
  return {
    "observation_ulp_upper_days": maximum_ulp,
    "emission_error_upper_days": dyadic_upper((emission,))[0],
    "fixed_point_vector_error_upper_au": dyadic_upper((vector,))[0],
    "fixed_point_distance_error_upper_au": dyadic_upper((distance,))[0],
  }
