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

"""Derivative majorants for the smooth retarded-distance fixed point, before evaluation-error analysis."""

from fractions import Fraction as F
from math import ceil, factorial

from .poisson_majorants import DEGREE_RAD_UPPER, stored as s
from .series_bounds import product_bounds, rotation_bounds
from .vector_derivative_majorants import norm_bounds


LIGHT_TIME = s("0.0057755183")


def compose_bounds(outer: tuple, inner: tuple) -> tuple:
  """Compose derivative magnitude sequences by the nonnegative Taylor form of Faa di Bruno."""
  if len(outer) != len(inner) or any(value < 0 for value in outer + inner):
    raise ValueError("Require equal nonnegative derivative sequences")
  size = len(outer)
  deviation = (F(0),) + inner[1:]
  power = (F(1),) + (F(0),) * (size - 1)
  result = [outer[0]] + [F(0)] * (size - 1)
  for n in range(1, size):
    power = product_bounds(power, deviation)
    for k in range(1, size):
      result[k] += outer[n] * power[k] / factorial(n)
  return tuple(result)


def precession_bounds(from_centuries: tuple, between_centuries: tuple) -> tuple:
  """Bound Rz(Pi+p)*Rx(-eta)*Rz(-Pi) from Meeus21.5 in the source's two time arguments."""
  size = len(from_centuries)
  if size != len(between_centuries):
    raise ValueError("Precession arguments must cover the same derivative orders")
  t, d = from_centuries, between_centuries

  def constant(value):
    return (s(value),) + (F(0),) * (size - 1)

  def scale(values, coefficient):
    return tuple(value * s(coefficient) for value in values)

  def add(*values):
    return tuple(sum(parts) for parts in zip(*values, strict=True))

  def multiply(left, right):
    return product_bounds(left, right)

  t2 = multiply(t, t)
  eta = multiply(
    d,
    add(
      constant("47.0029"),
      scale(t, "0.06603"),
      scale(t2, "0.000598"),
      multiply(d, add(constant("0.03302"), scale(t, "0.000598"), scale(d, "0.00006"))),
    ),
  )
  pi = add(
    multiply(t, add(constant("3289.4789"), scale(t, "0.60622"))),
    multiply(d, add(constant("869.8089"), scale(t, "0.50491"), scale(d, "0.03536"))),
  )
  p = multiply(
    d,
    add(
      constant("5029.0966"),
      scale(t, "2.22226"),
      scale(t2, "0.000042"),
      multiply(d, add(constant("1.11113"), scale(t, "0.000042"), scale(d, "0.000006"))),
    ),
  )
  factor = DEGREE_RAD_UPPER / 3600
  eta, pi, p = (tuple(value * factor for value in values) for values in (eta, pi, p))
  return product_bounds(product_bounds(rotation_bounds(add(pi, p)), rotation_bounds(eta)), rotation_bounds(pi))


def distance_bounds(target: tuple, earth: tuple, century_radius: F, separation: F, rotation=precession_bounds) -> tuple:
  """Solve the majorant inequality order by order, requiring a strict contraction and proved separation."""
  if len(target) != len(earth) or len(target) < 2 or century_radius < 0 or separation <= 0:
    raise ValueError("Require matching source sequences, a nonnegative epoch radius and positive geocentric separation")
  radius = [target[0] + earth[0]]
  audits = []
  for order in range(1, len(target)):

    def proposal(trial, order=order):
      r = tuple(radius + [trial])
      emission = (F(0), 1 + LIGHT_TIME * r[1]) + tuple(LIGHT_TIME * v for v in r[2:])
      from_centuries = (century_radius + LIGHT_TIME * r[0] / 36525,) + tuple(v / 36525 for v in emission[1:])
      between_centuries = tuple(LIGHT_TIME * v / 36525 for v in r)
      frame = rotation(from_centuries, between_centuries)
      retarded = compose_bounds(target[: order + 1], emission)
      observed = product_bounds(frame, retarded)
      geocentric = tuple(a + b for a, b in zip(observed, earth[: order + 1], strict=True))
      return norm_bounds(geocentric, separation)[order]

    intercept = proposal(F(0))
    slope = proposal(F(1)) - intercept
    if proposal(F(2)) != intercept + 2 * slope:
      raise ArithmeticError("Highest-order majorant is not affine")
    if not 0 <= slope < 1:
      raise ValueError("No strict light-time derivative contraction")
    exact_upper = intercept / (1 - slope)
    upper = F(ceil(exact_upper * 2**192), 2**192)
    if upper < exact_upper or proposal(upper) > upper:
      raise ArithmeticError("Directed implicit derivative bound failed")
    radius.append(upper)
    audits.append({"order": order, "slope_hex": float(slope).hex(), "upper_exact": str(upper)})
  return tuple(radius), audits
