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
import subprocess
import sys

from pathlib import Path

import pytest

from automation import provenance, paths
from automation.retained_provenance import verify_retained_provenance, REGISTRY_ROOT_RELATIVE
from automation.source_digest import RETAINED_REGISTRY, notebook_values, retained_digest
from automation.tests.test_retained_provenance import materialize_inputs, replace_once, write_json
from automation.third_party_notices import verify_notices


ROOT = Path(__file__).resolve().parents[2]


def test_actual_ordered_roster_runs_once(monkeypatch):
  calls = []

  def observe(name, verify):
    def run(root):
      calls.append(name)
      return verify(root)

    return run

  roster = provenance.VERIFIERS
  monkeypatch.setattr(provenance, "VERIFIERS", tuple((name, observe(name, verify)) for name, verify in roster))
  assert provenance.check_provenance() == 0
  assert calls == [name for name, _ in roster]
  assert len(calls) == len(set(calls))


def test_direct_script_entry():
  result = subprocess.run([sys.executable, "-B", "automation/provenance.py"], cwd=ROOT, capture_output=True, text=True)
  assert result.returncode == 0, result.stdout + result.stderr
  assert result.stdout.count("Provenance verified [") == len(provenance.VERIFIERS)


def test_missing_id_is_contextual_nonzero_through_actual_gate(tmp_path, capsys):
  record_path, _ = materialize_inputs(tmp_path)
  record = json.loads(record_path.read_text())
  record["rows"]["V38"]["evidence"]["ids"].append("absent-retained-id")
  write_json(record_path, record)
  assert provenance.check_provenance(tmp_path) == 1
  output = capsys.readouterr().out
  assert "[Retained sources and values]" in output and "retained data ID is missing: absent-retained-id" in output


def test_notices_are_compared_not_only_assembled(tmp_path, capsys):
  materialize_inputs(tmp_path)
  verify_notices(tmp_path)
  notice = tmp_path / "THIRD_PARTY_NOTICES.txt"
  notice.write_bytes(notice.read_bytes() + b"different committed bundle\n")
  with pytest.raises(RuntimeError, match="not the canonical assembled output"):
    verify_notices(tmp_path)
  assert provenance.check_provenance(tmp_path) == 1
  assert "[Third-party notices]" in capsys.readouterr().out


@pytest.mark.parametrize("entry", ["r38-pluto", "r38-pluto-evaluator", "earth-runtime-0", "daily-variation-constant"])
@pytest.mark.parametrize("mutation", ["entry-removal", "value", "duplicate-marker", "missing-marker"])
def test_new_marker_owners_retain_discrimination(tmp_path, entry, mutation):
  record_path, registry_path = materialize_inputs(tmp_path)
  registry = json.loads(registry_path.read_text())
  record = json.loads(record_path.read_text())
  block = registry["data_blocks"][entry]
  source = tmp_path / block["path"]
  old, new = {
    "r38-pluto": ("-19.799805", "-19.799804"),
    "r38-pluto-evaluator": ("238.958116 +", "238.958117 +"),
    "earth-runtime-0": ("118.568", "118.569"),
    "daily-variation-constant": ("return 3548.330", "return 3548.331"),
  }[entry]
  if mutation == "entry-removal":
    del registry["data_blocks"][entry]
    for host in registry["blocks"]:
      if host.get("data_id") == entry:
        del host["data_id"]
    for row in record["rows"].values():
      evidence = row["evidence"]
      if evidence["kind"] == "retained_data" and entry in evidence["ids"]:
        evidence["ids"].remove(entry)
        if not evidence["ids"]:
          row["evidence"] = {"kind": "not_applicable"}
    write_json(record_path, record)
    write_json(registry_path, registry)
    replace_once(source, old, new)
    message = "retained start marker needs exactly one data entry"
  elif mutation == "value":
    replace_once(source, old, new)
    message = "retained data hash differs"
  else:
    marker = block["start"]
    replace_once(source, marker, marker + "\n" + marker if mutation == "duplicate-marker" else "// Removed start.")
    message = (
      "retained data start is missing or ambiguous"
      if entry in {"earth-runtime-0", "daily-variation-constant"}
      else "retained (data entry lacks its unique start marker|start marker needs exactly one data entry)"
    )
  with pytest.raises(RuntimeError, match=message):
    verify_retained_provenance(tmp_path)


def remove_entry_and_evidence(root, entry):
  registry_path = root / RETAINED_REGISTRY
  registry = json.loads(registry_path.read_text())
  del registry["data_blocks"][entry]
  write_json(registry_path, registry)
  record_path = root / REGISTRY_ROOT_RELATIVE / "record.json"
  record = json.loads(record_path.read_text())
  for row in record["rows"].values():
    evidence = row["evidence"]
    if evidence["kind"] == "retained_data" and entry in evidence["ids"]:
      evidence["ids"].remove(entry)
      if not evidence["ids"]:
        row["evidence"] = {"kind": "not_applicable"}
  write_json(record_path, record)


WHOLE_FILES = [
  key
  for key, value in json.loads((ROOT / RETAINED_REGISTRY).read_text())["data_blocks"].items()
  if value["method"] in {"notebook_values", "raw_sha256"}
]


@pytest.mark.parametrize("entry", WHOLE_FILES)
def test_whole_file_entry_removal_reaches_existing_owner(tmp_path, entry):
  materialize_inputs(tmp_path)
  verify_retained_provenance(tmp_path)
  remove_entry_and_evidence(tmp_path, entry)
  if entry == "statistics/moon_phases.csv":
    verify_retained_provenance(tmp_path)
    source = tmp_path / entry
    source.write_bytes(source.read_bytes() + b"\n")
    message = "v30 retained data hash differs"
  else:
    message = "retained whole-file owner needs exactly one"
  with pytest.raises(RuntimeError, match=message):
    verify_retained_provenance(tmp_path)


@pytest.mark.parametrize("entry", WHOLE_FILES)
def test_whole_file_value_mutation_fails_real_verifier(tmp_path, entry):
  materialize_inputs(tmp_path)
  source = tmp_path / entry
  if source.suffix == ".ipynb":
    notebook = json.loads(source.read_text())
    cell = next(cell for cell in notebook["cells"] if cell.get("outputs"))
    cell["outputs"][0]["text"] = ["changed retained value 1234"]
    source.write_text(json.dumps(notebook))
  else:
    source.write_bytes(source.read_bytes() + b"changed")
  with pytest.raises(RuntimeError, match="retained data hash differs"):
    verify_retained_provenance(tmp_path)


@pytest.mark.parametrize("mutation", ["entry-removal", "input-value"])
def test_cleared_outputs_leave_directed_notebook_inputs_owned(tmp_path, mutation):
  materialize_inputs(tmp_path)
  entry = "statistics/lunar_calendar.ipynb"
  source = tmp_path / entry
  notebook = json.loads(source.read_text())
  for cell in notebook["cells"]:
    cell["outputs"] = []
  source.write_text(json.dumps(notebook))
  payload = notebook_values(source)
  assert payload["outputs"] == []
  assert sum(map(len, payload["directed_jieqi_inputs"])) == 6
  registry_path = tmp_path / RETAINED_REGISTRY
  registry = json.loads(registry_path.read_text())
  registry["data_blocks"][entry]["sha256"] = retained_digest(
    source, "notebook_values", input_facts=tuple(registry["data_blocks"][entry]["input_facts"])
  )
  write_json(registry_path, registry)
  verify_retained_provenance(tmp_path)
  if mutation == "input-value":
    replace_once(source, "JieqiPair(1912", "JieqiPair(1911")
    with pytest.raises(RuntimeError, match="retained data hash differs"):
      verify_retained_provenance(tmp_path)
    return
  remove_entry_and_evidence(tmp_path, entry)
  with pytest.raises(RuntimeError, match="retained whole-file owner needs exactly one notebook_values"):
    verify_retained_provenance(tmp_path)


def test_empty_notebook_payload_needs_no_entry(tmp_path):
  materialize_inputs(tmp_path)
  (tmp_path / "statistics/empty.ipynb").write_text(json.dumps({"cells": [], "metadata": {"example": 17}}))
  verify_retained_provenance(tmp_path)


def test_duplicate_whole_file_path_is_rejected(tmp_path):
  _, registry_path = materialize_inputs(tmp_path)
  registry = json.loads(registry_path.read_text())
  registry["data_blocks"]["duplicate"] = dict(registry["data_blocks"][paths.BINDING_GOLDEN_RELATIVE.as_posix()])
  write_json(registry_path, registry)
  with pytest.raises(RuntimeError, match="has no evidence or host anchor|needs exactly one"):
    verify_retained_provenance(tmp_path)


def test_project_source_cannot_become_an_spdx_exception(tmp_path):
  _, registry_path = materialize_inputs(tmp_path)
  registry = json.loads(registry_path.read_text())
  registry["non_project_spdx_hosts"].append("src/astro/pluto.hpp")
  write_json(registry_path, registry)
  with pytest.raises(RuntimeError, match="non-project SPDX exception lacks upstream marking"):
    verify_retained_provenance(tmp_path)


@pytest.mark.parametrize("mutation", ["duplicate-host", "missing-notice", "missing-owner"])
def test_registry_relations_survive_outer_pin_removal(tmp_path, mutation):
  _, registry_path = materialize_inputs(tmp_path)
  registry = json.loads(registry_path.read_text())
  host = next(block for block in registry["blocks"] if block["id"] == "r38-pluto")
  if mutation == "duplicate-host":
    registry["blocks"].append(dict(host))
    message = "block IDs are duplicated"
  elif mutation == "missing-notice":
    del host["notice_title"]
    message = "distributed retained host lacks its notice mapping"
  else:
    host["owner"] = "no source row"
    message = "owner names no row"
  write_json(registry_path, registry)
  with pytest.raises(RuntimeError, match=message):
    verify_retained_provenance(tmp_path)


def test_duplicate_data_json_key_fails_at_the_shared_reader(tmp_path):
  _, registry_path = materialize_inputs(tmp_path)
  text = registry_path.read_text()
  text = text.replace('"data_blocks": {', '"data_blocks": {"duplicate": {}, "duplicate": {},', 1)
  registry_path.write_text(text)
  with pytest.raises(RuntimeError, match="duplicate JSON key: duplicate"):
    verify_retained_provenance(tmp_path)


def test_end_comments_and_string_occurrences_are_not_start_markers(tmp_path):
  materialize_inputs(tmp_path)
  source = tmp_path / "src/astro/pluto.hpp"
  source.write_text(
    source.read_text() + '\n// End retained data: future.\nconst auto text = "// Retained data: future.";\n'
  )
  verify_retained_provenance(tmp_path)


def test_binding_generator_and_all_replay_paths_agree():
  from toolbox.make_bindings_golden import OUT_PATH

  assert OUT_PATH == ROOT / paths.BINDING_GOLDEN_RELATIVE
  for name in ("golden_replay.py", "events_replay.py"):
    source = ROOT / "bindings/python/test/consumer" / name
    expression = re.search(r"^GOLDEN = (.+)$", source.read_text(), re.MULTILINE)
    assert expression is not None
    assert eval(expression[1], {"REPO": ROOT}) == OUT_PATH
  for relative in ("browser/browser_test.mjs", "abi/raw_protocol_test.mjs"):
    source = ROOT / "bindings/javascript/test" / relative
    text = source.read_text()
    if relative.startswith("abi/"):
      match = re.search(r'readFile\(resolve\(REPO, "([^"]+)"\)', text)
      assert match is not None and ROOT / match[1] == OUT_PATH
    else:
      match = re.search(r'const golden = .*readFile\(new URL\("([^"]+)", import.meta.url\)', text)
      assert match is not None and (source.parent / match[1]).resolve() == OUT_PATH
