# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Check directed packet encoding and complete canonical-quarter eligibility."""

import sys
import unittest
from fractions import Fraction as F
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "statistics"))

from altitude_bounds.certificate_encoding import MAX_EXPONENT, MIN_EXPONENT, ZERO, decode, encode  # noqa: E402
from altitude_bounds.local_packets import derive  # noqa: E402


class CertificateEncodingTest(unittest.TestCase):
  def test_zero_sentinel_is_distinct_from_all_positive_representable_fields(self):
    self.assertEqual(decode(ZERO), 0)
    self.assertEqual(encode(F(0), True), ZERO)
    self.assertEqual(encode(F(0), False), ZERO)
    self.assertEqual(decode(MIN_EXPONENT), F(1, 2**127))

  def test_every_nonzero_exponent_round_trips_exactly_in_both_directions(self):
    for exponent in range(MIN_EXPONENT, MAX_EXPONENT + 1):
      with self.subTest(exponent=exponent):
        self.assertEqual(encode(decode(exponent), True), exponent)
        self.assertEqual(encode(decode(exponent), False), exponent)

  def test_nonpowers_round_up_for_majorants_and_down_for_separation_lowers(self):
    self.assertEqual((encode(F(3), False), encode(F(3), True)), (1, 2))
    self.assertEqual((encode(F(3, 8), False), encode(F(3, 8), True)), (-2, -1))

  def test_out_of_range_positive_fields_and_noninteger_wire_codes_fail(self):
    for value in (F(-1), F(1, 2**128), F(2) ** 128):
      with self.subTest(value=value), self.assertRaises(ValueError):
        encode(value, True)
    for value in (-129, 128, True, 1.0):
      with self.subTest(value=value), self.assertRaises(ValueError):
        decode(value)


class CanonicalPacketTest(unittest.TestCase):
  def test_two_certified_half_quarters_do_not_become_a_complete_quarter(self):
    start = F(2451545)
    rows = [
      {
        "planet": "saturn",
        "analysis_interval_exact": (str(left), str(left + F(1, 8))),
        "status": "observer_envelopes_constructed",
      }
      for left in (start, start + F(1, 8))
    ]
    records, exclusions = derive({"results": rows}, "", ())
    self.assertEqual(records, [])
    self.assertEqual([row["reason"] for row in exclusions], ["not_a_complete_canonical_quarter"] * 2)

  def test_unresolved_whole_quarter_remains_excluded(self):
    rows = [{"planet": "saturn", "analysis_interval_exact": ("2451545", "9806181/4"), "status": "axial_unresolved"}]
    records, exclusions = derive({"results": rows}, "", ())
    self.assertEqual(records, [])
    self.assertEqual(exclusions, [{"parent_index": 0, "planet": "saturn", "reason": "axial_unresolved"}])

  def test_a_query_shifted_quarter_does_not_relabel_itself_as_canonical(self):
    rows = [
      {
        "planet": "mars",
        "analysis_interval_exact": ("19612361/8", "19612363/8"),
        "status": "observer_envelopes_constructed",
      }
    ]
    with self.assertRaisesRegex(ValueError, "canonical cell"):
      derive({"results": rows}, "", ())

  def test_empty_proof_set_has_no_invented_packets_or_exclusions(self):
    self.assertEqual(derive({"results": []}, "", ()), ([], []))
