# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Check height-dependent allowances, pole identities and observer separation requirements."""

import sys
import unittest
from fractions import Fraction as F
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "statistics"))

from altitude_bounds.affine_observer_error import Affine, GRID, HeightBound, affine_observer_error  # noqa: E402
from altitude_bounds.altitude_objective_majorants import objective_bounds  # noqa: E402
from altitude_bounds.equatorial_evaluation_error import equatorial_error, rotated_axial_lower  # noqa: E402
from altitude_bounds.floating_operation_bounds import FloatingBound as B  # noqa: E402
from altitude_bounds.observer_evaluation_error import local_geometry_error, local_projection_error, positive_sqrt  # noqa: E402
from altitude_bounds.observer_geometry_majorants import projected_objective_bounds  # noqa: E402
from altitude_bounds.observer_objective_derivatives import observer_objective_derivatives, reference_site_bounds  # noqa: E402
from altitude_bounds.uniform_observer_error import uniform_observer_error  # noqa: E402


CONSTANTS = (F(2), F(1), F(100))
STATIONARY = (F(1), F(0), F(0), F(0))
ROTATION = (F(0), F(2), F(3), F(4))
ZERO = (F(0),) * 4


class HeightAllowanceTest(unittest.TestCase):
  def test_affine_products_keep_both_height_cross_terms(self):
    self.assertEqual(Affine(F(2), F(3)) * Affine(F(5)), Affine(F(10), F(15)))
    self.assertEqual(Affine(F(5)) * Affine(F(2), F(3)), Affine(F(10), F(15)))
    self.assertEqual((Affine(F(2), F(3)) + F(4))(F(1, 2)), F(15, 2))
    with self.assertRaises(ValueError):
      Affine(F(1), F(1)) * Affine(F(1), F(1))
    with self.assertRaises(ValueError):
      Affine()(F(-1))

  def test_affine_grid_enclosure_keeps_fractional_height_rounding_reserve(self):
    exact = Affine(F(1), F(1))
    enclosed = exact.enclose_grid()
    self.assertEqual(enclosed.constant, 1 + GRID)
    self.assertEqual(enclosed.height, 1)
    for height in (F(0), F(1, 3), F(1_000_000)):
      with self.subTest(height=height):
        self.assertGreaterEqual(enclosed(height), exact(height))
    with self.assertRaises(ValueError):
      Affine(F(1), F(-1))

  def test_height_arithmetic_keeps_error_cross_products_and_positive_divisors(self):
    left = HeightBound(Affine(F(2), F(3)), Affine(F(1, 10), F(1, 5)))
    right = HeightBound.fixed(B(F(4), F(1, 20)))
    product = left * right
    expected = left.magnitude * right.error + right.magnitude * left.error + left.error * right.error
    self.assertGreaterEqual(product.error(F(1, 2)), expected(F(1, 2)))
    self.assertGreater(product.error(F(1, 2)), (left.magnitude * right.error + right.magnitude * left.error)(F(1, 2)))
    with self.assertRaises(ValueError):
      left / 0

  def test_affine_observer_residual_grows_with_absolute_height(self):
    result = affine_observer_error(B(F(45)), F(1, 2**40), F(1, 2**30), B(F(1), F(1, 2**40)), F(1, 2), CONSTANTS)
    error = result["residual"].error
    self.assertGreater(error.constant, 0)
    self.assertGreater(error.height, 0)
    self.assertGreater(error(F(10)), error(F(0)))


class ObserverDerivativeTest(unittest.TestCase):
  def test_reference_sites_keep_exact_poles_and_equator(self):
    for latitude in (F(-90), F(90)):
      with self.subTest(latitude=latitude):
        site = reference_site_bounds((latitude, F(0), F(0)), CONSTANTS)
        self.assertEqual(
          site, {"equatorial_upper": 0, "polar_upper": 1, "cos_latitude_upper": 0, "sin_latitude_upper": 1}
        )
    self.assertEqual(
      reference_site_bounds((F(0), F(0), F(0)), CONSTANTS),
      {"equatorial_upper": 1, "polar_upper": 0, "cos_latitude_upper": 1, "sin_latitude_upper": 0},
    )

  def test_projection_does_not_add_daily_rotation_to_the_untranslated_unit_norm(self):
    result = projected_objective_bounds(STATIONARY, ROTATION, ZERO, F(1), F(0), F(1), F(0), F(1), F(1))
    self.assertEqual(result["norm_derivatives"], (0, 0, 0))
    self.assertEqual(result["up_derivatives"], (2, 7, 30))
    self.assertEqual(result["objective_derivatives"], (2, 7, 30))

  def test_polar_projection_removes_daily_up_rotation(self):
    result = projected_objective_bounds(STATIONARY, ROTATION, ZERO, F(0), F(1), F(0), F(1), F(1), F(1))
    self.assertEqual(result["up_derivatives"], (0, 0, 0))
    self.assertEqual(result["objective_derivatives"], (0, 0, 0))

  def test_generic_objective_keeps_squared_and_cubic_norm_terms(self):
    self.assertEqual(objective_bounds((F(2), F(3), F(5), F(7)), F(1), F(1)), (6, 19, 140))
    with self.assertRaises(ValueError):
      objective_bounds((F(1), F(2), F(3), F(4)), F(2), F(0))
    with self.assertRaises(ValueError):
      objective_bounds(STATIONARY, F(0), F(0))

  def test_composed_derivatives_keep_the_tighter_projection_at_poles(self):
    result = observer_objective_derivatives(
      STATIONARY, STATIONARY, ROTATION, F(1), F(1, 2), (F(90), F(0), F(0)), CONSTANTS, F(1)
    )
    self.assertEqual(result["objective_derivatives"], (0, 0, 0))
    self.assertTrue(any(value > 0 for value in result["generic_objective_derivatives"]))
    with self.assertRaises(ValueError):
      observer_objective_derivatives((F(2), *ZERO[1:]), STATIONARY, ROTATION, F(1), F(1), (F(0),) * 3, CONSTANTS, F(0))


class ObserverValueTest(unittest.TestCase):
  def test_positive_sqrt_transports_error_across_the_whole_positive_segment(self):
    result = positive_sqrt(B(F(4), F(1, 4)), F(4))
    self.assertGreater(result.error, F(1, 4) / 4)
    with self.assertRaises(ValueError):
      positive_sqrt(B(F(4), F(1)), F(1))

  def test_projection_can_report_lost_separation_but_geometry_must_fail(self):
    arguments = (B(F(0)), B(F(0)), F(0), F(0), B(F(1, 1000)), F(1, 1000), *CONSTANTS)
    projected = local_projection_error(*arguments)
    self.assertLess(projected["reference_q_lower"], 0)
    with self.assertRaisesRegex(ValueError, "positive local separation"):
      local_geometry_error(*arguments)

  def test_equatorial_reconstruction_retains_pole_fallback_and_axial_separation(self):
    lower = rotated_axial_lower(F(1), F(0), F(1, 2))
    self.assertEqual(lower, F(7, 8))
    self.assertEqual(rotated_axial_lower(F(0), F(0), F(1, 2)), 0)
    fallback = equatorial_error(B(F(360)), B(F(0)), B(F(24)))
    separated = equatorial_error(B(F(360)), B(F(0)), B(F(24)), F(1, 2))
    self.assertLess(separated["stored_angle_direction_error"], fallback["stored_angle_direction_error"])
    with self.assertRaises(ValueError):
      equatorial_error(B(F(0)), B(F(0)), B(F(0)), F(2))

  def test_whole_region_composition_rejects_mismatched_provider_or_clock_coverage(self):
    apparent = {"planet": "mars", "analysis_interval_exact": ("2451545", "2451546")}
    equatorial = {"planet": "venus", "analysis_interval_exact": ("2451545", "2451546")}
    clock = {"tt_interval": (F(2451545), F(2451546))}
    with self.assertRaisesRegex(ValueError, "identical full TT envelope"):
      uniform_observer_error(apparent, equatorial, clock, (F(0),) * 3, "", CONSTANTS)
