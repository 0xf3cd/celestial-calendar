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

"""Derive low-order VSOP source records directly from the retained coefficient initializers."""

import hashlib
from fractions import Fraction as F
from pathlib import Path

from .raw_evaluation_error import coordinate_error
from .series_bounds import moment_check, outward, product_bounds, rotation_bounds
from .vsop_derivative_majorants import coordinate_derivative_bounds


def derive_vsop_record(planet: str, source: Path, tables: dict, radius: F, order: int = 3) -> dict:
  """Retain exact source sums and their independently convolved moments through the requested TT-day order."""
  if radius <= 0 or not 1 <= order <= 3:
    raise ValueError("Require a positive source radius and derivative order from1 through3")
  coordinates, evaluation = {}, {}
  for family in "LBR":
    terms = {int(name[1:]): values for name, values in tables.items() if name[0] == family}
    bounds = coordinate_derivative_bounds(terms, radius, order)
    if bounds != moment_check(terms, radius, order):
      raise ArithmeticError("The source term and independent moment derivatives disagree")
    coordinates[family] = bounds
    magnitude, error = coordinate_error(terms, radius)
    evaluation[family] = {"computed_magnitude": outward(magnitude), "absolute_error": outward(error)}
  direction = product_bounds(rotation_bounds(coordinates["L"]), rotation_bounds(coordinates["B"]))
  vector = product_bounds(coordinates["R"], direction)
  return {
    "planet": planet,
    "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    "tau_radius_exact": str(radius),
    "coordinate_units": {"L": "rad", "B": "rad", "R": "AU"},
    "derivative_time_unit": "TT day",
    "coordinate_derivatives": {family: [outward(value) for value in values] for family, values in coordinates.items()},
    "evaluation_at_exact_binary64_tau": evaluation,
    "direction_norm_derivatives": [outward(value) for value in direction],
    "cartesian_norm_derivatives_au": [outward(value) for value in vector],
  }
