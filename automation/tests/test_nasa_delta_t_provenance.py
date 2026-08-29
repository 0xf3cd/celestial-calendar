# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

import hashlib
import json

from pathlib import Path
from shutil import copy2

import pytest

from automation.nasa_delta_t_provenance import (
  NASA_ACKNOWLEDGMENT,
  NASA_ACKNOWLEDGMENT_SHA256,
  NASA_NOTICE_APPLICABILITY,
  NASA_RECORD,
  NASA_RECORD_SHA256,
  NASA_ROOT,
  REPO_ROOT,
  ProvenanceCounts,
  verify_nasa_delta_t_provenance,
)


TARGET_FILES = (
  Path("src/astro/delta_t.hpp"),
  Path("src/calendar/lunar/algo3.hpp"),
  Path("src/test/astro/delta_t_test_helper.hpp"),
  Path("src/test/astro/julian_day_test.cpp"),
  Path("src/test/lunar/algo3_test.cpp"),
  Path("statistics/usno_data.txt"),
)


def materialize_inputs(destination: Path) -> Path:
  nasa_root = destination / NASA_ROOT.relative_to(REPO_ROOT)
  nasa_root.mkdir(parents=True)
  copy2(NASA_RECORD, nasa_root / NASA_RECORD.name)
  copy2(NASA_ACKNOWLEDGMENT, nasa_root / NASA_ACKNOWLEDGMENT.name)
  for relative in TARGET_FILES:
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / relative, target)
  return nasa_root


def replace_once(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  assert text.count(old) == 1
  path.write_text(text.replace(old, new), encoding="utf-8")


def mutate_record(path: Path, mutation) -> str:
  payload = json.loads(path.read_text(encoding="utf-8"))
  mutation(payload)
  path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
  return hashlib.sha256(path.read_bytes()).hexdigest()


def test_nasa_tp_relations_and_acknowledgment_are_pinned_without_an_upstream_byte_claim():
  assert NASA_RECORD_SHA256 == "032c18173f4d250aceab5df81cf7904cc0608b7af3e445dcd1af79e7045a759b"
  assert NASA_ACKNOWLEDGMENT_SHA256 == "2d90c4731996cd9b8586c055eb4c29535ebab66abe426b53ae944d15a4887881"
  assert NASA_NOTICE_APPLICABILITY == (
    "the NASA/TP-2006-214141 Delta-T polynomial material in src/astro/delta_t.hpp, the 398 non-HKO lunar-year "
    "table values in src/calendar/lunar/algo3.hpp retained from its NASA-backed original generation, and the "
    "NASA-sourced historical Delta-T validation values in src/test"
  )
  assert verify_nasa_delta_t_provenance() == ProvenanceCounts(15, 11, 2, 12, 2)


@pytest.mark.parametrize(
  ("old", "new", "message"),
  [
    ("if (year < -500) {", "if (year < -499) {", r"equation \(11\) repository relation differs"),
    ("return 10583.6 -", "return 10583.7 -", r"equation \(12\) repository relation differs"),
  ],
  ids=["interval", "coefficient"],
)
def test_nasa_runtime_interval_and_coefficient_mutations_fail(tmp_path, old, new, message):
  nasa_root = materialize_inputs(tmp_path)
  replace_once(tmp_path / "src/astro/delta_t.hpp", old, new)

  with pytest.raises(RuntimeError, match=message):
    verify_nasa_delta_t_provenance(repo_root=tmp_path, nasa_root=nasa_root)


@pytest.mark.parametrize("mutation", ["partition", "value"], ids=["source-partition", "source-value"])
def test_v25_source_partition_and_value_mutations_fail(tmp_path, mutation):
  nasa_root = materialize_inputs(tmp_path)
  record = nasa_root / "delta_t.json"

  def change(payload):
    nasa = payload["validation_relations"]["v25"]["partitions"][0]
    if mutation == "partition":
      nasa["years"][-1] = 2010
    else:
      nasa["repository_values"][0] = "31.2"

  digest = mutate_record(record, change)
  with pytest.raises(RuntimeError, match="V25 NASA"):
    verify_nasa_delta_t_provenance(repo_root=tmp_path, nasa_root=nasa_root, record_sha256=digest)


@pytest.mark.parametrize(
  ("relation", "field", "value", "message"),
  [
    ("year_500", "delta_t_seconds", 5711, "year-500 value"),
    ("year_2000", "repository_value", "63.84", "year-2000 rounding"),
    ("year_500", "source_standard_error_seconds", 141, "140 s locator"),
  ],
  ids=["5710-seconds", "63.83-seconds", "140-seconds"],
)
def test_v27_value_rounding_and_locator_mutations_fail(tmp_path, relation, field, value, message):
  nasa_root = materialize_inputs(tmp_path)
  record = nasa_root / "delta_t.json"
  digest = mutate_record(
    record,
    lambda payload: payload["validation_relations"]["v27"][relation].update({field: value}),
  )

  with pytest.raises(RuntimeError, match=message):
    verify_nasa_delta_t_provenance(repo_root=tmp_path, nasa_root=nasa_root, record_sha256=digest)


def test_missing_nasa_acknowledgment_fails(tmp_path):
  nasa_root = materialize_inputs(tmp_path)
  (nasa_root / "ACKNOWLEDGMENT.txt").unlink()

  with pytest.raises(FileNotFoundError):
    verify_nasa_delta_t_provenance(repo_root=tmp_path, nasa_root=nasa_root)


def test_overbroad_nasa_notice_applicability_fails():
  with pytest.raises(RuntimeError, match="applicability differs"):
    verify_nasa_delta_t_provenance(notice_applicability="all NASA material in this repository")


def test_unpartitioned_v25_row_fails(tmp_path):
  nasa_root = materialize_inputs(tmp_path)
  helper = tmp_path / "src/test/astro/delta_t_test_helper.hpp"
  replace_once(helper, "  { 2026.0, 69.11 },", "  { 2026.0, 69.11 },\n  { 2027.0, 69.3 },")

  with pytest.raises(RuntimeError, match="not covered by the recorded source partitions"):
    verify_nasa_delta_t_provenance(repo_root=tmp_path, nasa_root=nasa_root)


def test_nasa_lunar_table_citation_cannot_be_removed(tmp_path):
  nasa_root = materialize_inputs(tmp_path)
  lunar_table = tmp_path / "src/calendar/lunar/algo3.hpp"
  replace_once(
    lunar_table,
    " * @ref Espenak and Meeus, NASA/TP-2006-214141, Section 2.7, equations (11)-(25); historical source for 398\n",
    "",
  )

  with pytest.raises(RuntimeError, match="lunar-table citation differs"):
    verify_nasa_delta_t_provenance(repo_root=tmp_path, nasa_root=nasa_root)


@pytest.mark.parametrize(
  ("field", "value"),
  [
    ("entries", 400),
    ("source_relation", "unrecorded source"),
  ],
  ids=["entry-count", "source-relation"],
)
def test_nasa_lunar_table_historical_generation_is_pinned(tmp_path, field, value):
  nasa_root = materialize_inputs(tmp_path)
  record = nasa_root / "delta_t.json"
  digest = mutate_record(
    record,
    lambda payload: payload["downstream_runtime_relation"]["historical_generation"].update({field: value}),
  )

  with pytest.raises(RuntimeError, match="historical generation differs"):
    verify_nasa_delta_t_provenance(repo_root=tmp_path, nasa_root=nasa_root, record_sha256=digest)


@pytest.mark.parametrize(
  ("field", "value"),
  [
    ("entries_from_origin", 399),
    ("rebake", {"commit": "unrecorded", "generator": "unrecorded", "years": []}),
  ],
  ids=["retained-count", "rebake"],
)
def test_nasa_lunar_table_current_retention_is_pinned(tmp_path, field, value):
  nasa_root = materialize_inputs(tmp_path)
  record = nasa_root / "delta_t.json"
  digest = mutate_record(
    record,
    lambda payload: payload["downstream_runtime_relation"]["current_retention"].update({field: value}),
  )

  with pytest.raises(RuntimeError, match="current retention differs"):
    verify_nasa_delta_t_provenance(repo_root=tmp_path, nasa_root=nasa_root, record_sha256=digest)


@pytest.mark.parametrize(
  "replacement",
  ["  ASSERT_EQ(400, checked);\n", "  // ASSERT_EQ(401, checked);\n", ""],
  ids=["weakened", "commented-out", "removed"],
)
def test_nasa_lunar_table_full_regeneration_gate_is_pinned(tmp_path, replacement):
  nasa_root = materialize_inputs(tmp_path)
  replace_once(tmp_path / "src/test/lunar/algo3_test.cpp", "  ASSERT_EQ(401, checked);\n", replacement)

  with pytest.raises(RuntimeError, match="full-regeneration gate differs"):
    verify_nasa_delta_t_provenance(repo_root=tmp_path, nasa_root=nasa_root)
