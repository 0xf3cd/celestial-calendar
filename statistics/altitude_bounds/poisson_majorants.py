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

"""Polynomial-phase trigonometric majorants for the retained Moon and Pluto source formulas."""

import re
from fractions import Fraction as F
from math import ceil, factorial

from .series_bounds import product_bounds, rotation_bounds


def stored(literal: str) -> F:
  return F.from_float(float(literal))


def pi_interval() -> tuple[F, F]:
  """Enclose pi by Machin's formula and the alternating-series next-term remainder."""

  def atan_inverse(denominator, terms):
    partial = sum(F((-1) ** k, (2 * k + 1) * denominator ** (2 * k + 1)) for k in range(terms))
    endpoint = partial + F((-1) ** terms, (2 * terms + 1) * denominator ** (2 * terms + 1))
    return min(partial, endpoint), max(partial, endpoint)

  a_lo, a_hi = atan_inverse(5, 80)
  b_lo, b_hi = atan_inverse(239, 30)
  return 16 * a_lo - 4 * b_hi, 16 * a_hi - 4 * b_lo


DEGREE_RAD_UPPER = F(ceil(pi_interval()[1] * 2**192), 180 * 2**192)


def array_rows(source: str, name: str, element_type: str, count: int, fields: int) -> tuple:
  """Consume every row/token of one named retained numeric array, without interpreting C++ expressions."""
  clean = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.DOTALL)
  declaration = (
    rf"inline\s+constexpr\s+std::array\s*<\s*{element_type}\s*,\s*(\d+)\s*>\s*{name}\s*\{{\{{(.*?)\}}\}}\s*;"
  )
  matches = list(re.finditer(declaration, clean, re.DOTALL))
  if len(matches) != 1 or int(matches[0][1]) != count:
    raise ValueError(f"Expected one {name} declaration with {count} rows")
  number = r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?"
  row_pattern = re.compile(r"\{\s*(" + number + (r")\s*,\s*(" + number) * (fields - 1) + r")\s*,?\s*\}")
  return initializer_rows(matches[0][2], row_pattern, count, name)


def initializer_rows(initializer: str, row_pattern, count: int, name: str) -> tuple:
  """Consume the full named initializer with a caller-specified fixed row shape."""
  position = 0
  rows = []
  while position < len(initializer):
    if initializer[position].isspace():
      position += 1
      continue
    row = row_pattern.match(initializer, position)
    if row is None:
      raise ValueError(f"{name}: unconsumed initializer at {position}")
    rows.append(tuple(stored(literal) for literal in row.groups()))
    position = row.end()
    while position < len(initializer) and initializer[position].isspace():
      position += 1
    if position < len(initializer):
      if initializer[position] != ",":
        raise ValueError(f"{name}: missing row separator")
      position += 1
  if len(rows) != count:
    raise ValueError(f"{name}: parsed {len(rows)} rows, expected {count}")
  return tuple(rows)


def polynomial_bounds(coefficients: tuple, radius: F, days_per_argument: F, order: int = 7) -> tuple:
  """Bound a source polynomial and its TT-day derivatives on an argument-radius interval."""
  if radius < 0 or days_per_argument <= 0 or order < 0:
    raise ValueError("Require a nonnegative interval/order and positive day scale")
  return tuple(
    sum(
      abs(coefficient) * F(factorial(n), factorial(n - k)) * radius ** (n - k)
      for n, coefficient in enumerate(coefficients)
      if n >= k
    )
    / days_per_argument**k
    for k in range(order + 1)
  )


def phase_bounds(weights: tuple, polynomials: tuple, radius: F, order: int = 7) -> tuple:
  """Keep coefficient cancellation in the phase polynomial before taking absolute derivative bounds."""
  if len(weights) != len(polynomials):
    raise ValueError("Phase weights and polynomials must have the same length")
  degree = max(map(len, polynomials))
  phase = tuple(
    sum(weight * (p[n] if n < len(p) else 0) for weight, p in zip(weights, polynomials, strict=True))
    for n in range(degree)
  )
  return tuple(value * DEGREE_RAD_UPPER for value in polynomial_bounds(phase, radius, F(36525), order))


def moon_bounds(lr: tuple, lat: tuple, radius: F, order: int = 7) -> dict:
  """Bound the truncated lunar coordinates plus additive terms, before longitude nutation and FP evaluation."""
  s = stored
  lp = (s("218.3164477"), s("481267.88123421"), -s("0.0015786"), F(1, 538841), -F(1, 65194000))
  d = (s("297.8501921"), s("445267.1114034"), -s("0.0018819"), F(1, 545868), -F(1, 113065000))
  m = (s("357.5291092"), s("35999.0502909"), -s("0.0001536"), F(1, 24490000))
  mp = (s("134.9633964"), s("477198.8675055"), s("0.0087414"), F(1, 69699), -F(1, 14712000))
  f = (s("93.2720950"), s("483202.0175233"), -s("0.0036539"), -F(1, 3526000), F(1, 863310000))
  a1 = (s("119.75"), s("131.849"))
  a2 = (s("53.09"), s("479264.290"))
  a3 = (s("313.45"), s("481266.484"))
  eccentricity = polynomial_bounds((F(1), -s("0.002516"), -s("0.0000074")), radius, F(36525), order)
  e_powers = ((F(1),) + (F(0),) * order, eccentricity, product_bounds(eccentricity, eccentricity))
  longitude, latitude, distance = ([F(0)] * (order + 1) for _ in range(3))
  for row in lr + lat:
    if row[1].denominator != 1 or abs(row[1]) > 2:
      raise ValueError("The source eccentricity exponent is outside 0,1,2")
    phase = phase_bounds(row[:4], (d, m, mp, f), radius, order)
    harmonic = product_bounds(e_powers[int(abs(row[1]))], rotation_bounds(phase))
    targets = ((longitude, row[4]), (distance, row[5])) if len(row) == 6 else ((latitude, row[4]),)
    for target, amplitude in targets:
      for k, value in enumerate(harmonic):
        target[k] += abs(amplitude) * value
  for target, amplitude, weights, polynomials in [
    (longitude, 3958, (1,), (a1,)),
    (longitude, 1962, (1, -1), (lp, f)),
    (longitude, 318, (1,), (a2,)),
    (latitude, -2235, (1,), (lp,)),
    (latitude, 382, (1,), (a3,)),
    (latitude, 175, (1, -1), (a1, f)),
    (latitude, 175, (1, 1), (a1, f)),
    (latitude, 127, (1, -1), (lp, mp)),
    (latitude, -115, (1, 1), (lp, mp)),
  ]:
    harmonic = rotation_bounds(phase_bounds(weights, polynomials, radius, order))
    for k, value in enumerate(harmonic):
      target[k] += abs(amplitude) * value
  mean = polynomial_bounds(lp, radius, F(36525), order)
  lon = tuple((mean[k] + longitude[k] / 1_000_000) * DEGREE_RAD_UPPER for k in range(order + 1))
  beta = tuple(value * DEGREE_RAD_UPPER / 1_000_000 for value in latitude)
  radial = tuple(value / 1000 + (s("385000.56") if k == 0 else 0) for k, value in enumerate(distance))
  lower = s("385000.56") - distance[0] / 1000
  return {"longitude_rad": lon, "latitude_rad": beta, "radius_km": radial, "radius_lower_km": lower}


def pluto_bounds(rows: tuple, radius: F, order: int = 7) -> dict:
  """Bound the Chapter37 J2000 spherical source, before precession and inverse FK5."""
  s = stored
  arguments = ((s("34.35"), s("3034.9057")), (s("50.08"), s("1222.1138")), (s("238.96"), s("144.96")))
  correction = [[F(0)] * (order + 1) for _ in range(3)]
  for row in rows:
    harmonic = rotation_bounds(phase_bounds(row[:3], arguments, radius, order))
    for column in range(3):
      amplitude = abs(row[3 + 2 * column]) + abs(row[4 + 2 * column])
      for k, value in enumerate(harmonic):
        correction[column][k] += amplitude * value
  mean = polynomial_bounds((s("238.958116"), s("144.96")), radius, F(36525), order)
  longitude = tuple((mean[k] + correction[0][k]) * DEGREE_RAD_UPPER for k in range(order + 1))
  latitude = tuple(
    (value + (abs(s("-3.908239")) if k == 0 else 0)) * DEGREE_RAD_UPPER for k, value in enumerate(correction[1])
  )
  radial = tuple(value + (s("40.7241346") if k == 0 else 0) for k, value in enumerate(correction[2]))
  return {
    "longitude_rad": longitude,
    "latitude_rad": latitude,
    "radius_au": radial,
    "radius_lower_au": s("40.7241346") - correction[2][0],
  }
