# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Check radial lower bounds, complete light-time tubes and accepted fixed-point transport."""

import math
import sys
import unittest
from fractions import Fraction as F
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "statistics"))

from altitude_bounds.global_vsop_certificate import whole_tier_certificate  # noqa: E402
from altitude_bounds.global_vsop_geometry import geometry_on_retardation_band, source_radius_lower  # noqa: E402
from altitude_bounds.light_time_majorants import LIGHT_TIME  # noqa: E402
from altitude_bounds.local_apparent_value import local_rounded_geometry  # noqa: E402
from altitude_bounds.local_vector_separation import norm_enclosure, raw_cartesian_enclosure, separation_for_reserve  # noqa: E402
from altitude_bounds.rational_intervals import Interval as I  # noqa: E402
from altitude_bounds.uniform_apparent_error import accepted_fixed_point_errors  # noqa: E402


class SourceGeometryTest(unittest.TestCase):
  def test_radius_lower_keeps_constant_terms_and_subtracts_all_variable_terms(self):
    tables = {"R0": ((F(100_000_000), F(0), F(0)), (F(1_000_000), F(1), F(2))), "R1": ((F(2_000_000), F(0), F(0)),)}
    self.assertEqual(source_radius_lower(tables, F(1, 10)), 1 - F(1, 100) - F(1, 500))
    with self.assertRaises(ValueError):
      source_radius_lower(tables, F(-1))
    with self.assertRaises(ValueError):
      source_radius_lower({"R0": ((F(1), F(0), F(1)),)}, F(1))

  def test_raw_cartesian_source_enclosure_preserves_scale_and_horner_powers(self):
    tables = {
      "L0": ((F(0), F(0), F(0)),),
      "B0": ((F(0), F(0), F(0)),),
      "R0": ((F(1000), F(0), F(0)),),
      "R1": ((F(200), F(0), F(0)),),
    }
    point = raw_cartesian_enclosure(tables, F(2451545) + F(365250, 8))
    self.assertLessEqual(point[0].lo, F(41, 4_000_000))
    self.assertGreaterEqual(point[0].hi, F(41, 4_000_000))
    self.assertEqual((point[1].lo, point[1].hi, point[2].lo, point[2].hi), (0, 0, 0, 0))
    self.assertLess(point[0].hi - point[0].lo, F(1, 2**180))
    tables["R2"] = tables.pop("R1")
    with self.assertRaisesRegex(ValueError, "consecutive"):
      raw_cartesian_enclosure(tables, F(2451545))

  def test_interval_norm_encloses_exact_three_four_five_triangle(self):
    norm = norm_enclosure((I.enclose(3), I.enclose(4), I.enclose(0)))
    self.assertEqual((norm.lo, norm.hi), (5, 5))

  def test_local_tube_charges_both_time_arguments_and_complete_delay(self):
    point = (I.enclose(4), I.enclose(3), I.enclose(0))
    result = separation_for_reserve(point, (F(10), F(1)), (F(2), F(2)), F(2451545), F(1, 8), F(1), F(1, 16))
    self.assertGreaterEqual(result["emission_speed_au_per_day"], 1)
    self.assertGreaterEqual(result["observation_speed_au_per_day"], 2)
    self.assertEqual(
      result["tube_radius_au"],
      result["emission_speed_au_per_day"] * F(3, 16) + result["observation_speed_au_per_day"] * F(1, 8),
    )
    self.assertEqual(result["radial_lower_au"], 5 - result["tube_radius_au"])
    self.assertEqual(result["emission_map_contraction_upper"], LIGHT_TIME * result["emission_speed_au_per_day"])
    self.assertGreater(result["radial_lower_au"], 0)

  def test_uncovered_unseparated_or_noncontractive_rectangles_fail(self):
    centre = (I.enclose(1), I.enclose(0), I.enclose(0))
    for target, earth, half, radius, delay in (
      ((F(1), F(1)), (F(1), F(1)), F(1), F(1), F(1)),
      ((F(1), F(1000)), (F(1), F(0)), F(0), F(1), F(0)),
      ((F(1), F(0)), (F(1), F(0)), F(1), F(1, 1_000_000), F(0)),
    ):
      with self.subTest(target=target, half=half), self.assertRaises(ValueError):
        separation_for_reserve(centre, target, earth, F(2451545), half, radius, delay)

  def test_region_composition_rejects_mismatched_source_radii_before_evaluation(self):
    target, earth = {"tau_radius_exact": "1"}, {"tau_radius_exact": "2"}
    with self.assertRaises(ValueError):
      geometry_on_retardation_band(target, earth, {}, {}, F(1, 2))
    with self.assertRaises(ValueError):
      local_rounded_geometry(target, earth, F(2451545), F(1, 8), (), "")
    with self.assertRaises(ValueError):
      whole_tier_certificate(target, earth, {"source_tau_radius": "1"}, "", "", {})


class FixedPointTransportTest(unittest.TestCase):
  def test_accepted_estimate_includes_observation_ulp_and_contraction_denominator(self):
    uniform = {
      "rounded_map_error_upper_days": F(1, 2**30),
      "vector_error_upper_au": F(1, 2**20),
      "distance_error_upper_au": F(1, 2**21),
    }
    certificate = {"emission_map_contraction_upper": F(1, 2), "emission_speed_au_per_day": F(3)}
    result = accepted_fixed_point_errors(uniform, certificate, F(2451545), F(2451546))
    ulp = F(math.ulp(2451546.0))
    emission = 2 * (ulp + uniform["rounded_map_error_upper_days"])
    self.assertEqual(result["observation_ulp_upper_days"], ulp)
    self.assertEqual(result["emission_error_upper_days"], emission)
    self.assertEqual(result["fixed_point_vector_error_upper_au"], uniform["vector_error_upper_au"] + 3 * emission)
    self.assertEqual(result["fixed_point_distance_error_upper_au"], uniform["distance_error_upper_au"] + 3 * emission)

  def test_fixed_point_transport_rejects_a_noncontraction_or_reversed_interval(self):
    with self.assertRaises(ValueError):
      accepted_fixed_point_errors({}, {"emission_map_contraction_upper": F(1)}, F(2451545), F(2451546))
    with self.assertRaises(ValueError):
      accepted_fixed_point_errors({}, {"emission_map_contraction_upper": F(0)}, F(2451546), F(2451545))
