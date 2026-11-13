# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Check source-expression restrictions and separated apparent-place error transport."""

import re
import sys
import unittest
from fractions import Fraction as F
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "statistics"))

from altitude_bounds.apparent_correction_error import degree_input_error, positive_quotient, tangent_bound  # noqa: E402
from altitude_bounds.apparent_correction_majorants import aberration_bounds, fk5_bounds, polynomial  # noqa: E402
from altitude_bounds.floating_operation_bounds import FloatingBound as B  # noqa: E402
from altitude_bounds.nutation_majorants import nutation_bounds, nutation_rows  # noqa: E402
from altitude_bounds.precession_evaluation_error import source_polynomial_bound  # noqa: E402
from altitude_bounds.retarded_angular_error import angular_allowance_from_errors  # noqa: E402
from altitude_bounds.source_reading import correction_expressions, serial, stored_constant  # noqa: E402


class SourceExpressionsTest(unittest.TestCase):
  def test_stored_constant_uses_binary64_operation_order(self):
    text = "inline constexpr double VALUE = (0.1 + scale) * (-2.0) / 3.0;"
    self.assertEqual(stored_constant(text, "VALUE", {"scale": 0.2}), (0.1 + 0.2) * (-2.0) / 3.0)
    self.assertEqual(stored_constant("inline constexpr double VALUE = 0.1 + 0.2;", "VALUE", {}), 0.1 + 0.2)

  def test_stored_constant_rejects_calls_powers_unknown_names_and_boolean_literals(self):
    for expression in ("call()", "2 ** 3", "unknown + 1", "True", "[1.0][0]"):
      with self.subTest(expression=expression), self.assertRaises(ValueError):
        stored_constant("inline constexpr double VALUE = " + expression + ";", "VALUE", {})
    with self.assertRaises(ValueError):
      stored_constant("inline constexpr double OTHER = 1.0;", "VALUE", {})

  def test_error_algebra_preserves_the_expression_tree(self):
    values = {"a": B(F(1)), "b": B(F(2)), "c": B(F(3))}
    left = source_polynomial_bound("a + b * c", values)
    right = source_polynomial_bound("(a + b) * c", values)
    self.assertEqual(left, values["a"] + values["b"] * values["c"])
    self.assertEqual(right, (values["a"] + values["b"]) * values["c"])
    self.assertNotEqual(left, right)
    with self.assertRaises(ValueError):
      source_polynomial_bound("a / b", values)
    with self.assertRaises(ValueError):
      source_polynomial_bound("call(a)", values)

  def test_correction_reader_captures_all_actual_source_expressions(self):
    source = ROOT / "src/astro"
    expressions = correction_expressions(
      (source / "planet.hpp").read_text(), (source / "coord_transform.hpp").read_text()
    )
    self.assertEqual(
      set(expressions),
      {
        "eccentricity",
        "perihelion_degrees",
        "fk5_drift_degrees",
        "annual_longitude_arcsec",
        "annual_latitude_arcsec",
        "fk5_longitude_arcsec",
        "fk5_latitude_arcsec",
      },
    )
    self.assertIn("0.016708634", expressions["eccentricity"])
    self.assertIn("0.03916", expressions["fk5_longitude_arcsec"])

  def test_serialization_preserves_nested_dataclass_fields_as_exact_rationals(self):
    value = B(F(1, 3), F(1, 100))
    self.assertEqual(
      serial({"values": (value,)}),
      {
        "values": [{"magnitude": str(value.magnitude), "error": str(value.error)}],
      },
    )


class CorrectionBoundsTest(unittest.TestCase):
  def test_positive_quotient_and_tangent_reject_unseparated_perturbation_segments(self):
    with self.assertRaises(ValueError):
      positive_quotient(B(F(1)), B(F(1), F(1, 2)), F(1, 2))
    with self.assertRaises(ValueError):
      tangent_bound(B(F(1), F(1, 4)), F(1, 4))
    with self.assertRaises(ValueError):
      degree_input_error(F(1), F(-1))
    good = positive_quotient(B(F(1)), B(F(2)), F(2))
    self.assertGreaterEqual(good.magnitude, F(1, 2))
    self.assertGreater(good.error, 0)

  def test_angular_transport_keeps_exact_separation_denominators(self):
    actual = angular_allowance_from_errors(F(1, 10), F(1, 20), F(10), F(1))
    for name, expected in (("longitude_input_transport_rad", F(1, 9)), ("latitude_input_transport_rad", F(3, 197))):
      self.assertGreaterEqual(actual[name], expected)
      self.assertLess(actual[name] - expected, F(1, 2**192))
    with self.assertRaises(ValueError):
      angular_allowance_from_errors(F(1), F(0), F(10), F(1))
    with self.assertRaises(ValueError):
      angular_allowance_from_errors(F(1, 10), F(9, 10), F(1), F(10))

  def test_polynomial_bound_keeps_leibniz_cross_terms(self):
    self.assertEqual(polynomial((F(1), F(-2), F(3)), (F(4), F(5), F(6))), (57, 130, 306))

  def test_constant_angle_sources_have_zero_correction_time_derivatives(self):
    argument = (F(1), F(0), F(0))
    clock = (F(0), F(0), F(0))
    annual = aberration_bounds(argument, argument, argument, clock, F(1))
    fk5 = fk5_bounds(argument, argument, clock, F(1))
    for record in (annual, fk5):
      for field in ("delta_longitude_rad", "delta_latitude_rad"):
        self.assertEqual(record[field][1:], (0, 0))
    with self.assertRaises(ValueError):
      aberration_bounds(argument, argument, argument, clock, F(0))
    with self.assertRaises(ValueError):
      fk5_bounds(argument, argument, clock, F(0))

  def test_nutation_reader_requires_the_complete_fixed_model_table(self):
    source = (ROOT / "src/astro/earth.hpp").read_text()
    self.assertEqual(len(nutation_rows(source)), 106)
    changed = re.sub(r"(std::array<NutationCoeffs,\s*)106", r"\g<1>107", source)
    self.assertNotEqual(changed, source)
    with self.assertRaises(ValueError):
      nutation_rows(changed)

  def test_zero_nutation_table_does_not_create_longitude_or_obliquity_terms(self):
    actual = nutation_bounds(((F(0),) * 9,), F(1), 2)
    self.assertEqual(actual["longitude_rad"], (0, 0, 0))
    self.assertEqual(actual["obliquity_rad"], (0, 0, 0))
    self.assertEqual(actual["equation_of_equinoxes_rad"], (0, 0, 0))
