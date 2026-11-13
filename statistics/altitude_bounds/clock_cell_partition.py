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

"""Partition stored TT intervals into smooth clock pieces and Gregorian/model join collars."""

import math
from dataclasses import dataclass
from fractions import Fraction as F

from .calendar_lattice import exact_jde_fraction, inverse_integer_calendar, midnight_jd
from .clock_branch_majorants import branch_records
from .rounded_light_time_reach import outward_binary64


@dataclass(frozen=True)
class ClockJoin:
  time: F
  kind: str
  year: F


@dataclass(frozen=True)
class ClockPiece:
  left: F
  right: F
  joins: tuple


def partition_clock_interval(left: F, right: F, year_error: F, maximum_pieces: int) -> tuple:
  """Cover the whole requested interval; a collar owns its endpoints and may contain coincident joins."""
  if not left < right or year_error < 0 or maximum_pieces < 1:
    raise ValueError("Require ordered TT endpoints, a nonnegative year allowance and a positive piece budget")
  if any(F(float(value)) != value for value in (left, right)):
    raise ValueError("Clock partition endpoints must bind actual binary64 TT inputs")
  first, last = (inverse_integer_calendar(exact_jde_fraction(value)["julian_day_number"])[0] for value in (left, right))
  if last - first > maximum_pieces:
    raise ArithmeticError("The clock partition piece budget is exhausted")
  joins = [
    ClockJoin(midnight_jd(year, 1, 1), "year", F(year))
    for year in range(first, last + 2)
    if left <= midnight_jd(year, 1, 1) <= right
  ]
  for branch in branch_records()[:-1]:
    year = branch["end_year"]
    whole = year.numerator // year.denominator
    days = midnight_jd(whole + 1, 1, 1) - midnight_jd(whole, 1, 1)
    time = midnight_jd(whole, 1, 1) + (year - whole) * days
    if left <= time <= right:
      joins.append(ClockJoin(time, "model", year))
  collars = []
  for join in sorted(joins, key=lambda row: (row.time, row.kind)):
    before = F(math.nextafter(float(outward_binary64(join.time - 366 * year_error, False)), -math.inf))
    after = F(math.nextafter(float(outward_binary64(join.time + 366 * year_error, True)), math.inf))
    before, after = max(left, before), min(right, after)
    if before >= after:
      raise ArithmeticError("A clock join has no representable complete collar")
    if collars and before <= collars[-1].right:
      previous = collars[-1]
      collars[-1] = ClockPiece(previous.left, max(previous.right, after), (*previous.joins, join))
    else:
      collars.append(ClockPiece(before, after, (join,)))
  pieces = []
  cursor = left
  for collar in collars:
    if cursor < collar.left:
      pieces.append(ClockPiece(cursor, collar.left, ()))
    pieces.append(collar)
    cursor = collar.right
  if cursor < right:
    pieces.append(ClockPiece(cursor, right, ()))
  if len(pieces) > maximum_pieces:
    raise ArithmeticError("The clock partition piece budget is exhausted")
  return tuple(pieces)
