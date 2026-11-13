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

"""Fixed IAU1980 allowances in source polynomial, phase, fold and degree-storage order."""

import re
from fractions import Fraction as F

from .floating_operation_bounds import FloatingBound as B
from .nutation_majorants import nutation_rows
from .precession_evaluation_error import source_polynomial_bound


def nutation_error_from_centuries(centuries: B, source: str) -> dict:
  """Use century-operation enclosures for the fixed IAU1980 source path."""
  clean = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.DOTALL)
  context = clean.split("inline auto gen_eval_θ(const double jc)", 1)[1].split("return [=]", 1)[0]
  assignments = re.findall(r"const double (\w+)\s*=\s*(.*?);", context, re.DOTALL)
  variables = {"jc": centuries}
  for name, expression in assignments:
    variables[name] = source_polynomial_bound(expression, variables)
  if set(variables) != {"jc", "jc2", "jc3", "D", "M", "Mp", "F", "Ω"}:
    raise ValueError("The source fundamental-argument assignments changed")
  totals = [B(F(0)), B(F(0))]
  for row in nutation_rows(source):
    products = tuple(
      variables[name] * B.literal(float(weight))
      for name, weight in zip(("D", "M", "Mp", "F", "Ω"), row[:5], strict=True)
    )
    phase = products[0]
    for term in products[1:]:
      phase = phase + term
    wave = phase.degrees_to_radians().sine_or_cosine()
    for index, column in enumerate((5, 7)):
      amplitude = B.literal(float(row[column])) + B.literal(float(row[column + 1])) * centuries
      totals[index] = totals[index] + amplitude * wave
  longitude, obliquity = tuple(total * B.literal(0.0001) / 3600 for total in totals)
  mean_expression = re.search(r"const double ε0_arcsec\s*=\s*(.*?);", clean, re.DOTALL)[1]
  mean = source_polynomial_bound(mean_expression, {"jc": centuries}) / 3600
  true = mean + obliquity
  equinoxes = longitude * true.degrees_to_radians().sine_or_cosine()
  return {
    "longitude_degrees": longitude,
    "obliquity_degrees": obliquity,
    "mean_obliquity_degrees": mean,
    "true_obliquity_degrees": true,
    "equation_of_equinoxes_degrees": equinoxes,
  }
