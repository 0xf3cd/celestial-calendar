# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Check body-specific powers, nested fixed points and retained source-domain boundaries."""

import sys
import unittest
from fractions import Fraction as F
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "statistics"))

from altitude_bounds.conditional_source_derivatives import solar_variation_derivatives  # noqa: E402
from altitude_bounds.coefficient_reader import read_coefficients  # noqa: E402
from altitude_bounds.floating_operation_bounds import FloatingBound as B  # noqa: E402
from altitude_bounds.inverse_fk5_error import accepted_updated_error, fk5_map_lipschitz, inverse_fk5_error  # noqa: E402
from altitude_bounds.lunar_evaluation_error import (  # noqa: E402
  lunar_context_error,
  lunar_eccentricity_powers,
  lunar_evaluation_error,
)
from altitude_bounds.other_body_tiers import other_body_tier  # noqa: E402
from altitude_bounds.pluto_apparent_error import pluto_apparent_error  # noqa: E402
from altitude_bounds.pluto_provider_error import pluto_provider_error, stored_degrees  # noqa: E402
from altitude_bounds.pluto_raw_evaluation_error import RAW_END_JDE_TT, RAW_START_JDE_TT, pluto_raw_evaluation_error  # noqa: E402
from altitude_bounds.pluto_smooth_majorants import inverse_fk5_derivative_bounds, pluto_distance_bounds  # noqa: E402
from altitude_bounds.poisson_majorants import stored  # noqa: E402
from altitude_bounds.solar_evaluation_error import solar_power_error, solar_variation_error  # noqa: E402
from altitude_bounds.source_c2_records import derive_vsop_record  # noqa: E402
from altitude_bounds.source_reading import correction_expressions  # noqa: E402


class SolarLunarBoundsTest(unittest.TestCase):
  def test_solar_power_zero_is_exact_and_power_one_retains_its_elementary_allowance(self):
    argument = B(F(2), F(1, 100))
    self.assertEqual(solar_power_error(argument, 0), B(F(1)))
    self.assertGreater(solar_power_error(argument, 1).error, argument.error)
    self.assertGreater(solar_power_error(argument, 2).error, 4 * argument.error)
    with self.assertRaises(ValueError):
      solar_power_error(argument, 4)

  def test_solar_variation_keeps_the_constant_and_rejects_fractional_powers(self):
    self.assertEqual(solar_variation_derivatives((), F(1)), (stored("3548.330"), 0, 0, 0))
    with self.assertRaises(ValueError):
      solar_variation_derivatives(((F(1), F(0), F(1), F(1, 2)),), F(1))

  def test_actual_solar_variation_consumes_all_twenty_one_source_terms(self):
    source = (ROOT / "src/astro/earth.hpp").read_text()
    result = solar_variation_error(B(F(1, 10)), source)
    self.assertGreater(result.magnitude, stored("3548.330"))
    self.assertGreater(result.error, 0)
    changed = source.replace("std::array<DailyVariationTerm, 21>", "std::array<DailyVariationTerm, 20>")
    self.assertNotEqual(changed, source)
    with self.assertRaises(ValueError):
      solar_variation_error(B(F(1, 10)), changed)

  def test_lunar_context_reads_all_eight_arguments_and_eccentricity(self):
    source = (ROOT / "src/astro/elp2000_82b.hpp").read_text()
    result = lunar_context_error(B(F(1, 10)), source)
    self.assertEqual(set(result), {"E", "Lp", "D", "M", "Mp", "F", "A1", "A2", "A3"})
    self.assertTrue(all(value.error > 0 for value in result.values()))
    powers = lunar_eccentricity_powers(result["E"])
    self.assertEqual(powers[0], B(F(1)))
    self.assertGreater(powers[1].error, result["E"].error)

  def test_lunar_value_bounds_keep_au_conversion_and_positive_distance(self):
    earth = (ROOT / "src/astro/earth.hpp").read_text()
    lunar = (ROOT / "src/astro/elp2000_82b.hpp").read_text()
    result = lunar_evaluation_error(F(2451545), F(2451545) + F(1, 4), lunar, earth)
    expected = (B.literal(385000.56) + result["radius_series"] / 1000) / 149597870.700
    self.assertEqual(result["distance_au"], expected)
    self.assertGreater(result["reference_distance_lower_au"], 0)
    self.assertLess(result["reference_distance_lower_au"], result["distance_au"].magnitude)
    with self.assertRaises(ValueError):
      lunar_evaluation_error(F(2451546), F(2451545), lunar, earth)


class PlutoFixedPointTest(unittest.TestCase):
  def test_geocentric_cosine_minimizes_the_correlated_radius_ratio(self):
    path = ROOT / "src/astro/vsop87d/earth_coeff.hpp"
    earth = derive_vsop_record("earth", path, read_coefficients(path), F(21, 100))
    expressions = correction_expressions(
      (ROOT / "src/astro/planet.hpp").read_text(), (ROOT / "src/astro/coord_transform.hpp").read_text()
    )
    proof = pluto_apparent_error(
      F(2451545),
      F(2451546),
      earth,
      (ROOT / "src/astro/pluto.hpp").read_text(),
      (ROOT / "src/astro/earth/precession.hpp").read_text(),
      (ROOT / "src/astro/earth.hpp").read_text(),
      expressions,
    )
    raw = proof["provider"]["raw"]
    radius_lower = raw["reference_radius_lower_au"]
    radius_upper = raw["radius_au"].magnitude + raw["radius_au"].error
    cosine_lower = proof["provider"]["dynamic_reference_cosine_lower"]
    earth_upper = F(earth["cartesian_norm_derivatives_au"][0]["exact"])
    claimed = proof["geocentric_cosine_lower"]
    self.assertEqual(claimed, (radius_lower * cosine_lower - earth_upper) / (radius_lower + earth_upper))
    for radius in (radius_lower, (radius_lower + radius_upper) / 2, radius_upper):
      for earth_radius in (F(0), earth_upper / 2, earth_upper):
        with self.subTest(radius=radius, earth_radius=earth_radius):
          correlated = (radius * cosine_lower - earth_radius) / (radius + earth_radius)
          self.assertGreaterEqual(correlated, claimed)
          # Cross-multiplication retains the shared radius instead of varying two copies independently.
          numerator = (1 + cosine_lower) * (
            earth_upper * (radius - radius_lower) + radius_lower * (earth_upper - earth_radius)
          )
          self.assertEqual(correlated - claimed, numerator / ((radius + earth_radius) * (radius_lower + earth_upper)))

  def test_updated_iterate_uses_the_contracted_step_in_the_same_norm(self):
    self.assertEqual(accepted_updated_error(F(1, 8), F(1, 2), F(1, 4)), F(1, 2))
    for contraction in (F(-1), F(1)):
      with self.subTest(contraction=contraction), self.assertRaises(ValueError):
        accepted_updated_error(F(0), contraction, F(1, 4))

  def test_fk5_jacobian_increases_as_the_latitude_strip_approaches_a_pole(self):
    self.assertGreater(fk5_map_lipschitz(F(1, 4)), fk5_map_lipschitz(F(1, 2)))
    with self.assertRaises(ValueError):
      fk5_map_lipschitz(F(0))
    with self.assertRaises(ValueError):
      inverse_fk5_error(B(F(0)), F(0), B(F(60)), F(1), "1.397 * jc")

  def test_smooth_fk5_and_outer_light_time_require_independent_strict_contractions(self):
    with self.assertRaises(ValueError):
      inverse_fk5_derivative_bounds((F(1), F(1)), (F(1), F(1)), (F(1), F(1)), F(1), F(1, 10**10))
    with self.assertRaises(ValueError):
      pluto_distance_bounds({}, (F(1), F(1)), F(0), {}, {"emission_map_contraction_upper": F(1)})

  def test_radian_storage_error_includes_both_conversion_and_rounding(self):
    result = stored_degrees(B(F(1), F(1, 100)))
    self.assertGreater(result.magnitude, 57)
    self.assertGreater(result.error, F(57, 100))


class BodyDomainTest(unittest.TestCase):
  def test_pluto_raw_interval_is_closed_but_the_provider_window_ends_exclusively(self):
    source = (ROOT / "src/astro/pluto.hpp").read_text()
    result = pluto_raw_evaluation_error(F(2451545), F(2451545) + F(1, 4), source)
    self.assertGreater(result["reference_radius_lower_au"], 0)
    self.assertLess(result["reference_radius_lower_au"], result["radius_au"].magnitude)
    with self.assertRaises(ValueError):
      pluto_raw_evaluation_error(RAW_START_JDE_TT, RAW_END_JDE_TT, source)
    with self.assertRaises(ValueError):
      pluto_raw_evaluation_error(RAW_START_JDE_TT - 1, RAW_START_JDE_TT, source)

  def test_pluto_apparent_admission_preserves_the_one_day_emission_reserve(self):
    with self.assertRaises(ValueError):
      pluto_provider_error(RAW_START_JDE_TT, RAW_START_JDE_TT, RAW_START_JDE_TT, RAW_START_JDE_TT, "", "", "")

  def test_body_dispatch_rejects_unknown_targets_and_out_of_domain_pluto_before_io(self):
    for name, left, right in (
      ("mars", F(2451545), F(2451546)),
      ("pluto", RAW_START_JDE_TT, RAW_START_JDE_TT + 1),
      ("sun", F(2451546), F(2451545)),
    ):
      with self.subTest(name=name, left=left), self.assertRaises(ValueError):
        other_body_tier(name, ROOT, left, right, {}, {}, {}, ())
