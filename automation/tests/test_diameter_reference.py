# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Independent disk-source facts, regeneration and exact R34 conversion boundaries."""

import importlib.util
import json
import re
from decimal import Decimal
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_reference_replay_and_full_fixture_regeneration():
  path = ROOT / "statistics/diameter_reference.py"
  spec = importlib.util.spec_from_file_location("diameter_reference", path)
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  data = json.loads(module.RECEIPT.read_text())
  module.replay(data)
  assert module.FIXTURE.read_text() == module.render(data)
  assert len(data["cases"]) == 50
  assert {row["target"] for row in data["cases"]} == set(range(10))
  assert {row["jde_tt"] for row in data["cases"] if row["target"] == 1} >= {2461042.5, 2461054.5}
  assert {row["jde_tt"] for row in data["cases"] if row["target"] == 0} >= {2461044.5, 2461228.5}


def test_adopted_radius_sources_and_header_values_are_independent():
  record = json.loads((ROOT / "src/test/provenance/diameter/radii.json").read_text())
  expected = {
    "sun": 695700.0,
    "moon": 1737.4,
    "mercury": 2440.53,
    "venus": 6051.8,
    "mars": 3396.19,
    "jupiter": 71492.0,
    "saturn": 60268.0,
    "uranus": 25559.0,
    "neptune": 24764.0,
    "pluto": 1188.3,
  }
  assert {r["target"]: r["radius_km"] for r in record["radii"]} == expected
  data = json.loads((ROOT / "src/test/provenance/diameter/calibration.json").read_text())
  requests = {r["label"]: r for r in data["requests"]}
  for target, radius in expected.items():
    assert requests[target + "-j2000"]["header_radius_km"] == radius
  assert "arxiv.org/abs/1605.09788v1" in record["definitions"]["1"]["source"]
  assert "mean" in record["definitions"]["2"]["convention"]
  assert "equatorial" in record["definitions"]["3"]["convention"]


def test_exact_au_definitions_match_retained_nist_erfa_relation():
  source = (ROOT / "src/test/provenance/erfa/v2.0.1/src/erfam.h").read_text()
  metres = Decimal(re.search(r"#define ERFA_DAU\s+\(([\d.e]+)\)", source)[1])
  for relative, name in (
    ("bindings/python/src/celestial_calendar/diameter.py", "AU_KM"),
    ("bindings/javascript/src/index.mjs", "DIAMETER_AU_KM"),
  ):
    text = (ROOT / relative).read_text()
    value = Decimal(re.search(rf"\b{name}\s*=\s*([\d.]+)", text)[1])
    assert value * 1000 == metres
    assert "NIST SP 330 (2019), Table 8" in text and "Retained material boundary (R34)" in text
