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

from automation.usno_identity import (
  REPO_ROOT,
  USNO_RECORD_SHA256,
  USNO_ROOT,
  IdentityCounts,
  verify_usno_identities,
)


TARGET_FILES = (
  Path("src/test/astro/rise_set_moon_golden_test.cpp"),
  Path("src/test/astro/sidereal_time_test.cpp"),
  Path("src/test/astro/moon_phase_test.cpp"),
  Path("src/test/astro/delta_t_test_helper.hpp"),
  Path("src/test/astro/julian_day_test.cpp"),
  Path("src/test/shared_lib/cabi_smoke_test.cpp"),
  Path("statistics/usno_data.txt"),
)


def materialize_inputs(destination: Path) -> Path:
  usno_root = destination / USNO_ROOT.relative_to(REPO_ROOT)
  usno_root.mkdir(parents=True)
  for name in USNO_RECORD_SHA256:
    copy2(USNO_ROOT / name, usno_root / name)
  for relative in TARGET_FILES:
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / relative, target)
  return usno_root


def record_hashes(usno_root: Path) -> dict[str, str]:
  return {name: hashlib.sha256((usno_root / name).read_bytes()).hexdigest() for name in USNO_RECORD_SHA256}


def mutate_record(usno_root: Path, name: str, mutation) -> None:
  path = usno_root / name
  payload = json.loads(path.read_text(encoding="utf-8"))
  mutation(payload)
  path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def replace_once(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  assert text.count(old) == 1
  path.write_text(text.replace(old, new), encoding="utf-8")


def test_usno_records_reproduce_every_retained_relation_offline():
  assert verify_usno_identities() == IdentityCounts(181, 60, 50, 12, 1, 639, 3, 1)


def test_usno_record_directory_inventory_is_exact(tmp_path):
  usno_root = materialize_inputs(tmp_path)
  (usno_root / "extra.json").write_text("{}\n", encoding="utf-8")

  with pytest.raises(RuntimeError, match="directory inventory differs"):
    verify_usno_identities(repo_root=tmp_path, usno_root=usno_root)


def test_usno_response_hash_mutation_fails_the_record_pin(tmp_path):
  usno_root = materialize_inputs(tmp_path)
  mutate_record(
    usno_root,
    "v12-rstt-oneday.json",
    lambda payload: payload["records"][0]["response"].update(sha256="0" * 64),
  )

  with pytest.raises(RuntimeError, match="record hash mismatch"):
    verify_usno_identities(repo_root=tmp_path, usno_root=usno_root)


@pytest.mark.parametrize(
  "mutation",
  [
    "v12-request",
    "v12-api-version",
    "v12-normalized-cell",
    "v12-record-count",
    "v12-normalization",
    "v13-longitude-sign",
    "v13-jd-request-conversion",
    "v14-civil-jd",
    "v14-six-decimal-rule",
    "v26-source-value-2000",
    "v26-source-value-2010",
    "v26-source-value-2014",
    "v26-rounding-relation",
    "v26-last-modified",
    "v26-tracked-hash",
    "v26-revision",
    "v29-timezone",
    "v29-era",
    "v29-jd",
  ],
)
def test_usno_record_mutations_fail_semantic_gate(tmp_path, mutation):
  usno_root = materialize_inputs(tmp_path)

  def change_v12(payload):
    if mutation == "v12-request":
      payload["records"][0]["request"]["date"] = "1999-06-15"
    elif mutation == "v12-api-version":
      payload["records"][0]["response"]["apiversion"] = "4.0.0"
    elif mutation == "v12-normalized-cell":
      payload["records"][0]["normalized_cells"]["rise"] = "11:59"
    elif mutation == "v12-normalization":
      payload["normalization"] = "first event wins"
    else:
      payload["records"].pop()

  def change_v13(payload):
    local = payload["records"][40]
    if mutation == "v13-longitude-sign":
      local["request"]["coords"] = "0,-74.0403"
    else:
      payload["records"][0]["request"]["time"] = "15:40:58"

  def change_v14(payload):
    if mutation == "v14-civil-jd":
      payload["events"][0]["civil_jd"] = "2460313.645834"
    else:
      payload["conversion"]["decimal_places"] = 7

  def change_v26(payload):
    if mutation.startswith("v26-source-value-"):
      index = {"v26-source-value-2000": 0, "v26-source-value-2010": 1, "v26-source-value-2014": 2}[mutation]
      payload["citation_relations"][index]["source_value"] = "0.0000"
    elif mutation == "v26-rounding-relation":
      payload["citation_relations"][1]["rounding_places"] = 2
    elif mutation == "v26-last-modified":
      payload["tracked_copy"]["last_modified"] = "Fri, 19 Jun 2026 17:26:25 GMT"
    elif mutation == "v26-tracked-hash":
      payload["tracked_copy"]["sha256"] = "0" * 64
    else:
      payload["revision"]["unchanged_overlap_values"] = 617

  def change_v29(payload):
    row = payload["current_recapture"]["response"]["data"][0]
    if mutation == "v29-timezone":
      row["tz"] = 1
    elif mutation == "v29-era":
      row["era"] = "BC"
    else:
      row["jd"] = "2460463.000001"

  if mutation.startswith("v12-"):
    mutate_record(usno_root, "v12-rstt-oneday.json", change_v12)
  elif mutation.startswith("v13-"):
    mutate_record(usno_root, "v13-siderealtime.json", change_v13)
  elif mutation.startswith("v14-"):
    mutate_record(usno_root, "v14-moon-phases-year-2024.json", change_v14)
  elif mutation.startswith("v26-"):
    mutate_record(usno_root, "v26-deltat.json", change_v26)
  else:
    mutate_record(usno_root, "v29-juliandate.json", change_v29)

  with pytest.raises(RuntimeError):
    verify_usno_identities(repo_root=tmp_path, usno_root=usno_root, record_hashes=record_hashes(usno_root))


def test_v03_silent_one_minute_resource_mutation_fails(tmp_path):
  usno_root = materialize_inputs(tmp_path)
  replace_once(
    tmp_path / "src/test/astro/moon_phase_test.cpp",
    "to_ymd(2024,  9,  3), hms {  9h + 56min }",
    "to_ymd(2024,  9,  3), hms {  9h + 55min }",
  )

  with pytest.raises(RuntimeError, match="V03 HKO/USNO minute relation differs"):
    verify_usno_identities(repo_root=tmp_path, usno_root=usno_root)
