# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Keep TT lattice, clock units and complete Gregorian/model join ownership explicit."""

import math
import sys
import unittest
from fractions import Fraction as F
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "statistics"))

from altitude_bounds.calendar_lattice import (  # noqa: E402
  END_JDE_TT,
  NANOSECONDS_PER_DAY,
  START_JDE_TT,
  exact_jde_fraction,
  inverse_integer_calendar,
  midnight_jd,
  shifted_datetime_error,
  tt_datetime_error_upper_ns,
)
from altitude_bounds.clock_branch_majorants import branch_records, clock_bounds, translated  # noqa: E402
from altitude_bounds.clock_cell_partition import partition_clock_interval  # noqa: E402
from altitude_bounds.clock_model_evaluation_error import clock_model_branch_error, clock_source_bodies  # noqa: E402
from altitude_bounds.source_arithmetic import arithmetic  # noqa: E402
from altitude_bounds.uniform_clock_error import exact_decimal_year, uniform_clock_error  # noqa: E402


class CalendarLatticeTest(unittest.TestCase):
  def test_midnight_and_inverse_match_j2000_and_directed_calendar_dates(self):
    self.assertEqual(midnight_jd(2000, 1, 1), F(4903089, 2))
    for date in ((401, 1, 1), (1582, 10, 15), (1900, 3, 1), (2000, 2, 29), (32767, 12, 31)):
      with self.subTest(date=date):
        self.assertEqual(inverse_integer_calendar(int(midnight_jd(*date) + F(1, 2))), date)

  def test_gregorian_century_leap_rules_preserve_year_lengths(self):
    self.assertEqual(midnight_jd(2001, 1, 1) - midnight_jd(2000, 1, 1), 366)
    self.assertEqual(midnight_jd(1901, 1, 1) - midnight_jd(1900, 1, 1), 365)
    self.assertEqual(midnight_jd(2101, 1, 1) - midnight_jd(2100, 1, 1), 365)

  def test_jde_fraction_and_nanoseconds_respect_the_source_lattice(self):
    result = exact_jde_fraction(F(2451545) + F(1, 4))
    self.assertEqual(result["exact_fraction"], F(3, 4))
    self.assertEqual(result["predicted_nanoseconds"], 64800000000000)
    for time in (START_JDE_TT, START_JDE_TT + F(1, 2**32), F(2451545) + F(1, 2**31)):
      with self.subTest(time=time):
        actual = exact_jde_fraction(time)
        self.assertLessEqual(abs(actual["quantization_error_ns"]), tt_datetime_error_upper_ns())
        self.assertLess(actual["predicted_nanoseconds"], NANOSECONDS_PER_DAY)

  def test_inverse_calendar_end_is_exclusive_and_nonbinary_times_are_rejected(self):
    with self.assertRaises(ValueError):
      exact_jde_fraction(END_JDE_TT)
    with self.assertRaises(ValueError):
      exact_jde_fraction(F(2451545) + F(1, 10))
    previous = F(math.nextafter(float(END_JDE_TT), -math.inf))
    result = exact_jde_fraction(previous)
    self.assertEqual(inverse_integer_calendar(result["julian_day_number"]), (32767, 12, 31))

  def test_clock_shift_error_keeps_seconds_to_days_and_quantization(self):
    actual = shifted_datetime_error(F(100), F(1))
    self.assertEqual(actual["model_error_days"], F(1, 86400))
    self.assertGreater(actual["total_error_days"], F(1, 86400))
    self.assertGreater(actual["nanosecond_quantization_error_days"], 0)
    with self.assertRaises(ValueError):
      shifted_datetime_error(F(100), F(-1))


class ClockBranchesTest(unittest.TestCase):
  def test_translated_polynomial_equals_the_original_before_taking_magnitudes(self):
    coefficients, centre, x = (F(2), F(-3), F(5)), F(7, 2), F(-1, 3)
    shifted = translated(coefficients, centre)
    self.assertEqual(
      sum(a * x**k for k, a in enumerate(shifted)), sum(a * (centre + x) ** k for k, a in enumerate(coefficients))
    )

  def test_branch_roster_has_one_fitted_endpoint_owner(self):
    records = branch_records()
    self.assertEqual(len(records), 13)
    for left, right in zip(records, records[1:], strict=False):
      self.assertEqual(left["end_year"], right["start_year"])
    self.assertTrue(records[11]["end_inclusive"])
    self.assertFalse(records[12]["start_inclusive"])
    self.assertTrue(all(value >= 0 for row in records for value in row["delta_t_year_derivatives"]))

  def test_clock_derivative_units_use_seconds_per_day_and_the_shorter_year(self):
    record = {"delta_t_year_derivatives": (F(10), F(2), F(3), F(4))}
    ordinary = clock_bounds(record, 365)
    leap = clock_bounds(record, 366)
    self.assertEqual(ordinary["ut1_first_upper"], 1 + F(2, 86400 * 365))
    self.assertEqual(ordinary["ut1_second_abs"], F(3, 86400 * 365**2))
    self.assertGreater(ordinary["ut1_first_upper"], leap["ut1_first_upper"])
    with self.assertRaises(ValueError):
      clock_bounds(record, 360)

  def test_source_arithmetic_is_exact_and_rejects_unapproved_operations(self):
    values = {"x": F(3)}
    self.assertEqual(arithmetic("std::pow(x, 3) + 2.0 / x", values, True), 27 + F(2, 3))
    self.assertEqual(arithmetic("-x", values, True), -3)
    self.assertTrue(arithmetic("x >= 2.0 and x < 4.0", values, True))
    for source in ("other(x)", "std::pow(x, 0.5)", "std::cos(x)", "True"):
      with self.subTest(source=source), self.assertRaises(ValueError):
        arithmetic(source, values, True)

  def test_actual_model_bodies_and_uniform_clock_enclose_the_j2000_branch(self):
    source = (ROOT / "src/astro/delta_t.hpp").read_text()
    self.assertEqual(len(clock_source_bodies(source)), 13)
    bound = clock_model_branch_error(10, F(2000), F(2000), F(0), source)
    self.assertGreaterEqual(bound.magnitude, F(float("63.86")))
    self.assertGreater(bound.error, 0)
    left, right = F(2451545), F(2451545) + F(1, 4)
    result = uniform_clock_error(left, right, source)
    self.assertEqual([row["branch"] for row in result["model_bounds"]], [10])
    self.assertLessEqual(result["reference_ut1_interval"][0], left)
    self.assertGreaterEqual(result["reference_ut1_interval"][1], right)
    self.assertEqual(exact_decimal_year(midnight_jd(2000, 1, 1)), 2000)

  def test_coincident_year_and_model_joins_share_one_complete_collar(self):
    centre = midnight_jd(2005, 1, 1)
    left, right = centre - F(1, 4), centre + F(1, 4)
    pieces = partition_clock_interval(left, right, F(1, 2**40), 16)
    self.assertEqual((pieces[0].left, pieces[-1].right), (left, right))
    for first, second in zip(pieces, pieces[1:], strict=False):
      self.assertEqual(first.right, second.left)
    collars = [piece for piece in pieces if piece.joins]
    self.assertEqual(len(collars), 1)
    self.assertEqual({join.kind for join in collars[0].joins}, {"year", "model"})
    self.assertLess(collars[0].left, centre)
    self.assertGreater(collars[0].right, centre)

  def test_partition_budget_failure_does_not_return_a_partial_cover(self):
    centre = midnight_jd(2005, 1, 1)
    with self.assertRaises(ArithmeticError):
      partition_clock_interval(centre - F(1, 4), centre + F(1, 4), F(1, 2**40), 1)
