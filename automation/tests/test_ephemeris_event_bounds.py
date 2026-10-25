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
import subprocess
import sys
import unittest

from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = runpy.run_path(str(ROOT / "statistics/ephemeris_event_bounds.py"))


class EventBoundRecordsTest(unittest.TestCase):
  def setUp(self) -> None:
    self.record = json.loads((ROOT / GENERATOR["RECORD_PATH"]).read_text())

  def test_generated_header_preserves_every_exact_directed_bound(self) -> None:
    self.assertEqual(GENERATOR["emit"](self.record), (ROOT / GENERATOR["HEADER_PATH"]).read_text())

  def test_current_numerical_sources_are_pinned(self) -> None:
    GENERATOR["check_sources"](self.record, GENERATOR["source_identity"](ROOT))
    self.assertEqual(len(self.record["source_identity"]["sha256"]), 21)

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
    actual = copy.deepcopy(self.record["source_identity"])
    GENERATOR["check_sources"](self.record, actual)
    actual["sha256"]["src/astro/ephemeris.hpp"] = "0" * 64
    with self.assertRaisesRegex(ValueError, "source identity"):
      GENERATOR["check_sources"](self.record, actual)


@pytest.fixture
def source_copy(tmp_path):
  for name in GENERATOR["SOURCE_PATHS"]:
    path = tmp_path / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes((ROOT / name).read_bytes())
  return tmp_path


def test_source_identity_accepts_comments_and_layout_in_real_header_copies(source_copy):
  record = json.loads((ROOT / GENERATOR["RECORD_PATH"]).read_text())
  for name in GENERATOR["SOURCE_PATHS"]:
    path = source_copy / name
    text = path.read_text(encoding="utf-8").replace("#pragma once", "#  pragma   once", 1)
    text = text.replace("namespace ", "namespace\n  ", 1)
    path.write_bytes((text + "\n// Comment-only source identity check.\n").replace("\n", "\r\n").encode())
  GENERATOR["check_sources"](record, GENERATOR["source_identity"](source_copy))


def test_source_identity_names_the_header_with_an_unsupported_shape(source_copy):
  path = source_copy / "src/astro/ephemeris.hpp"
  path.write_text(path.read_text(encoding="utf-8") + "\n#define F(x) #x\n", encoding="utf-8")
  with pytest.raises(ValueError, match="src/astro/ephemeris.hpp: Unsupported"):
    GENERATOR["source_identity"](source_copy)


@pytest.mark.parametrize("name", GENERATOR["SOURCE_PATHS"])
def test_source_identity_rejects_include_changes_in_each_real_header_copy(source_copy, name):
  record = json.loads((ROOT / GENERATOR["RECORD_PATH"]).read_text())
  path = source_copy / name
  text = path.read_text(encoding="utf-8")
  assert "#pragma once" in text
  path.write_text(text.replace("#pragma once", "#pragma once\n#include <cstddef>", 1), encoding="utf-8")
  with pytest.raises(ValueError, match="source identity changed"):
    GENERATOR["check_sources"](record, GENERATOR["source_identity"](source_copy))


@pytest.mark.parametrize(
  ("name", "old", "new"),
  [
    ("src/astro/vsop87d/earth_coeff.hpp", "175347045.673", "175347045.674"),
    ("src/astro/sun.hpp", "year + 1", "year + 2"),
    ("src/astro/sun.hpp", '#include "earth.hpp"', '#include "moon.hpp"'),
  ],
)
def test_source_identity_rejects_coefficient_and_complete_solar_file_changes(source_copy, name, old, new):
  record = json.loads((ROOT / GENERATOR["RECORD_PATH"]).read_text())
  path = source_copy / name
  text = path.read_text(encoding="utf-8")
  assert text.count(old) == 1
  path.write_text(text.replace(old, new), encoding="utf-8")
  with pytest.raises(ValueError, match="source identity changed"):
    GENERATOR["check_sources"](record, GENERATOR["source_identity"](source_copy))


@pytest.mark.parametrize("field", ["algorithm", "version"])
@pytest.mark.parametrize("action", ["missing", "unknown"])
def test_source_identity_rejects_missing_or_unknown_versions(field, action):
  record = json.loads((ROOT / GENERATOR["RECORD_PATH"]).read_text())
  if action == "missing":
    del record["source_identity"][field]
  else:
    record["source_identity"][field] = "unknown"
  with pytest.raises(ValueError, match="Unsupported C\\+\\+ token digest"):
    GENERATOR["emit"](record)
  with pytest.raises(ValueError, match="Unsupported C\\+\\+ token digest"):
    GENERATOR["check_sources"](record, {})


@pytest.mark.parametrize("action", ["missing-identity", "missing-file", "extra-file", "bad-hash", "old-schema"])
def test_source_identity_rejects_incomplete_or_stale_records(action):
  record = json.loads((ROOT / GENERATOR["RECORD_PATH"]).read_text())
  if action == "missing-identity":
    del record["source_identity"]
  elif action == "missing-file":
    del record["source_identity"]["sha256"]["src/astro/sun.hpp"]
  elif action == "extra-file":
    record["source_identity"]["sha256"]["src/astro/other.hpp"] = "0" * 64
  elif action == "bad-hash":
    record["source_identity"]["sha256"]["src/astro/sun.hpp"] = "not-a-sha256"
  else:
    record["schema"] = 1
  with pytest.raises(ValueError, match="Unsupported"):
    GENERATOR["emit"](record)


def test_bound_checker_direct_script_from_unrelated_directory(tmp_path):
  result = subprocess.run(
    [sys.executable, str(ROOT / "statistics/ephemeris_event_bounds.py"), "--check"],
    cwd=tmp_path,
    capture_output=True,
    text=True,
    check=True,
  )
  assert "source pins, directed C++ literals and math-reference identities match" in result.stdout


if __name__ == "__main__":
  unittest.main()
