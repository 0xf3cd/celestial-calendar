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
import ast
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

import pytest

from automation.markdown_source import links, tokens


ROOT = Path(__file__).resolve().parents[2]
RECEIPTS = ROOT / "src/test/provenance/observer"


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


def test_observer_source_and_configuration_pins_are_frozen():
  raw = (RECEIPTS / "SOURCE_PINS.json").read_bytes()
  assert hashlib.sha256(raw).hexdigest() == "fca7352caf640c390f2043307e7318cd27b323b87405976fb8c8d0a0641502cc", (
    "observer source/configuration pins drift"
  )
  pins = json.loads(raw)
  sibling = json.loads((ROOT / "src/test/provenance/longitude-rate/SOURCE_PINS.json").read_text())["sources"]["pymeeus"]
  assert pins["pymeeus"]["commit"] == sibling["commit"]
  assert pins["pymeeus"]["source_sha256"] == sibling["entry_point_sha256"]["pymeeus/Earth.py"]
  assert pins["pymeeus"]["default_ellipsoid"] == "WGS84"
  assert pins["sofa"]["issue"] == "2023-10-11"
  assert pins["horizons"]["api_version"] == "1.2"
  assert pins["horizons"]["kernel_revision"] == "not exposed by the retained responses"
  assert len(pins["horizons"]["responses"]) == 30
  assert len({row["id"] for row in pins["horizons"]["responses"]}) == 30
  assert {row["target_source_label"] for row in pins["horizons"]["responses"]} == {
    "DE441",
    "mar099",
    "jup365_merged",
    "sat441l",
    "ura184_merged",
    "nep098_merged",
    "plu060_merged",
  }


def test_public_numeric_receipts_have_exact_inputs_and_no_extra_fields():
  generator = load("observer_reference")
  names = ("sofa-observer-preflight.json", "pymeeus-matched-constant-preflight.json", "calibration-results.json")
  sofa, pymeeus, calibration = generator.load_receipts(
    [RECEIPTS / name for name in names], RECEIPTS / "SOURCE_PINS.json"
  )
  assert set(sofa) == {"seed", "a_m", "flattening", "oracle_source_sha256", "cases"}
  assert set(pymeeus) == {"seed", "oracle_source_sha256", "controlled_coefficient_factor", "cases"}
  assert set(calibration) == {"source_sha", "cases"}
  assert sofa["seed"] == pymeeus["seed"] == 42
  assert sofa["a_m"] == 6378137.0 and sofa["flattening"] == 1.0 / 298.257223563
  for data, keys in (
    (sofa, {"longitude_deg", "latitude_deg", "height_m", "sofa_xyz_m"}),
    (
      pymeeus,
      {
        "ra_deg",
        "dec_deg",
        "distance_au",
        "latitude_deg",
        "height_m",
        "hour_angle_deg",
        "oracle_distance_au",
        "matched_ra_deg",
        "matched_dec_deg",
      },
    ),
    (
      calibration,
      {
        "target_code",
        "site",
        "jde_tt",
        "jd_ut1",
        "tt_minus_utc_seconds",
        "dut1_seconds",
        "gast_deg",
        "expected",
        "library_geocentric",
        "horizons_geocentric",
        "native_reduced",
        "horizons_reduced",
      },
    ),
  ):
    assert all(set(row) == keys for row in data["cases"])
  factor = math.sin(math.radians(8.794 / 3600.0)) / (6378.137 / 149597870.7)
  assert abs(pymeeus["controlled_coefficient_factor"] - factor) < 3e-16
  for row in pymeeus["cases"]:
    assert row["oracle_distance_au"] == row["distance_au"] * pymeeus["controlled_coefficient_factor"]


def test_clone_default_full_fixture_regeneration_is_identical(tmp_path):
  output = tmp_path / "observer.hpp"
  result = subprocess.run(
    [sys.executable, "-B", str(ROOT / "statistics/observer_reference.py"), "--output", str(output)],
    cwd=tmp_path,
    capture_output=True,
    text=True,
    check=False,
  )
  assert result.returncode == 0, result.stderr
  assert output.read_bytes() == (ROOT / "src/test/astro/observer_reference_data.hpp").read_bytes(), (
    "full observer fixture regeneration differs"
  )


def test_independent_calibration_replays_both_frozen_tolerance_bases():
  replay, generator = load("observer_oracle_replay"), load("observer_reference")
  rows = replay.replay_calibration(RECEIPTS / "calibration-results.json")
  assert len(rows) == 60
  fixture = (ROOT / "src/test/astro/observer_reference_data.hpp").read_text()
  for table, key in (("REDUCTION_TOL", "horizons_reduced"), ("PROVIDER_TOL", "native_reduced")):
    actual = [generator.bounds([row for row in rows if row["target_code"] == code], key) for code in range(10)]
    assert actual == replay.rows(fixture, table), f"independent calibration tolerance basis: {table}"
  parsed = ast.parse((ROOT / "statistics/observer_oracle_replay.py").read_text())
  imports = [node for node in ast.walk(parsed) if isinstance(node, (ast.Import, ast.ImportFrom))]
  modules = [
    node.module if isinstance(node, ast.ImportFrom) else name.name
    for node in imports
    for name in (node.names if isinstance(node, ast.Import) else [None])
  ]
  assert set(modules) <= {
    "argparse",
    "csv",
    "ctypes",
    "hashlib",
    "io",
    "json",
    "math",
    "pathlib",
    "re",
    "sys",
    "pymeeus.Angle",
    "pymeeus.Earth",
  }


def test_offline_calibration_needs_no_fixture_or_external_oracle(tmp_path):
  result = subprocess.run(
    [
      sys.executable,
      "-S",
      "-B",
      str(ROOT / "statistics/observer_oracle_replay.py"),
      "--fixture",
      str(tmp_path / "absent-fixture.hpp"),
    ],
    cwd=tmp_path,
    capture_output=True,
    text=True,
    check=False,
  )
  assert result.returncode == 0, result.stderr
  assert "60 rows, both frozen reductions identical" in result.stdout


def test_observer_terms_links_resolve_to_the_named_sources():
  guide = ROOT / "statistics/observer_reference.md"
  targets = dict(links(tokens(guide.read_text())))
  expected = {
    "2023-10-11 terms": "../src/test/provenance/sofa/2023-10-11/doc/copyr.lis",
    "LICENSE.txt at the oracle commit": "https://github.com/architest/pymeeus/blob/c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92/LICENSE.txt",
    "manual acknowledgements": "https://ssd.jpl.nasa.gov/horizons/manual.html#acknowledgements",
    "JPL policies and important notices": "https://www.jpl.nasa.gov/caltechjpl-privacy-policies-and-important-notices",
    "second-edition terms record": "../src/test/provenance/retained/MEEUS_R22_ATTRIBUTION.md",
    "V32 disposition": "../src/test/provenance/retained/record.json",
  }
  assert {name: targets.get(name) for name in expected} == expected, "observer source/terms linkage differs"
  for path in (guide, RECEIPTS / "ATTRIBUTION.md"):
    for _label, target in links(tokens(path.read_text())):
      if not target.startswith("https://"):
        assert (path.parent / target.split("#")[0]).is_file(), target
  pins = json.loads((RECEIPTS / "SOURCE_PINS.json").read_text())
  assert pins["sofa"]["terms"] == "../sofa/2023-10-11/doc/copyr.lis"
  assert pins["pymeeus"]["terms"] == expected["LICENSE.txt at the oracle commit"]
  assert pins["horizons"]["terms"] == expected["JPL policies and important notices"]
  assert pins["meeus"]["terms_record"] == "../retained/MEEUS_R22_ATTRIBUTION.md"


@pytest.mark.parametrize(
  "name", ["sofa-observer-preflight.json", "pymeeus-matched-constant-preflight.json", "calibration-results.json"]
)
def test_changed_valid_json_receipt_is_rejected(tmp_path, name):
  generator = load("observer_reference")
  data = json.loads((RECEIPTS / name).read_text())
  row = data["cases"][0]
  key = "longitude_deg" if name.startswith("sofa") else "ra_deg" if name.startswith("pymeeus") else "gast_deg"
  row[key] += 0.25
  path = tmp_path / name
  path.write_text(json.dumps(data))
  with pytest.raises(AssertionError, match="receipt identity"):
    generator.load_receipts([path], RECEIPTS / "SOURCE_PINS.json")


@pytest.mark.parametrize("key", ["horizons_geocentric", "library_geocentric", "horizons_reduced", "native_reduced"])
def test_independent_calibration_rejects_changed_inputs_or_outputs(tmp_path, key):
  replay = load("observer_oracle_replay")
  data = json.loads((RECEIPTS / "calibration-results.json").read_text())
  data["cases"][0][key][0] += 0.25
  path = tmp_path / "calibration.json"
  path.write_text(json.dumps(data))
  with pytest.raises(AssertionError, match="independent calibration"):
    replay.replay_calibration(path)
