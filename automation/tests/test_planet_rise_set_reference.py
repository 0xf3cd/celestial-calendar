# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Frozen event-reference replay, exact raw input checks and full fixture regeneration."""

import copy
import hashlib
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


def generator():
  spec = importlib.util.spec_from_file_location(
    "planet_rise_set_reference", ROOT / "statistics/planet_rise_set_reference.py"
  )
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


def test_full_replay_and_generation():
  module = generator()
  data = json.loads(module.RECEIPT.read_text())
  pins = json.loads((module.RECEIPT.parent / "SOURCE_PINS.json").read_text())
  assert pins["calibration_sha256"] == hashlib.sha256(module.RECEIPT.read_bytes()).hexdigest()
  assert data["frozen_calibration_sha256"] == "cf02a81b07b227695549d8f49d9bc1b1ea7284dfcbd8ccbd14e415150730194c"
  module.replay(data)
  assert module.render(data) == module.FIXTURE.read_text()
  assert {row["case"]["target"] for row in data["cases"]} == set(range(2, 10))
  assert len(data["source_requests"]) == 68
  assert sum(len(row["events"]) for row in data["cases"]) == 32
  assert all(source["params"]["TIME_TYPE"] == "'TT'" for source in data["source_requests"])
  assert all(source["params"]["APPARENT"] == "'AIRLESS'" for source in data["source_requests"])
  assert {source["params"]["SITE_COORD"] for source in data["source_requests"]} >= {"'180,0,0'", "'30,-40,1'"}


@pytest.mark.parametrize(
  "field",
  [
    "reference_width_seconds",
    "numerical_solver_error_bound_seconds",
    "measured_clock_shift_seconds",
    "source_clock_physical_mid_gap_seconds",
  ],
)
def test_changed_calibration_relations_fail(field):
  module = generator()
  data = json.loads(module.RECEIPT.read_text())
  data["cases"][0]["events"][0][field] += 1
  with pytest.raises(AssertionError):
    module.replay(data)


def test_reversed_crossing_and_widened_allowance_fail():
  module = generator()
  data = json.loads(module.RECEIPT.read_text())
  broken = copy.deepcopy(data)
  broken["cases"][0]["events"][0]["kind"] = "set"
  with pytest.raises(AssertionError):
    module.replay(broken)
  data["full_model_test_tolerances_seconds"]["2"] *= 2
  with pytest.raises(AssertionError):
    module.replay(data)


def test_whole_table_alignment():
  lines = generator().aligned([[1.0, 2.0], [10000.0, -1e-10]]).splitlines()
  assert lines[0].index(",") == lines[1].index(",")


def test_generation_does_not_read_production_header(tmp_path, monkeypatch):
  module = generator()
  data = json.loads(module.RECEIPT.read_text())
  expected = module.FIXTURE.read_text()
  monkeypatch.setattr(module, "ROOT", tmp_path)
  assert module.render(data) == expected


def test_raw_response_identity_and_exact_source_endpoints(tmp_path):
  module = generator()
  data = json.loads(module.RECEIPT.read_text())
  # A small independent CSV parser control uses retained numeric endpoints, not production output.
  event = data["cases"][0]["events"][0]
  directory = tmp_path / data["cases"][0]["case"]["id"]
  directory.mkdir()
  path = directory / "refine-0.01.request.json"
  params = {"TIME_TYPE": "'TT'", "APPARENT": "'AIRLESS'"}
  path.write_text(json.dumps({"params": params}))
  header = (
    "Target body name: Mercury\nNO (AIRLESS)\n"
    "Date_________JDTT,Elevation_(a-app),R.A.__(a-app),DEC___(a-app),L_Ap_Sid_Time,TDB-UT,UT1-UTC,\n$$SOE\n"
  )
  raw_rows = []
  for key in ("reference_lower", "reference_upper"):
    endpoint = event[key]
    last = (endpoint["hour_angle_deg"] + endpoint["ra_deg"]) / 15
    raw_rows.append(
      f"{endpoint['jde_tt']},{endpoint['altitude_deg']},{endpoint['ra_deg']},{endpoint['dec_deg']},{last},{endpoint['TDB_UT1_seconds']},0,\n"
    )
  raw = (header + "".join(raw_rows) + "$$EOE\n").encode()
  response = path.with_name("refine-0.01.response.txt")
  response.write_bytes(raw)
  data["source_requests"] = [
    {
      "path": str(path.relative_to(tmp_path)),
      "params": params,
      "response_bytes": len(raw),
      "response_sha256": hashlib.sha256(raw).hexdigest(),
      "source_identity": ["Target body name: Mercury"],
    }
  ]
  data["cases"][0]["events"] = [event]
  module.verify_raw(data, tmp_path)
  response.write_bytes(raw.replace(b"Mercury", b"Venus  "))
  with pytest.raises(AssertionError):
    module.verify_raw(data, tmp_path)


def test_regenerates_outside_repository_and_maps_docs(tmp_path):
  module = generator()
  output = tmp_path / "reference.hpp"
  result = subprocess.run(
    [sys.executable, "-B", str(ROOT / "statistics/planet_rise_set_reference.py"), "--output", str(output)],
    cwd=tmp_path,
    capture_output=True,
    text=True,
    check=False,
  )
  assert result.returncode == 0, result.stderr
  assert output.read_bytes() == module.FIXTURE.read_bytes()
  from automation import docs_site

  assert "statistics/planet_rise_set_reference.md" in docs_site.GUIDES


def test_source_registration_requires_reciprocal_facts(tmp_path):
  from automation import retained_provenance

  registry = json.loads((ROOT / "src/test/provenance/retained/retained_host_blocks.json").read_text())
  block = next(row for row in registry["blocks"] if row["id"] == "v06-planet-rise-set-receipt")
  retained_provenance._verify_adjacent_record(ROOT, block, {})
  for marking in (
    row for row in registry["blocks"] if row["id"] in {"v06-planet-rise-set-fixture", "v06-planet-rise-set-generator"}
  ):
    text = (ROOT / marking["path"]).read_text()
    retained_provenance._marking_slice(marking["path"], text, marking["marker"], marking["id"])
  for key in ("path", "adjacent_path"):
    dest = tmp_path / block[key]
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes((ROOT / block[key]).read_bytes())
  attribution = tmp_path / block["adjacent_path"]
  attribution.write_text(attribution.read_text().replace("not a consumer-package member", "consumer-package member"))
  with pytest.raises(RuntimeError):
    retained_provenance._verify_adjacent_record(tmp_path, block, {})
