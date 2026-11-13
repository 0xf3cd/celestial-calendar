# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Exercise source parsing, exact derivative algebra and directed arithmetic certificates."""

import math
import sys
import unittest
from fractions import Fraction as F
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "statistics"))

from altitude_bounds.coefficient_reader import PLANETS, parse_coefficients, read_coefficients  # noqa: E402
from altitude_bounds.floating_operation_bounds import FloatingBound  # noqa: E402
from altitude_bounds.heliocentric_evaluation_error import time_argument_error  # noqa: E402
from altitude_bounds.light_time_majorants import compose_bounds, distance_bounds  # noqa: E402
from altitude_bounds.poisson_majorants import array_rows, pi_interval, polynomial_bounds  # noqa: E402
from altitude_bounds.rational_intervals import PI, Interval, sincos  # noqa: E402
from altitude_bounds.raw_evaluation_error import SUBNORMAL_ALLOWANCE, coordinate_error  # noqa: E402
from altitude_bounds.series_bounds import moment_check, outward, product_bounds, rotation_bounds  # noqa: E402
from altitude_bounds.spherical_coordinate_majorants import argument_bounds  # noqa: E402
from altitude_bounds.vector_derivative_majorants import norm_bounds, reciprocal_bounds  # noqa: E402
from altitude_bounds.vsop_derivative_majorants import coordinate_derivative_bounds  # noqa: E402


def header(row="{0.1, 0.2, 0.3,},"):
  declarations = []
  for family in "LBR":
    declarations.extend(
      (
        f"inline constexpr std::array<Coefficients, 1> {family}0 {{{{ {row} }}}};",
        f"inline constexpr std::array<Vsop87dTable, 1> {family}_ARRAY {{ {family}0, }};",
        f"inline constexpr Vsop87dTables {family} {{ {family}_ARRAY }};",
      )
    )
  return "\n".join(
    (
      '#pragma once\n#include <array>\n#include "defines.hpp"',
      "namespace astro::vsop87d::earth_coeff {",
      *(f"using {name} = astro::vsop87d::{name};" for name in ("Coefficients", "Vsop87dTable", "Vsop87dTables")),
      *declarations,
      "}\nnamespace astro::vsop87d { template <> struct PlanetTables<Planet::EAR> {",
      *(f"static const inline Vsop87dTables& {family} = earth_coeff::{family};" for family in "LBR"),
      "}; }",
    )
  )


class CoefficientReaderTest(unittest.TestCase):
  def test_literals_are_exact_stored_binary64_not_decimal_rationals(self):
    tables = parse_coefficients(header())
    self.assertEqual(tables["L0"][0][0], F(3602879701896397, 36028797018963968))
    self.assertNotEqual(tables["L0"][0][0], F("0.1"))
    self.assertEqual(list(tables), ["L0", "B0", "R0"])

  def test_signed_exponents_and_optional_trailing_commas(self):
    term = parse_coefficients(header("{-1.25e-2, +4E2, -.5}"))["L0"][0]
    self.assertEqual(term, tuple(F(float(value)) for value in ("-1.25e-2", "+4E2", "-.5")))

  def test_incomplete_or_extra_initializer_tokens_are_rejected(self):
    for row in (
      "{1.0, 2.0}",
      "{1.0, 2.0, 3.0, 4.0}",
      "{1.0 2.0, 3.0}",
      "{0x1p0, 2.0, 3.0}",
      "{1.0, 2.0, 3.0}, garbage",
      "{1.0, 2.0, 3.0},,",
      "{1.0, 2.0, 3.0 + 4.0}",
    ):
      with self.subTest(row=row), self.assertRaises(ValueError):
        parse_coefficients(header(row))

  def test_counts_mappings_duplicates_and_exports_are_checked(self):
    original = header()
    for text in (
      original.replace("Coefficients, 1> L0", "Coefficients, 0> L0"),
      original.replace("Coefficients, 1> L0", "Coefficients, 2> L0"),
      original.replace("L_ARRAY { L0, }", "L_ARRAY { B0, }"),
      original.replace("Planet::EAR", "Planet::MER"),
      original.replace("L = earth_coeff::L", "L = earth_coeff::B"),
      original.replace("using Coefficients", "using Vsop87dTable = astro::vsop87d::Vsop87dTable; using Coefficients"),
      original + "\ntrailing_garbage",
    ):
      with self.subTest(text=text), self.assertRaises(ValueError):
        parse_coefficients(text)

  def test_nonfinite_literals_are_rejected(self):
    with self.assertRaisesRegex(ValueError, "non-finite"):
      parse_coefficients(header("{1e9999, 0.0, 0.0}"))

  def test_all_eight_actual_headers_have_complete_ordered_families(self):
    for name in PLANETS:
      with self.subTest(planet=name):
        tables = read_coefficients(ROOT / "src/astro/vsop87d" / (name + "_coeff.hpp"))
        for family in "LBR":
          names = [key for key in tables if key[0] == family]
          self.assertEqual(names, [f"{family}{power}" for power in range(len(names))])
          self.assertTrue(names)
        self.assertTrue(all(isinstance(value, F) for terms in tables.values() for term in terms for value in term))

  def test_numeric_array_reader_rejects_unconsumed_tokens_and_wrong_counts(self):
    source = "inline constexpr std::array<Row, 1> X {{ {1.0, 2.0} }};"
    self.assertEqual(array_rows(source, "X", "Row", 1, 2), ((F(1), F(2)),))
    for changed in (source.replace("1>", "2>"), source.replace("{1.0, 2.0}", "{1.0, 2.0}, garbage")):
      with self.subTest(source=changed), self.assertRaises(ValueError):
        array_rows(changed, "X", "Row", 1, 2)


class DerivativeAlgebraTest(unittest.TestCase):
  def test_raw_term_and_independent_moment_routes_match_exact_symbolic_values(self):
    tables = {0: ((F(3), F(123), F(2)),), 1: ((F(5), F(-10), F(7)),)}
    expected = tuple(F(value, 100_000_000 * 365250**order) for order, value in enumerate((13, 81, 572, 4189)))
    self.assertEqual(coordinate_derivative_bounds(tables, F(2), 3), expected)
    self.assertEqual(moment_check(tables, F(2), 3), expected)

  def test_product_rotation_norm_and_reciprocal_rules(self):
    self.assertEqual(product_bounds((F(1), F(2), F(3)), (F(4), F(5), F(6))), (4, 13, 38))
    self.assertEqual(rotation_bounds((F(0), F(2), F(3))), (1, 2, 7))
    self.assertEqual(norm_bounds((F(10), F(2), F(3)), F(5)), (10, 2, F(19, 5)))
    self.assertEqual(reciprocal_bounds((F(10), F(2), F(3)), F(5)), (F(1, 5), F(2, 25), F(23, 125)))

  def test_polynomial_composition_preserves_cross_terms_and_time_units(self):
    self.assertEqual(compose_bounds((F(2), F(3), F(4)), (F(0), F(5), F(6))), (2, 15, 118))
    self.assertEqual(polynomial_bounds((F(1), F(2), F(3)), F(2), F(10), 2), (17, F(7, 5), F(3, 50)))

  def test_missing_separation_and_wrong_derivative_shapes_are_rejected(self):
    for call in (
      lambda: norm_bounds((F(1), F(2)), F(0)),
      lambda: reciprocal_bounds((F(1), F(2)), F(2)),
      lambda: argument_bounds((F(1), F(2)), F(0)),
      lambda: product_bounds((F(1),), (F(1), F(2))),
      lambda: compose_bounds((F(-1),), (F(0),)),
      lambda: coordinate_error({1: ((F(1), F(2), F(3)),)}, F(1)),
    ):
      with self.subTest(call=call), self.assertRaises(ValueError):
        call()

  def test_implicit_light_time_bounds_require_strict_contraction(self):
    def identity(left, right):
      return (F(1),) + (F(0),) * (len(left) - 1)

    bounded, audits = distance_bounds((F(4), F(1), F(1, 2)), (F(2), F(1, 10), F(1, 10)), F(0), F(1, 2), identity)
    self.assertEqual(len(bounded), 3)
    self.assertEqual([row["order"] for row in audits], [1, 2])
    self.assertTrue(all(0 <= float.fromhex(row["slope_hex"]) < 1 for row in audits))
    with self.assertRaisesRegex(ValueError, "strict light-time derivative contraction"):
      distance_bounds((F(4), F(2_000_000)), (F(2), F(1)), F(0), F(1, 2), identity)


class DirectedArithmeticTest(unittest.TestCase):
  def test_pi_interval_fits_an_independent_fifty_decimal_digit_bracket(self):
    lower, upper = pi_interval()
    printed = F("3.14159265358979323846264338327950288419716939937510")
    self.assertLess(printed, lower)
    self.assertLess(upper, printed + F(1, 10**50))

  def test_sqrt_and_division_enclose_their_exact_inputs(self):
    root = Interval.enclose(2).sqrt()
    self.assertLessEqual(root.lo**2, 2)
    self.assertGreaterEqual(root.hi**2, 2)
    quotient = Interval.enclose(1, 2) / Interval.enclose(3, 4)
    self.assertLessEqual(quotient.lo, F(1, 4))
    self.assertGreaterEqual(quotient.hi, F(2, 3))
    with self.assertRaisesRegex(ValueError, "containing zero"):
      Interval.enclose(1) / Interval.enclose(-1, 1)

  def test_taylor_reduction_encloses_quadrant_extrema(self):
    sine, cosine = sincos(Interval.enclose(0))
    self.assertEqual((sine.lo, sine.hi, cosine.lo, cosine.hi), (0, 0, 1, 1))
    sine, cosine = sincos(PI / 2)
    self.assertLessEqual(sine.lo, 1)
    self.assertGreaterEqual(sine.hi, 1)
    self.assertLessEqual(cosine.lo, 0)
    self.assertGreaterEqual(cosine.hi, 0)
    with self.assertRaises(ValueError):
      sincos(Interval.enclose(-10, 10))

  def test_outward_binary64_serialization_uses_the_least_upper_neighbour(self):
    for value in (F(1, 10), F(1, 3)):
      with self.subTest(value=value):
        field = outward(value)
        upper = float.fromhex(field["hex"])
        self.assertGreaterEqual(F(upper), value)
        self.assertLess(F(math.nextafter(upper, -math.inf)), value)
    with self.assertRaises(ValueError):
      outward(F(-1))

  def test_floating_operation_bounds_keep_underflow_and_argument_premises(self):
    product = FloatingBound(F(2), F(1, 100)) * FloatingBound(F(3), F(1, 50))
    self.assertGreater(product.error, F(7, 100) + F(1, 5000))
    self.assertGreaterEqual(time_argument_error(F(2451545)), SUBNORMAL_ALLOWANCE)
    with self.assertRaises(ValueError):
      FloatingBound(F(2**41)).sine_or_cosine()
    with self.assertRaises(ValueError):
      time_argument_error(F(1, 10))
