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

"""Smooth-branch DeltaT and TT-to-UT1 derivative majorants, before datetime/FP allowances."""

from fractions import Fraction as F
from math import comb, factorial

from .poisson_majorants import polynomial_bounds, stored as s


def translated(polynomial: tuple, centre: F) -> tuple:
  """Translate a polynomial exactly before bounding its coefficients around the interval midpoint."""
  return tuple(
    sum(polynomial[n] * comb(n, k) * centre ** (n - k) for n in range(k, len(polynomial)))
    for k in range(len(polynomial))
  )


def branch_records() -> list:
  """Transcribe the DeltaT branches reached by fixed Algo5, including its historical delegation."""
  branches = [
    (
      401,
      500,
      0,
      100,
      (
        s("10583.6"),
        -s("1014.41"),
        s("33.78311"),
        -s("5.952053"),
        -s("0.1798452"),
        s("0.022174192"),
        s("0.0090316521"),
      ),
    ),
    (
      500,
      1600,
      1000,
      100,
      (s("1574.2"), -s("556.01"), s("71.23472"), s("0.319781"), -s("0.8503463"), -s("0.005050998"), s("0.0083572073")),
    ),
    (1600, 1700, 1600, 1, (F(120), -s("0.9808"), -s("0.01532"), F(1) / s("7129.0"))),
    (1700, 1800, 1700, 1, (s("8.83"), s("0.1603"), -s("0.0059285"), s("0.00013336"), -F(1) / s("1174000.0"))),
    (
      1800,
      1860,
      1800,
      1,
      (
        s("13.72"),
        -s("0.332447"),
        s("0.0068612"),
        s("0.0041116"),
        -s("0.00037436"),
        s("0.0000121272"),
        -s("0.0000001699"),
        s("0.000000000875"),
      ),
    ),
    (
      1860,
      1900,
      1860,
      1,
      (s("7.62"), s("0.5737"), -s("0.251754"), s("0.01680668"), -s("0.0004473624"), F(1) / s("233174.0")),
    ),
    (1900, 1920, 1900, 1, (-s("2.79"), s("1.494119"), -s("0.0598939"), s("0.0061966"), -s("0.000197"))),
    (1920, 1941, 1920, 1, (s("21.20"), s("0.84493"), -s("0.076100"), s("0.0020936"))),
    (1941, 1961, 1950, 1, (s("29.07"), s("0.407"), -F(1) / s("233.0"), F(1) / s("2547.0"))),
    (1961, 1986, 1975, 1, (s("45.45"), s("1.067"), -F(1) / s("260.0"), -F(1) / s("718.0"))),
    (
      1986,
      2005,
      2000,
      1,
      (s("63.86"), s("0.3345"), -s("0.060374"), s("0.0017275"), s("0.000651814"), s("0.00002373599")),
    ),
  ]
  last = s("2026.7479452054795")
  branches.append(
    (
      F(2005),
      last,
      1990,
      1,
      (
        -s("10202.434877796037"),
        s("1282.9448070843766"),
        -s("87.1663776984486"),
        s("3.4765919929384914"),
        -s("0.08136170238837674"),
        s("0.0010347362811410302"),
        -s("5.521142968454219e-06"),
      ),
    )
  )
  branches.append((last, F(32768), 1825, 100, (-s("150.635787765945"), F(0), s("31.4115"))))
  result = []
  for index, (start, end, origin, scale, polynomial) in enumerate(branches):
    lo, hi = (F(start) - origin) / scale, (F(end) - origin) / scale
    centre, radius = (lo + hi) / 2, (hi - lo) / 2
    bounds = list(polynomial_bounds(translated(polynomial, centre), radius, F(scale), 3))
    if index == 11:
      for k in range(4):
        bounds[k] += s("34469.54162338588") * factorial(k) / lo ** (k + 1)
    if index == 12:
      for k in range(4):
        bounds[k] += s("284.8435805251424") * (s("0.4487989505128276") / scale) ** k
    result.append(
      {
        "start_year": F(start),
        "end_year": F(end),
        "start_inclusive": index != 12,
        "end_inclusive": index == 11,
        "origin": F(origin),
        "scale": F(scale),
        "polynomial": polynomial,
        "delta_t_year_derivatives": tuple(bounds),
      }
    )
  return result


def clock_bounds(branch: dict, year_days: int) -> dict:
  """Bound JDE(TT)-DeltaT(decimal_year(TT))/86400 on one branch and Gregorian year."""
  if year_days not in (365, 366):
    raise ValueError("Gregorian year length must be 365 or 366")
  delta = branch["delta_t_year_derivatives"]
  first = delta[1] / (86400 * year_days)
  return {
    "ut1_first_lower": 1 - first,
    "ut1_first_upper": 1 + first,
    "ut1_second_abs": delta[2] / (86400 * year_days**2),
    "ut1_third_abs": delta[3] / (86400 * year_days**3),
  }


def cosine_join_interval(argument: F) -> tuple:
  """Enclose the small extrapolation-join cosine by its alternating Taylor remainder."""
  if abs(argument) > F(3, 2):
    raise ValueError("This bounded cosine construction is only for the retained clock join")
  terms = 24
  partial = sum((-1) ** k * argument ** (2 * k) / factorial(2 * k) for k in range(terms))
  endpoint = partial + (-1) ** terms * argument ** (2 * terms) / factorial(2 * terms)
  return min(partial, endpoint), max(partial, endpoint)


def join_jumps(records: list) -> list:
  """Return exact-real right-minus-left DeltaT enclosures at all twelve model joins."""

  def value(index, year):
    record = records[index]
    t = (year - record["origin"]) / record["scale"]
    polynomial = sum(coefficient * t**power for power, coefficient in enumerate(record["polynomial"]))
    if index == 11:
      polynomial += s("34469.54162338588") / t
    if index == 12:
      lower, upper = cosine_join_interval(s("0.4487989505128276") * (t + s("0.75")))
      return polynomial + s("284.8435805251424") * lower, polynomial + s("284.8435805251424") * upper
    return polynomial, polynomial

  result = []
  for index in range(len(records) - 1):
    year = records[index]["end_year"]
    left_lower, left_upper = value(index, year)
    right_lower, right_upper = value(index + 1, year)
    result.append(
      {"year": year, "jump_lower_seconds": right_lower - left_upper, "jump_upper_seconds": right_upper - left_lower}
    )
  return result
