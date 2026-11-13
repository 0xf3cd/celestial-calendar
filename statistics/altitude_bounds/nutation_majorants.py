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

"""Fixed IAU1980 nutation, obliquity and equation-of-equinoxes derivative majorants."""

import re
from fractions import Fraction as F

from .poisson_majorants import DEGREE_RAD_UPPER, initializer_rows, phase_bounds, polynomial_bounds, stored as s
from .series_bounds import product_bounds, rotation_bounds


def nutation_rows(source: str) -> tuple:
  clean = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.DOTALL)
  declarations = re.findall(
    r"inline\s+constexpr\s+std::array<NutationCoeffs,\s*(\d+)>\s+IAU1980_NUTATION_COEFFS\s*\{\{(.*?)\}\};",
    clean,
    re.DOTALL,
  )
  if len(declarations) != 1 or int(declarations[0][0]) != 106:
    raise ValueError("Expected the single106-row fixed IAU1980 table")
  number = r"([+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?)"
  pair = r"\{\s*" + number + r"\s*,\s*" + number + r"\s*\}"
  weights = r"\{\s*" + r"\s*,\s*".join([r"(-?[0-9]+)"] * 5) + r"\s*\}"
  row = re.compile(r"\{\s*" + weights + r"\s*,\s*" + pair + r"\s*,\s*" + pair + r"\s*\}")
  return initializer_rows(declarations[0][1], row, 106, "IAU1980_NUTATION_COEFFS")


def nutation_bounds(rows: tuple, radius: F, order: int = 7) -> dict:
  arguments = (
    (s("297.85036"), s("445267.111480"), -s("0.0019142"), F(1) / s("189474.0")),
    (s("357.52772"), s("35999.050340"), -s("0.0001603"), -F(1) / s("300000.0")),
    (s("134.96298"), s("477198.867398"), s("0.0086972"), F(1) / s("56250.0")),
    (s("93.27191"), s("483202.017538"), -s("0.0036825"), F(1) / s("327270.0")),
    (s("125.04452"), -s("1934.136261"), s("0.0020708"), F(1) / s("450000.0")),
  )
  summed = [[F(0)] * (order + 1) for _ in range(2)]
  for row in rows:
    harmonic = rotation_bounds(phase_bounds(row[:5], arguments, radius, order))
    for column in range(2):
      amplitude = polynomial_bounds(row[5 + 2 * column : 7 + 2 * column], radius, F(36525), order)
      term = product_bounds(amplitude, harmonic)
      for k, value in enumerate(term):
        summed[column][k] += value
  scale = s("0.0001") * DEGREE_RAD_UPPER / 3600
  longitude, obliquity = (tuple(value * scale for value in coordinate) for coordinate in summed)
  mean_coefficients = (s("84381.448"), -s("46.8150"), -s("0.00059"), s("0.001813"))
  mean = tuple(
    value * DEGREE_RAD_UPPER / 3600 for value in polynomial_bounds(mean_coefficients, radius, F(36525), order)
  )
  true = tuple(a + b for a, b in zip(mean, obliquity, strict=True))
  equinoxes = product_bounds(longitude, rotation_bounds(true))
  return {
    "longitude_rad": longitude,
    "obliquity_rad": obliquity,
    "mean_obliquity_rad": mean,
    "true_obliquity_rad": true,
    "equation_of_equinoxes_rad": equinoxes,
  }
