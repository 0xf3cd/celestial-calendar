# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import json
import subprocess
import sys

from pathlib import Path

import pytest

from automation import source_digest
from automation.source_digest import canonical_cpp, retained_digest, retained_slice, verify_retained_data
from automation.tests.test_retained_maintenance import materialize_block


REPO_ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
  ("source", "expected"),
  [
    ("int x = 1; // line comment\nreturn x;", "int x = 1; return x;"),
    ("int /* block\ncomment */ x =\t1;", "int x = 1;"),
  ],
  ids=["line-comment", "block-comment-and-whitespace"],
)
def test_canonical_cpp(source, expected):
  assert canonical_cpp(source) == expected


@pytest.mark.parametrize(
  ("script", "expected"),
  [
    (
      "automation/internal_provenance.py",
      "InternalProvenanceCounts(history_tables=8, horizons_inputs=42, julian_internal_rows=7)",
    ),
    (
      "automation/nasa_delta_t_provenance.py",
      "ProvenanceCounts(runtime_branches=15, nasa_v25_rows=11, usno_v25_rows=2, bulletin_a_v25_rows=12, "
      "v27_relations=2)",
    ),
  ],
  ids=["internal-provenance", "nasa-delta-t-provenance"],
)
def test_provenance_direct_script(script, expected):
  result = subprocess.run(
    [sys.executable, script],
    cwd=REPO_ROOT,
    check=True,
    capture_output=True,
    text=True,
  )
  assert result.stdout.strip() == expected


def test_retained_master_record_rejects_tampering(tmp_path):
  materialize_block(tmp_path, "moon-0")
  registry_path = tmp_path / source_digest.RETAINED_REGISTRY
  registry = json.loads(registry_path.read_text(encoding="utf-8"))
  registry["data_blocks"]["moon-0"]["sha256"] = "0" * 64
  registry_path.write_text(json.dumps(registry), encoding="utf-8")
  with pytest.raises(RuntimeError, match="retained data identity record differs"):
    verify_retained_data(tmp_path, ("moon-0",))


@pytest.mark.parametrize("path", ["../outside.cpp", "/absolute.cpp"])
def test_retained_contract_requires_repository_relative_paths(tmp_path, monkeypatch, path):
  materialize_block(tmp_path, "moon-0")
  registry_path = tmp_path / source_digest.RETAINED_REGISTRY
  registry = json.loads(registry_path.read_text(encoding="utf-8"))
  registry["data_blocks"]["moon-0"]["path"] = path
  registry_path.write_text(json.dumps(registry), encoding="utf-8")
  # A synthetic contract reaches path safety independently of the master-record guard.
  monkeypatch.setattr(source_digest, "RETAINED_DATA_SHA256", source_digest.json_digest(registry["data_blocks"]))
  with pytest.raises(RuntimeError, match="path must be repository-relative"):
    verify_retained_data(tmp_path, ("moon-0",))


def test_unknown_retained_digest_method_fails(tmp_path):
  path = tmp_path / "data.txt"
  path.write_text("reference", encoding="utf-8")
  with pytest.raises(RuntimeError, match="unknown retained-digest method"):
    retained_digest(path, "unknown")


@pytest.mark.parametrize(
  ("text", "message"),
  [
    ("start data start more end", "start is missing or ambiguous"),
    ("start data", "end is missing"),
  ],
)
def test_retained_slice_rejects_ambiguous_or_incomplete_boundaries(text, message):
  with pytest.raises(RuntimeError, match=message):
    retained_slice(text, "start", "end")
