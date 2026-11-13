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

"""Truncated ELP2000-82B context, table folds, auxiliary terms and apparent-coordinate errors."""

import re
from fractions import Fraction as F

from .apparent_correction_error import normalized_degrees
from .clock_model_evaluation_error import integer_power_error
from .floating_operation_bounds import FloatingBound as B
from .nutation_evaluation_error import nutation_error_from_centuries
from .poisson_majorants import array_rows, pi_interval
from .precession_evaluation_error import source_polynomial_bound
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, rounded


def lunar_context_error(centuries: B, source: str) -> dict:
  """Follow source powers, polynomials and stored normalized degrees for eight lunar arguments."""
  clean = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.DOTALL)
  body = clean.split("inline auto create_context(const double jc)", 1)[1].split("return {", 1)[0]
  variables = {"jc": centuries}
  for name, expression in re.findall(r"const double (\w+)\s*=\s*(.*?);", body, re.DOTALL):
    variables[name] = source_polynomial_bound(expression, variables)
  if set(variables) != {"jc", "jc2", "jc3", "jc4", "E"}:
    raise ValueError("The lunar context source power/eccentricity assignments changed")
  result = {"E": variables["E"]}
  for name, expression in re.findall(r"const toolbox::AngleDeg (\w+)\s*\{(.*?)\};", body, re.DOTALL):
    result[name] = normalized_degrees(source_polynomial_bound(expression, variables))
  if set(result) != {"E", "Lp", "D", "M", "Mp", "F", "A1", "A2", "A3"}:
    raise ValueError("The eight lunar degree arguments were not all consumed")
  return result


def lunar_eccentricity_powers(eccentricity: B) -> tuple:
  """Bound pow(E,0/1/2) with exact power zero and separate power-one allowances."""
  one_error = LIBM_ALLOWANCE * (1 + eccentricity.magnitude) + SUBNORMAL_ALLOWANCE
  return (
    B(F(1)),
    B(eccentricity.magnitude + one_error, eccentricity.error + one_error),
    integer_power_error(eccentricity, 2),
  )


def lunar_evaluation_error(left: F, right: F, elp_source: str, earth_source: str) -> dict:
  """Bound the complete TT interval through stored apparent longitude/latitude and AU distance."""
  if left > right:
    raise ValueError("The lunar TT interval endpoints are reversed")
  time_magnitude = max(abs(left - 2451545), abs(right - 2451545))
  centuries = B(*rounded(time_magnitude, F(0))) / 36525
  context = lunar_context_error(centuries, elp_source)
  powers = lunar_eccentricity_powers(context["E"])
  lr = array_rows(elp_source, "LR", "LRCoefficients", 60, 6)
  latitude_rows = array_rows(elp_source, "B", "BCoefficients", 60, 5)

  def row_phase(row):
    phase = B(abs(row[0])) * context["D"]
    for weight, name in zip(row[1:4], ("M", "Mp", "F"), strict=True):
      phase = phase + B(abs(weight)) * context[name]
    return phase.degrees_to_radians().sine_or_cosine()

  def correction(row):
    exponent = abs(row[1])
    if exponent.denominator != 1 or exponent > 2:
      raise ValueError("The source lunar eccentricity exponent is outside0,1,2")
    return powers[int(exponent)]

  longitude_sum, radius_sum, latitude_sum = B(F(0)), B(F(0)), B(F(0))
  for row in lr:
    phase, factor = row_phase(row), correction(row)
    longitude_sum = longitude_sum + B(abs(row[4])) * phase * factor
    radius_sum = radius_sum + B(abs(row[5])) * phase * factor
  for row in latitude_rows:
    latitude_sum = latitude_sum + B(abs(row[4])) * row_phase(row) * correction(row)

  angles = {name: value.degrees_to_radians() for name, value in context.items() if name != "E"}
  longitude_aux = B.literal(3958.0) * angles["A1"].sine_or_cosine()
  longitude_aux = longitude_aux + B.literal(1962.0) * (angles["Lp"] - angles["F"]).sine_or_cosine()
  longitude_aux = longitude_aux + B.literal(318.0) * angles["A2"].sine_or_cosine()
  latitude_aux = B.literal(-2235.0) * angles["Lp"].sine_or_cosine()
  latitude_aux = latitude_aux + B.literal(382.0) * angles["A3"].sine_or_cosine()
  latitude_aux = latitude_aux + B.literal(175.0) * (angles["A1"] - angles["F"]).sine_or_cosine()
  latitude_aux = latitude_aux + B.literal(175.0) * (angles["A1"] + angles["F"]).sine_or_cosine()
  latitude_aux = latitude_aux + B.literal(127.0) * (angles["Lp"] - angles["Mp"]).sine_or_cosine()
  latitude_aux = latitude_aux - B.literal(115.0) * (angles["Lp"] + angles["Mp"]).sine_or_cosine()

  nutation = nutation_error_from_centuries(centuries, earth_source)
  longitude = normalized_degrees(
    context["Lp"] + (longitude_sum + longitude_aux) / 1_000_000 + nutation["longitude_degrees"]
  )
  latitude = (latitude_sum + latitude_aux) / 1_000_000
  radius_km = B.literal(385000.56) + radius_sum / 1000
  distance = radius_km / 149597870.700
  reference_distance_lower_au = (F(385000.56) - (radius_sum.magnitude + radius_sum.error) / 1000) / F(149597870.700)
  if reference_distance_lower_au <= 0:
    raise ValueError("The complete lunar source box has no positive distance lower")
  beta_upper = (latitude.magnitude + latitude.error) * pi_interval()[1] / 180
  return {
    "context": context,
    "eccentricity_powers": powers,
    "longitude_series": longitude_sum,
    "latitude_series": latitude_sum,
    "radius_series": radius_sum,
    "longitude_auxiliary": longitude_aux,
    "latitude_auxiliary": latitude_aux,
    "final_longitude_degrees": longitude,
    "final_latitude_degrees": latitude,
    "distance_au": distance,
    "reference_distance_lower_au": reference_distance_lower_au,
    "reference_latitude_cosine_lower": max(F(0), 1 - beta_upper**2 / 2),
    "true_obliquity_degrees": nutation["true_obliquity_degrees"],
  }
