# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Refraction support, units, monotonicity, source binding and directed inverse certificates."""

import sys
import unittest
from fractions import Fraction as F
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "statistics"))
from altitude_bounds.refraction_value_error import (  # noqa: E402
  derive,
  refraction_map_interval,
  refraction_value_error,
  source_parameters,
  weather_factor_error,
)
from altitude_bounds.raw_evaluation_error import SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF  # noqa: E402


SOURCE = (ROOT / "src/astro/earth/refraction.hpp").read_text()
TOOLBOX = (ROOT / "src/astro/toolbox.hpp").read_text()


class RefractionValueErrorTest(unittest.TestCase):
  def test_weather_magnitude_covers_the_whole_support(self):
    proof = weather_factor_error()
    self.assertGreaterEqual(proof.magnitude, F(1100, 1010) * F(283, 183))
    self.assertGreater(proof.error, 0)

  def test_bennett_inverse_map_has_slope_at_least_one(self):
    proof = refraction_value_error("bennett", SOURCE)
    self.assertEqual(proof["input_interval_degrees"], (F(0), F(90)))
    self.assertEqual(proof["map_derivative_lower"], 1)
    self.assertGreater(proof["phase_derivative_lower"], F(3, 5))

  def test_saemundsson_forward_map_remains_strictly_increasing(self):
    proof = refraction_value_error("saemundsson", SOURCE)
    self.assertEqual(proof["input_interval_degrees"], (F(-3, 2), F(90)))
    self.assertGreater(proof["map_derivative_lower"], F(1, 10))
    self.assertLess(proof["map_derivative_lower"], 1)

  def test_reciprocal_tangent_does_not_require_a_nonzero_cosine_lower(self):
    for model in ("bennett", "saemundsson"):
      with self.subTest(model=model):
        proof = refraction_value_error(model, SOURCE)
        self.assertLess(proof["phase_interval_degrees"][0], 90)
        self.assertGreater(proof["phase_interval_degrees"][1], 90)
        self.assertGreater(proof["stored_sine_lower"], 0)
        self.assertLess(proof["tangent_relative_error_upper"], F(1, 10**10))

  def test_arcminutes_are_converted_before_weather_scaling(self):
    proof = refraction_value_error("saemundsson", SOURCE)
    self.assertLess(proof["native_degrees"].magnitude, 1)
    self.assertGreater(proof["native_degrees"].error, 0)
    self.assertGreater(proof["corrected_degrees"].error, proof["native_degrees"].error)

  def test_native_value_magnitude_covers_independent_exact_map_endpoints(self):
    for name in ("bennett", "saemundsson"):
      proof = refraction_value_error(name, SOURCE)
      for altitude in proof["input_interval_degrees"]:
        mapped = refraction_map_interval(name, altitude, F(10), F(1010), SOURCE)
        correction = mapped - altitude
        self.assertGreaterEqual(proof["native_degrees"].magnitude, max(abs(correction.lo), abs(correction.hi)))

  def test_model_validation_does_not_fall_through_to_saemundsson(self):
    with self.assertRaises(ValueError):
      refraction_value_error("unknown", SOURCE)

  def test_bennett_apparent_ninety_is_outside_the_geometric_ninety_image(self):
    for temperature, pressure in ((F(-90), F(1100)), (F(60), F(300))):
      self.assertGreater(refraction_map_interval("bennett", F(90), temperature, pressure, SOURCE).lo, 90)
      self.assertLess(refraction_map_interval("bennett", F(0), temperature, pressure, SOURCE).hi, 0)

  def test_saemundsson_apparent_ninety_is_not_attainable(self):
    self.assertLess(refraction_map_interval("saemundsson", F(90), F(10), F(1010), SOURCE).hi, 90)

  def test_saemundsson_analysis_brackets_the_horizon_for_extreme_weather(self):
    for temperature in (F(-90), F(60)):
      for pressure in (F(300), F(1100)):
        self.assertLess(refraction_map_interval("saemundsson", F(-3, 2), temperature, pressure, SOURCE).hi, 0)
        self.assertGreater(refraction_map_interval("saemundsson", F(0), temperature, pressure, SOURCE).lo, 0)

  def test_exact_map_rejects_unsupported_weather_and_analysis_input(self):
    for args in (("bennett", F(-1), F(10), F(1010)), ("saemundsson", F(0), F(61), F(1010))):
      with self.assertRaises(ValueError):
        refraction_map_interval(*args, SOURCE)

  def test_current_stored_coefficients_are_read_from_the_source(self):
    self.assertEqual(source_parameters("bennett", SOURCE)[:3], (F(1), F(7.31), F(4.4)))
    self.assertEqual(source_parameters("saemundsson", SOURCE)[:3], (F(1.02), F(10.3), F(5.11)))
    changed = SOURCE.replace("h_deg + 4.4", "h_deg + 4.5")
    self.assertEqual(source_parameters("bennett", changed)[2], F(4.5))
    self.assertNotEqual(derive(changed, TOOLBOX)["proofs"]["bennett"], derive(SOURCE, TOOLBOX)["proofs"]["bennett"])

  def test_unknown_operation_order_units_and_weather_do_not_keep_the_old_proof(self):
    for old, new in (
      ("7.31 / denominator", "7.31 * denominator"),
      ("return r_native * factor;", "return r_native / factor;"),
      ("273.0 + params.temperature_c", "273.0 - params.temperature_c"),
      ("NATIVE_PRESSURE_HPA = 1010.0", "NATIVE_PRESSURE_HPA = 1000.0"),
      ("params.temperature_c <= 60.0", "params.temperature_c <= 90.0"),
      ("enum class Model : uint8_t { BENNETT, SAEMUNDSSON }", "enum class Model : uint8_t { SAEMUNDSSON, BENNETT }"),
    ):
      with self.subTest(expression=old), self.assertRaises(ValueError):
        derive(SOURCE.replace(old, new), TOOLBOX)
    for old, new in (
      ("MIN_PER_DEG = 60;", "MIN_PER_DEG = 600;"),
      ("return deg / DEG_PER_RAD;", "return deg * DEG_PER_RAD;"),
      ("return Angle<Unit> { _value * other };", "return Angle<Unit> { _value / other };"),
      ("return as<AngleUnit::RAD>();", "return as<AngleUnit::DEG>();"),
    ):
      with self.subTest(expression=old), self.assertRaises(ValueError):
        derive(SOURCE, TOOLBOX.replace(old, new))

  def test_comments_and_spacing_do_not_change_the_source_expressions(self):
    changed = SOURCE.replace("h_deg + 4.4", "h_deg /* phase offset */ +\n 4.4")
    self.assertEqual(derive(changed, TOOLBOX), derive(SOURCE, TOOLBOX))

  def test_duplicate_model_expressions_are_rejected(self):
    with self.assertRaises(ValueError):
      source_parameters("bennett", SOURCE + SOURCE)

  def test_serialized_errors_round_up_and_slope_rounds_down(self):
    proof = derive(SOURCE, TOOLBOX)
    for certificate in proof["certificates"]:
      model = proof["proofs"][certificate["model"]]
      residual = model["map"].error + UNIT_ROUNDOFF * (model["map"].magnitude + 90) + SUBNORMAL_ALLOWANCE
      self.assertGreaterEqual(certificate["map_error"], model["map"].error)
      self.assertGreaterEqual(certificate["residual_error"], residual)
      self.assertLessEqual(certificate["slope_lower"], model["map_derivative_lower"])
      self.assertGreater(certificate["slope_lower"], 0)
      for field in ("map_error", "residual_error", "slope_lower"):
        self.assertEqual(certificate[field], F(float(certificate[field])))
