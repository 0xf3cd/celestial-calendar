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

"""Source-derived clock bounds shared by ordinary Gregorian pieces of a proved TT tier."""

import math
from fractions import Fraction as F

from .calendar_lattice import midnight_jd
from .clock_branch_majorants import branch_records, clock_bounds
from .clock_cell_partition import partition_clock_interval
from .nutation_majorants import nutation_bounds, nutation_rows
from .poisson_majorants import DEGREE_RAD_UPPER, stored
from .rounded_light_time_reach import outward_binary64
from .series_bounds import product_bounds
from .sidereal_evaluation_error import sidereal_error
from .spherical_coordinate_majorants import dyadic_upper
from .uniform_clock_error import exact_decimal_year, uniform_clock_error


def model_join_time(year: F) -> F:
  whole = year.numerator // year.denominator
  start = midnight_jd(whole, 1, 1)
  return start + (year - whole) * (midnight_jd(whole + 1, 1, 1) - start)


def branch_rotation(index: int, left: F, right: F, earth: str) -> tuple:
  """Bound smooth rotation derivatives on either year length; year/model joins remain separate collars."""
  branch = branch_records()[index]
  clock = clock_bounds(branch, 365)
  if clock["ut1_first_lower"] <= 0:
    raise ArithmeticError("The tier's clock is not proved increasing on each smooth piece")
  magnitude = max(abs(left - 2451545), abs(right - 2451545)) + branch["delta_t_year_derivatives"][0] / 86400
  days = (magnitude, clock["ut1_first_upper"], clock["ut1_second_abs"], clock["ut1_third_abs"])
  centuries = tuple(value / 36525 for value in days)
  square = product_bounds(centuries, centuries)
  cube = product_bounds(square, centuries)
  mean = tuple(
    DEGREE_RAD_UPPER * (stored("360.98564736629") * days[k] + stored("0.000387933") * square[k] + cube[k] / 38710000)
    for k in range(1, 4)
  )
  radius = max(abs(left - 2451545), abs(right - 2451545)) / 36525
  iau = nutation_bounds(nutation_rows(earth), radius, 3)
  return (F(0), *dyadic_upper(tuple(mean[k - 1] + iau["equation_of_equinoxes_rad"][k] for k in range(1, 4))))


def ordinary_clock_tiers(left: F, right: F, earth: str, delta: str) -> dict:
  """Cover each branch's ordinary pieces; value bounds include whole years and derivatives exclude joins."""
  if not left < right or any(F(float(value)) != value for value in (left, right)):
    raise ValueError("A clock tier requires ordered exact binary64 endpoints")
  whole = uniform_clock_error(left, right, delta)
  error = whole["year_error_upper"]
  joins = []
  for row in branch_records()[:-1]:
    time = model_join_time(row["end_year"])
    if left <= time <= right:
      before = F(math.nextafter(float(outward_binary64(time - 366 * error, False)), -math.inf))
      after = F(math.nextafter(float(outward_binary64(time + 366 * error, True)), math.inf))
      joins.append({"year": row["end_year"], "time": time, "before": before, "after": after})
  records = []
  branches = branch_records()
  for index, branch in enumerate(branches):
    start, end = model_join_time(branch["start_year"]), model_join_time(branch["end_year"])
    a, b = max(left, start), min(right, end)
    for join in joins:
      if join["time"] == start:
        a = max(a, join["after"])
      if join["time"] == end:
        b = min(b, join["before"])
    if a >= b:
      continue
    clock = uniform_clock_error(a, b, delta)
    if (
      len(clock["model_bounds"]) != 1
      or clock["model_bounds"][0]["branch"] != index
      or clock["model_bounds"][0]["mismatch_seconds"]
    ):
      raise ArithmeticError("An ordinary clock record still contains model selection ambiguity")
    if not branch["start_year"] < exact_decimal_year(a) - error <= exact_decimal_year(b) + error < branch["end_year"]:
      raise ArithmeticError("The complete input-year halo leaves the selected model branch")
    theta = sidereal_error(a, b, *clock["reference_ut1_interval"], clock["ut1_jd_error_days"], F(180), earth)
    records.append(
      {
        "branch": index,
        "interval": (a, b),
        "clock": clock,
        "sidereal_degree_error": theta["local"].error,
        "rotation": branch_rotation(index, a, b, earth),
      }
    )
  partition = partition_clock_interval(left, right, error, 4096)
  for piece in partition:
    if not piece.joins and not any(
      record["interval"][0] <= piece.left < piece.right <= record["interval"][1] for record in records
    ):
      raise ArithmeticError("An ordinary Gregorian piece lacks a complete model record")
  theta = sidereal_error(left, right, *whole["reference_ut1_interval"], whole["ut1_jd_error_days"], F(180), earth)
  return {
    "tt_interval": (left, right),
    "year_error_upper": error,
    "model_joins": joins,
    "records": records,
    "ordinary_sidereal_degree_error": max(row["sidereal_degree_error"] for row in records),
    "collar_sidereal_degree_error": theta["local"].error,
    "rotation": tuple(max(row["rotation"][k] for row in records) for k in range(4)),
    "partition": partition,
    "collar_clock": whole,
  }
