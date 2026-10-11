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
import re

from pathlib import Path
from shutil import copy2

import pytest

from automation.batch_a_closeout import REPO_ROOT, verify_batch_a_closeout
from automation.internal_provenance import InternalProvenanceCounts, verify_internal_provenance
from automation.source_digest import RETAINED_REGISTRY, verify_retained_data
from automation.tests.test_batch_a_closeout import materialize_inputs, replace_once
from automation.tests.test_internal_provenance import materialize_inputs as materialize_internal
from automation.third_party_notices import NOTICE_SOURCES, assemble_notices
from automation.usno_identity import verify_usno_identities
from automation.vsop87d_import import PLANETS, verify_repository


DATA_BLOCKS = json.loads((REPO_ROOT / RETAINED_REGISTRY).read_text(encoding="utf-8"))["data_blocks"]


def materialize_block(destination: Path, block_id: str) -> Path:
  for relative in (RETAINED_REGISTRY, Path(DATA_BLOCKS[block_id]["path"])):
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / relative, target)
  return destination / DATA_BLOCKS[block_id]["path"]


@pytest.mark.parametrize("block_id", DATA_BLOCKS)
def test_every_retained_value_identity_rejects_a_mutation(tmp_path, block_id):
  path = materialize_block(tmp_path, block_id)
  assert verify_retained_data(tmp_path, (block_id,)) == 1
  block = DATA_BLOCKS[block_id]
  if block["method"] == "raw_sha256":
    path.write_bytes(path.read_bytes() + b"changed")
  elif block["method"] == "notebook_values":
    notebook = json.loads(path.read_text(encoding="utf-8"))
    cell = next(cell for cell in notebook["cells"] if cell.get("outputs"))
    cell["outputs"][0]["text"] = ["retained value changed: 17\n"]
    path.write_text(json.dumps(notebook), encoding="utf-8")
  else:
    text = path.read_text(encoding="utf-8")
    start = text.index(block["start"]) + len(block["start"])
    end = text.index(block["end"], start)
    active = re.sub(r"//[^\n]*|/\*.*?\*/", lambda match: " " * len(match.group()), text[start:end], flags=re.DOTALL)
    match = re.search(r"(?<![\w])\d+(?:\.\d+)?", active)
    assert match is not None, block_id
    begin, finish = start + match.start(), start + match.end()
    path.write_text(text[:begin] + "987654321" + text[finish:], encoding="utf-8")
  with pytest.raises(RuntimeError, match="retained data"):
    verify_retained_data(tmp_path, (block_id,))


@pytest.mark.parametrize("block_id", DATA_BLOCKS)
def test_live_host_maintenance_does_not_require_a_value_repin(tmp_path, block_id):
  path = materialize_block(tmp_path, block_id)
  block = DATA_BLOCKS[block_id]
  if block["method"] == "notebook_values":
    notebook = json.loads(path.read_text(encoding="utf-8"))
    notebook["metadata"]["maintenance_probe"] = "new metadata"
    for cell in notebook["cells"]:
      if cell["cell_type"] == "code":
        cell["execution_count"] = None
        for output in cell.get("outputs", []):
          if "execution_count" in output:
            output["execution_count"] = None
          output["metadata"] = {"maintenance_probe": True}
    path.write_text(json.dumps(notebook), encoding="utf-8")
  elif path.suffix == ".cpp":
    text = re.sub(r"TEST\(([^,]+),\s*([^)]+)\)", r"TEST(\1, Maintenance_\2)", path.read_text(encoding="utf-8"))
    path.write_text(text + "\nTEST(Maintenance, UnrelatedRegression) { EXPECT_EQ(17, 17); }\n", encoding="utf-8")
  elif path.suffix == ".hpp":
    path.write_text(path.read_text(encoding="utf-8") + "\n// Unrelated maintenance note.\n", encoding="utf-8")
  assert verify_retained_data(tmp_path, (block_id,)) == 1


def test_full_closeout_accepts_ordinary_maintenance(tmp_path):
  materialize_inputs(tmp_path)
  expected = verify_batch_a_closeout(tmp_path)
  sun = tmp_path / "src/test/astro/sun_test.cpp"
  replace_once(sun, "TEST(Sun, EquatorialApparentVsJplHorizons)", "TEST(Sun, RenamedHorizonsComparison)")
  sun.write_text(
    sun.read_text(encoding="utf-8") + "\nTEST(Sun, NewRegression) { EXPECT_EQ(17, 17); }\n", encoding="utf-8"
  )
  replace_once(
    tmp_path / "src/astro/earth.hpp",
    "Retained material boundary (R22):",
    "Retained material boundary (R22): retained series.",
  )
  notebook = tmp_path / "statistics/delta_t.ipynb"
  payload = json.loads(notebook.read_text(encoding="utf-8"))
  payload["metadata"]["maintenance"] = True
  notebook.write_text(json.dumps(payload), encoding="utf-8")
  new_source = tmp_path / "automation/maintenance_source.py"
  banner = "\n".join((REPO_ROOT / "automation/source_digest.py").read_text(encoding="utf-8").splitlines()[:11])
  new_source.write_text(banner + "\n\nVALUE = 17\n", encoding="utf-8")
  assert verify_batch_a_closeout(tmp_path) == expected
  assert verify_internal_provenance(tmp_path) == InternalProvenanceCounts(8, 42, 7)


def test_unrelated_same_shape_table_is_not_part_of_retained_data(tmp_path):
  materialize_internal(tmp_path)
  path = tmp_path / "src/test/astro/sun_test.cpp"
  original = path.read_text(encoding="utf-8")
  unrelated = (
    "TEST(Sun, Unrelated) {\n"
    "  const std::unordered_map<double, std::pair<double, double>> dataset { { 1.0, { 2.0, 3.0 } } };\n}\n"
  )
  path.write_text(unrelated + original, encoding="utf-8")
  assert verify_internal_provenance(tmp_path) == InternalProvenanceCounts(8, 42, 7)


@pytest.mark.parametrize("replacement", ["", "# CelestialCalendar:\n# SPDX-License-Identifier: MIT\n"])
def test_new_unmarked_source_is_rejected(tmp_path, replacement):
  materialize_inputs(tmp_path)
  (tmp_path / "automation/unmarked.py").write_text(replacement + "VALUE = 17\n", encoding="utf-8")
  with pytest.raises(RuntimeError, match="project banner differs"):
    verify_batch_a_closeout(tmp_path)


@pytest.mark.parametrize(
  "source", [source for source in NOTICE_SOURCES if source.facts], ids=lambda source: str(source.path)
)
def test_project_notice_prose_can_change_but_required_facts_cannot(tmp_path, source):
  for item in NOTICE_SOURCES:
    target = tmp_path / item.path
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / item.path, target)
  target = tmp_path / source.path
  text = target.read_text(encoding="utf-8")
  # Change the project heading while keeping source facts, then actually assemble the notices.
  revised = (
    "# Source attribution record\n" + text.partition("\n")[2]
    if target.suffix == ".md"
    else text + "\nThis project record supplies source attribution.\n"
  )
  target.write_text(revised, encoding="utf-8")
  assert assemble_notices(tmp_path) != assemble_notices()
  fact = source.facts[0]
  modified = target.read_text(encoding="utf-8")
  assert fact in modified
  target.write_text(modified.replace(fact, "unidentified supplier"), encoding="utf-8")
  with pytest.raises(RuntimeError, match="required fact"):
    assemble_notices(tmp_path)


@pytest.mark.parametrize(
  "source", [source for source in NOTICE_SOURCES if not source.facts], ids=lambda source: str(source.path)
)
def test_original_notice_inputs_keep_raw_byte_identity(tmp_path, source):
  for item in NOTICE_SOURCES:
    target = tmp_path / item.path
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / item.path, target)
  target = tmp_path / source.path
  target.write_bytes(target.read_bytes() + b"changed")
  with pytest.raises(RuntimeError, match="input hash mismatch"):
    assemble_notices(tmp_path)


@pytest.mark.parametrize(
  "block_id",
  [
    "v44-longitude-rate",
    "v20-longitude-rate",
    "v15-longitude-rate",
    "r38-longitude-rate",
    "v20-lunar-node",
    "v44-lunar-node",
  ],
)
def test_each_shared_supplier_relationship_is_independently_required(tmp_path, block_id):
  _record, registry_path = materialize_inputs(tmp_path)
  registry = json.loads(registry_path.read_text(encoding="utf-8"))
  entry = next(block for block in registry["blocks"] if block["id"] == block_id)
  entry["source_identity"] += " wrong source version"
  registry_path.write_text(json.dumps(registry), encoding="utf-8")
  with pytest.raises(RuntimeError, match="source/marking facts differ"):
    verify_batch_a_closeout(tmp_path)


def test_v37_last_digit_change_is_rejected(tmp_path):
  materialize_internal(tmp_path)
  replace_once(tmp_path / "src/test/astro/moon_test.cpp", "36.726233329781266", "36.726233329781276")
  with pytest.raises(RuntimeError, match="moon-0 retained data hash differs"):
    verify_internal_provenance(tmp_path)


def test_usno_generator_maintenance_and_test_renames_are_allowed(tmp_path):
  materialize_inputs(tmp_path)
  generator = tmp_path / "statistics/usno_validation_repro.py"
  generator.write_text(
    generator.read_text(encoding="utf-8") + "\ndef unrelated_helper():\n  return 17\n", encoding="utf-8"
  )
  for relative in ("src/test/astro/sidereal_time_test.cpp", "src/test/astro/moon_phase_test.cpp"):
    path = tmp_path / relative
    path.write_text(
      re.sub(r"TEST\(([^,]+),\s*([^)]+)\)", r"TEST(\1, Maintenance_\2)", path.read_text(encoding="utf-8")),
      encoding="utf-8",
    )
  counts = verify_usno_identities(tmp_path, tmp_path / "src/test/provenance/usno/2026-08-26")
  assert (counts.v13_records, counts.v14_events) == (60, 50)


def test_generated_header_prose_is_not_coefficient_identity(tmp_path):
  materialize_inputs(tmp_path)
  for spec in PLANETS:
    path = tmp_path / spec.generated_path
    path.write_text(path.read_text(encoding="utf-8") + "\n// Unrelated maintenance note.\n", encoding="utf-8")
  assert verify_repository(tmp_path).terms == 29152


def test_development_shas_are_not_source_version_pins(tmp_path):
  materialize_internal(tmp_path)
  anchors = (
    "202a0bd",
    "397422cc",
    "066c28db",
    "5101b6d2",
    "bc79c991",
    "b5f6c35",
    "a99d4d7",
    "f4b0980",
    "da22c87",
    "066c28d",
    "7ea26ee",
    "da333dd",
  )
  for path in (tmp_path / "src").rglob("*.hpp"):
    text = path.read_text(encoding="utf-8")
    for anchor in anchors:
      text = text.replace(anchor, "historical")
    path.write_text(text, encoding="utf-8")
  for path in (tmp_path / "src/test").rglob("*.cpp"):
    text = path.read_text(encoding="utf-8")
    for anchor in anchors:
      text = text.replace(anchor, "historical")
    path.write_text(text, encoding="utf-8")
  assert verify_internal_provenance(tmp_path) == InternalProvenanceCounts(8, 42, 7)


def test_project_prose_can_repeat_an_existing_licence_citation(tmp_path):
  materialize_inputs(tmp_path)
  path = tmp_path / "src/test/lunar/algo3_ytliu0_golden_test.cpp"
  path.write_text(
    path.read_text(encoding="utf-8") + "\n// Existing ytliu0 source terms: GPL" + "v3.\n", encoding="utf-8"
  )
  assert verify_batch_a_closeout(tmp_path).rows == 58


def test_printed_reference_input_units_are_retained(tmp_path):
  path = materialize_block(tmp_path, "AscendantMeeus14a")
  text = path.read_text(encoding="utf-8")
  start = text.index(DATA_BLOCKS["AscendantMeeus14a"]["start"])
  end = text.index("\n}", start)
  reference = text[start:end]
  assert reference.count("AngleDeg { 75.0 }") == 1
  revised = reference.replace("AngleDeg { 75.0 }", "AngleRad { 75.0 }")
  path.write_text(text[:start] + revised + text[end:], encoding="utf-8")
  with pytest.raises(RuntimeError, match="retained data hash differs"):
    verify_retained_data(tmp_path, ("AscendantMeeus14a",))


@pytest.mark.parametrize(
  "old", ["geometric mirrors", "all other rows are independent samples", "JPL DE441) via apparent()"]
)
def test_v37_independence_boundaries_are_required(tmp_path, old):
  materialize_internal(tmp_path)
  replace_once(tmp_path / "src/test/astro/sun_test.cpp", old, "unestablished comparison")
  with pytest.raises(RuntimeError, match="V37 sun-0 required fact"):
    verify_internal_provenance(tmp_path)


def test_source_pin_record_formatting_is_allowed(tmp_path):
  materialize_inputs(tmp_path)
  expected = verify_batch_a_closeout(tmp_path)
  pins = tmp_path / "src/test/provenance/longitude-rate/SOURCE_PINS.json"
  pins.write_text(json.dumps(json.loads(pins.read_text(encoding="utf-8")), sort_keys=True), encoding="utf-8")
  assert verify_batch_a_closeout(tmp_path) == expected


@pytest.mark.parametrize("fact", ["Second Edition", "p.99", "v2.10.3bfinal"])
def test_house_reference_identity_is_required(tmp_path, fact):
  materialize_inputs(tmp_path)
  path = tmp_path / "src/test/astro/house_test.cpp"
  path.write_text(path.read_text(encoding="utf-8").replace(fact, "unidentified reference"), encoding="utf-8")
  with pytest.raises(RuntimeError, match="House references required fact"):
    verify_batch_a_closeout(tmp_path)


def test_notebook_retained_input_table_cannot_change(tmp_path):
  key = "statistics/lunar_calendar.ipynb"
  path = materialize_block(tmp_path, key)
  notebook = json.loads(path.read_text(encoding="utf-8"))
  cell = next(cell for cell in notebook["cells"] if "JieqiPair(1912" in "".join(cell["source"]))
  cell["source"] = ["".join(cell["source"]).replace("JieqiPair(1912", "JieqiPair(1911")]
  path.write_text(json.dumps(notebook), encoding="utf-8")
  with pytest.raises(RuntimeError, match="retained data hash differs"):
    verify_retained_data(tmp_path, (key,))


def test_notebook_unrelated_code_can_be_added(tmp_path):
  key = "statistics/lunar_calendar.ipynb"
  path = materialize_block(tmp_path, key)
  notebook = json.loads(path.read_text(encoding="utf-8"))
  notebook["cells"].append({"cell_type": "code", "source": ["def unrelated():\n  return 17\n"], "outputs": []})
  path.write_text(json.dumps(notebook), encoding="utf-8")
  assert verify_retained_data(tmp_path, (key,)) == 1
