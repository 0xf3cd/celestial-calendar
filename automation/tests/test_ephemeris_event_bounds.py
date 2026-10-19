# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Keep the numerical evidence bound to its source and conservative serialized fields."""

import copy
import hashlib
import json
import runpy
import unittest

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = runpy.run_path(str(ROOT / "statistics/ephemeris_event_bounds.py"))


class EventBoundRecordsTest(unittest.TestCase):
  def setUp(self) -> None:
    self.record = json.loads((ROOT / GENERATOR["RECORD_PATH"]).read_text())

  def test_generated_header_preserves_every_exact_directed_bound(self) -> None:
    self.assertEqual(GENERATOR["emit"](self.record), (ROOT / GENERATOR["HEADER_PATH"]).read_text())

  def test_current_numerical_sources_are_pinned(self) -> None:
    actual = {
      name: hashlib.sha256((ROOT / name).read_text(encoding="utf-8").encode("utf-8")).hexdigest()
      for name in self.record["source_sha256"]
    }
    solar = (
      (ROOT / "src/astro/sun.hpp").read_text(encoding="utf-8").split("namespace astro::sun::geocentric_coord {", 1)[1]
    )
    solar = solar.split("} // namespace astro::sun::geocentric_coord", 1)[0]
    GENERATOR["check_sources"](self.record, actual, hashlib.sha256(solar.encode("utf-8")).hexdigest())

  def test_math_reference_artifacts_and_collector_are_pinned(self) -> None:
    references = (ROOT / GENERATOR["RECORD_PATH"]).parent
    record = json.loads((references / "LIBM_REFERENCE_PINS.json").read_text())
    collector = hashlib.sha256((ROOT / "statistics/ephemeris_event_math_reference.py").read_bytes()).hexdigest()
    header = hashlib.sha256((references / "libm_reference.hpp").read_bytes()).hexdigest()
    GENERATOR["check_math_references"](record, collector, header)
    with self.assertRaisesRegex(ValueError, "math-reference identity"):
      GENERATOR["check_math_references"](record, "0" * 64, header)
    with self.assertRaisesRegex(ValueError, "math-reference identity"):
      GENERATOR["check_math_references"](record, collector, "0" * 64)

  def test_understated_rate_error_is_rejected(self) -> None:
    self.record["tables"]["EVENT_BOUNDS_WIDE"]["MARS"]["rate_error"]["hex"] = "0x0p+0"
    with self.assertRaisesRegex(ValueError, "rounding direction"):
      GENERATOR["emit"](self.record)

  def test_raised_signed_lower_bound_is_rejected(self) -> None:
    field = self.record["tables"]["EVENT_BOUNDS_WIDE"]["SUN"]["signed_rate_lower"]
    field["hex"] = (float.fromhex(field["hex"]) + 1.0).hex()
    with self.assertRaisesRegex(ValueError, "rounding direction"):
      GENERATOR["emit"](self.record)

  def test_reordered_targets_and_changed_premises_are_rejected(self) -> None:
    reordered = copy.deepcopy(self.record)
    reordered["target_order"][0:2] = reversed(reordered["target_order"][0:2])
    with self.assertRaisesRegex(ValueError, "target order"):
      GENERATOR["emit"](reordered)
    self.record["epsilon_hex"] = "0x1p-30"
    with self.assertRaisesRegex(ValueError, "premise"):
      GENERATOR["emit"](self.record)

  def test_changed_numerical_source_invalidates_the_bound_evidence(self) -> None:
    actual = dict(self.record["source_sha256"])
    solar = self.record["solar_geocentric_namespace_body_sha256"]
    GENERATOR["check_sources"](self.record, actual, solar)
    actual["src/astro/ephemeris.hpp"] = "0" * 64
    with self.assertRaisesRegex(ValueError, "source identity"):
      GENERATOR["check_sources"](self.record, actual, solar)
    with self.assertRaisesRegex(ValueError, "source identity"):
      GENERATOR["check_sources"](self.record, self.record["source_sha256"], "0" * 64)


if __name__ == "__main__":
  unittest.main()
