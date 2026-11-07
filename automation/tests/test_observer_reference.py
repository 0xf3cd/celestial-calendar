# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Observer fixture parsing and frozen-calibration tolerance generation."""

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def load(name):
  spec = importlib.util.spec_from_file_location(name, ROOT / "statistics" / f"{name}.py")
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


def test_frozen_calibration_bounds_are_per_column_wrap_aware_and_outward():
  generator = load("observer_reference")
  data = [{"expected": [359.75, 0, 1, 0.25, -1], "before_implementation": [0, 0.0011, 1.00001, 0, -0.99]}]
  tolerance = generator.bounds(data, "before_implementation")
  assert tolerance == [0.8, 0.004, 4e-05, 0.8, 0.04]


def test_exact_oracle_fixture_inventory_and_real_inputs():
  replay = load("observer_oracle_replay")
  text = (ROOT / "src/test/astro/observer_reference_data.hpp").read_text()
  sofa = replay.rows(text, "SOFA")
  pymeeus = replay.rows(text, "PYMEEUS")
  horizons = replay.rows(text, "HORIZONS")
  assert len(sofa) == 72 and len(pymeeus) == 128 and len(horizons) == 60
  assert {row[1] for row in sofa} >= {-90.0, 90.0}
  assert any(row[2] < 0 for row in sofa)
  assert all(row[6] != row[2] for row in pymeeus)
  assert {row[0] for row in horizons} == set(range(10))
  assert {row[5] for row in horizons} == {2451545.0, 2461351.5, 2461351.875}
  assert {row[3] for row in horizons} == {1.706, 0.05}
  assert all(row[4] != row[5] for row in horizons)


def test_table_aligns_over_whole_block_and_preserves_float_values():
  generator, replay = load("observer_reference"), load("observer_oracle_replay")
  data = [[1.0, -999.125], [123456.0, 2e-12]]
  text = generator.table("EXAMPLE", data)
  assert replay.rows(text, "EXAMPLE") == data
  lines = text.splitlines()[1:3]
  assert lines[0].index(",") == lines[1].index(",")


def test_observer_guide_and_reference_routes_have_site_owners(monkeypatch):
  from automation import docs_site

  assert "docs/OBSERVER.md" in docs_site.GUIDES
  assert "statistics/observer_reference.md" in docs_site.GUIDES
  assert ("Observer queries", "docs/OBSERVER.html") in docs_site.NAVIGATION
  # Route selection is independent of the clean-committed-source producer gate.
  monkeypatch.setattr(docs_site, "git", lambda *_args: b"tracked-route-control")
  assert (
    docs_site.source_destination(
      ROOT, "bindings/python/README.md", "../../docs/OBSERVER.md", "bindings/python/README.html", "a" * 40
    )
    == "../../docs/OBSERVER.html"
  )
  assert (
    docs_site.source_destination(
      ROOT, "docs/OBSERVER.md", "../statistics/observer_reference.md", "docs/OBSERVER.html", "a" * 40
    )
    == "../statistics/observer_reference.html"
  )
