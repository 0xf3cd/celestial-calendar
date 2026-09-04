# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import hashlib
import json

from pathlib import Path
from shutil import copy2, copytree, ignore_patterns

import pytest

from automation.batch_a_closeout import (
  A4_SCAN_EXCLUDED_PARTS,
  A4_SCAN_FILES,
  A4_SCAN_ROOTS,
  CLOSEOUT_ROOT_RELATIVE,
  GPL_V3,
  MIT_LICENSE_BYTES,
  MIT_SPDX_MARKER,
  OLD_FULL_HEADER_MARKER,
  OLD_SHORT_HEADER_MARKER,
  RECORD_NAME,
  RECORD_SHA256,
  REGISTRY_NAME,
  REGISTRY_SHA256,
  REPO_ROOT,
  CloseoutCounts,
  verify_batch_a_closeout,
)
from automation.third_party_notices import SEPARATOR


def _source_payloads() -> tuple[dict, dict]:
  root = REPO_ROOT / CLOSEOUT_ROOT_RELATIVE
  return (
    json.loads((root / RECORD_NAME).read_text(encoding="utf-8")),
    json.loads((root / REGISTRY_NAME).read_text(encoding="utf-8")),
  )


def _referenced_paths(record: dict, registry: dict) -> set[Path]:
  paths = {Path("src/astro/vsop87d/defines.hpp")}
  for row in record["rows"].values():
    evidence = row["evidence"]
    if evidence["kind"] == "existing_gate":
      paths.add(Path(evidence["path"]))
    elif evidence["kind"] == "implementation_locations":
      paths.update(Path(path) for path in evidence["paths"])
    elif evidence["kind"] == "direct_digest":
      paths.update(Path(block["path"]) for block in evidence["blocks"])
  for block in registry["blocks"]:
    paths.add(Path(block["path"]))
    if block["marking_mode"] == "adjacent_record":
      paths.add(Path(block["adjacent_path"]))
  return paths


def materialize_inputs(destination: Path) -> tuple[Path, Path]:
  record, registry = _source_payloads()
  closeout_root = destination / CLOSEOUT_ROOT_RELATIVE
  closeout_root.mkdir(parents=True)
  record_path = closeout_root / RECORD_NAME
  registry_path = closeout_root / REGISTRY_NAME
  copy2(REPO_ROOT / CLOSEOUT_ROOT_RELATIVE / RECORD_NAME, record_path)
  copy2(REPO_ROOT / CLOSEOUT_ROOT_RELATIVE / REGISTRY_NAME, registry_path)
  for relative in _referenced_paths(record, registry):
    target = destination / relative
    if target in {record_path, registry_path}:
      continue
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / relative, target)
  for relative in A4_SCAN_ROOTS:
    copytree(
      REPO_ROOT / relative,
      destination / relative,
      dirs_exist_ok=True,
      ignore=ignore_patterns(*sorted(A4_SCAN_EXCLUDED_PARTS), "*.ipynb"),
    )
  for relative in A4_SCAN_FILES:
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / relative, target)
  return record_path, registry_path


def write_json(path: Path, payload: dict) -> str:
  path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
  return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_once(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  assert text.count(old) == 1
  path.write_text(text.replace(old, new), encoding="utf-8")


def test_batch_a_closeout_records_are_pinned_and_complete():
  assert RECORD_SHA256 == "fcc705215221f6924d0406728f48b19166f197a7dc72389b6d48dc50cf7959fb"
  assert REGISTRY_SHA256 == "2f31482e49d51f1f7b80ad979ece00fda088a16161b41dd45ae456ffe32cfe6e"
  assert verify_batch_a_closeout() == CloseoutCounts(57, 90, 47, 2, 14)


@pytest.mark.parametrize(
  ("mutation", "message"),
  [
    ("missing-row", "closeout row inventory differs"),
    ("unexpected-row", "closeout row inventory differs"),
    ("incomplete-split", "R17 split-row assignment is incomplete"),
    ("wrong-shape", "V18 disposition/material/terms shape differs"),
    ("v18-unavailable", "V18 unavailable fields differ"),
    ("v04-underlying", "V04 underlying-source boundary differs"),
    ("v37-partition", "V37 source partition differs"),
    ("t03-pending", "T03 capture boundary differs"),
    ("permission-claim", "V03 must not claim upstream permission"),
    ("v03-partition", "V03 HKO/USNO partition differs"),
    ("unknown-evidence", "V30 evidence kind is unknown"),
  ],
)
def test_closeout_row_mutations_fail(tmp_path, mutation, message):
  record_path, _registry_path = materialize_inputs(tmp_path)
  record = json.loads(record_path.read_text(encoding="utf-8"))
  rows = record["rows"]
  if mutation == "missing-row":
    del rows["V18"]
  elif mutation == "unexpected-row":
    rows["V42"] = dict(rows["V18"])
  elif mutation == "incomplete-split":
    del rows["R17"]["covered_part"]
  elif mutation == "wrong-shape":
    rows["V18"]["terms_shape"] = "symmetric_silence"
  elif mutation == "v18-unavailable":
    rows["V18"]["unavailable"] = ["original generator", "original PyMeeus version"]
  elif mutation == "v04-underlying":
    rows["V04"]["unavailable"].remove("underlying HMNAO/USNO rights allocation")
  elif mutation == "v37-partition":
    rows["V37"]["source_identity"] = "PyMeeus-derived legacy rows"
  elif mutation == "t03-pending":
    rows["T03"]["current_reproducibility"] = "positive link evidence captured"
  elif mutation == "permission-claim":
    rows["V03"]["permission_claim"] = True
  elif mutation == "v03-partition":
    rows["V03"]["source_facts"]["retained_hko_utc"] = "2024-09-03T01:55Z"
  else:
    rows["V30"]["evidence"] = {"kind": "unknown"}
  digest = write_json(record_path, record)

  with pytest.raises(RuntimeError, match=message):
    verify_batch_a_closeout(repo_root=tmp_path, record_sha256=digest)


def test_duplicate_row_key_fails(tmp_path):
  record_path, _registry_path = materialize_inputs(tmp_path)
  text = record_path.read_text(encoding="utf-8")
  row_start = text.index('    "R01":')
  row_end = text.index("\n", row_start) + 1
  duplicate = text[row_start:row_end]
  record_path.write_text(text[:row_end] + duplicate + text[row_end:], encoding="utf-8")
  digest = hashlib.sha256(record_path.read_bytes()).hexdigest()

  with pytest.raises(RuntimeError, match="duplicate JSON key: R01"):
    verify_batch_a_closeout(repo_root=tmp_path, record_sha256=digest)


def test_direct_digest_detects_a_retained_value_change(tmp_path):
  materialize_inputs(tmp_path)
  source = tmp_path / "src/astro/vsop87d/earth_coeff.hpp"
  replace_once(source, "175347045.673", "175347045.674")

  with pytest.raises(RuntimeError, match="R12 direct digest differs"):
    verify_batch_a_closeout(repo_root=tmp_path)


def test_vsop_table_manifest_is_independently_reconciled(tmp_path):
  record_path, _registry_path = materialize_inputs(tmp_path)
  record = json.loads(record_path.read_text(encoding="utf-8"))
  record["rows"]["R12"]["source_facts"]["repository_tables"]["L0"]["rows"] = 558
  digest = write_json(record_path, record)

  with pytest.raises(RuntimeError, match="R12 table record differs: L0"):
    verify_batch_a_closeout(repo_root=tmp_path, record_sha256=digest)


@pytest.mark.parametrize(
  ("mutation", "message"),
  [
    ("missing-block", "retained registry inventory differs"),
    ("duplicate-locator", "duplicate retained locator"),
    ("wrong-scope", "r12 scope differs"),
    ("wrong-source", "r12 source identity is not recognised"),
    ("changed-marker", "r12 retained marking differs"),
    ("changed-marking-hash", "r12 retained marking hash differs"),
    ("unknown-mode", "r12 marking mode differs"),
    ("notice-map", "notice applicability mapping differs"),
  ],
)
def test_registry_mutations_fail(tmp_path, mutation, message):
  _record_path, registry_path = materialize_inputs(tmp_path)
  registry = json.loads(registry_path.read_text(encoding="utf-8"))
  blocks = registry["blocks"]
  by_id = {block["id"]: block for block in blocks}
  if mutation == "missing-block":
    blocks.remove(by_id["r12"])
  elif mutation == "duplicate-locator":
    by_id["r13"]["path"] = by_id["r14"]["path"]
    by_id["r13"]["locator"] = by_id["r14"]["locator"]
    by_id["r13"]["marker"] = by_id["r14"]["marker"]
    by_id["r13"]["marking_sha256"] = by_id["r14"]["marking_sha256"]
  elif mutation == "wrong-scope":
    by_id["r12"]["material_scope"] = "project_authored_mit"
  elif mutation == "wrong-source":
    by_id["r12"]["source_identity"] = "unidentified source"
  elif mutation == "changed-marker":
    by_id["r12"]["marker"] = "Retained material boundary (R12): changed"
  elif mutation == "changed-marking-hash":
    by_id["r12"]["marking_sha256"] = "0" * 64
  elif mutation == "unknown-mode":
    by_id["r12"]["marking_mode"] = "notice_only"
  else:
    by_id["notice-emscripten"]["notice_title"] = "ERFA v2.0.1 — LICENSE"
  digest = write_json(registry_path, registry)

  with pytest.raises(RuntimeError, match=message):
    verify_batch_a_closeout(repo_root=tmp_path, registry_sha256=digest)


def test_registry_source_must_match_its_owning_row(tmp_path):
  _record_path, registry_path = materialize_inputs(tmp_path)
  registry = json.loads(registry_path.read_text(encoding="utf-8"))
  block = next(item for item in registry["blocks"] if item["id"] == "r22")
  block["source_identity"] = "IAU SOFA issue 2023-10-11"
  digest = write_json(registry_path, registry)

  with pytest.raises(RuntimeError, match="r22 source identity differs from its owning row"):
    verify_batch_a_closeout(repo_root=tmp_path, registry_sha256=digest)


def test_adjacent_data_hash_mutation_fails(tmp_path):
  record_path, _registry_path = materialize_inputs(tmp_path)
  data = tmp_path / "statistics/moon_phases.csv"
  data.write_bytes(data.read_bytes() + b"\n")
  record = json.loads(record_path.read_text(encoding="utf-8"))
  record["rows"]["V30"]["evidence"]["blocks"][0]["sha256"] = hashlib.sha256(data.read_bytes()).hexdigest()
  digest = write_json(record_path, record)

  with pytest.raises(RuntimeError, match="v30 retained data hash differs"):
    verify_batch_a_closeout(repo_root=tmp_path, record_sha256=digest)


def test_adjacent_record_mutation_fails(tmp_path):
  materialize_inputs(tmp_path)
  attribution = tmp_path / CLOSEOUT_ROOT_RELATIVE / "ATTRIBUTION.md"
  replace_once(attribution, "notebook-only new/full Moon", "changed new/full Moon")

  with pytest.raises(RuntimeError, match="v26 adjacent record hash differs"):
    verify_batch_a_closeout(repo_root=tmp_path)


def test_orphan_adjacent_section_fails(tmp_path):
  _record_path, registry_path = materialize_inputs(tmp_path)
  attribution = tmp_path / CLOSEOUT_ROOT_RELATIVE / "ATTRIBUTION.md"
  attribution.write_text(attribution.read_text(encoding="utf-8") + "\n## V99: orphan\n", encoding="utf-8")
  adjacent_sha256 = hashlib.sha256(attribution.read_bytes()).hexdigest()
  registry = json.loads(registry_path.read_text(encoding="utf-8"))
  for block in registry["blocks"]:
    if block["marking_mode"] == "adjacent_record":
      block["adjacent_sha256"] = adjacent_sha256
  digest = write_json(registry_path, registry)

  with pytest.raises(RuntimeError, match="orphan or missing adjacent section"):
    verify_batch_a_closeout(repo_root=tmp_path, registry_sha256=digest)


def test_orphan_in_file_marking_fails(tmp_path):
  materialize_inputs(tmp_path)
  cmake = tmp_path / "src/CMakeLists.txt"
  cmake.write_text(
    cmake.read_text(encoding="utf-8")
    + "\n# Retained material boundary (R99): orphan material remains outside the project MIT grant.\n",
    encoding="utf-8",
  )

  with pytest.raises(RuntimeError, match="orphan retained marking"):
    verify_batch_a_closeout(repo_root=tmp_path)


def test_identity_gate_host_coverage_is_independent(tmp_path):
  _record_path, registry_path = materialize_inputs(tmp_path)
  registry = json.loads(registry_path.read_text(encoding="utf-8"))
  entry = next(block for block in registry["blocks"] if block["id"] == "v07-refresh")
  v07 = next(block for block in registry["blocks"] if block["id"] == "v07")
  entry["path"] = "src/test/astro/sun_test.cpp"
  entry["locator"] = "V07 refresh relation"
  entry["marker"] = "Retained material boundaries: the V07 JPL Horizons DE440 table"
  entry["marking_sha256"] = v07["marking_sha256"]
  digest = write_json(registry_path, registry)

  with pytest.raises(RuntimeError, match="identity-gate hosts lack registry entries"):
    verify_batch_a_closeout(repo_root=tmp_path, registry_sha256=digest)


def test_a4_license_surfaces_are_exact_and_complete(tmp_path):
  materialize_inputs(tmp_path)

  assert (tmp_path / "LICENSE").read_bytes() == MIT_LICENSE_BYTES
  assert verify_batch_a_closeout(repo_root=tmp_path) == CloseoutCounts(57, 90, 47, 2, 14)


@pytest.mark.parametrize(
  ("relative", "marker", "message"),
  [
    ("automation/action_pins.py", OLD_FULL_HEADER_MARKER, "old full project header remains"),
    ("project.py", OLD_SHORT_HEADER_MARKER, "old short project header remains"),
  ],
)
def test_a4_old_header_positive_controls_reject_injected_markers(tmp_path, relative, marker, message):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / relative, MIT_SPDX_MARKER, marker)

  with pytest.raises(RuntimeError, match=message):
    verify_batch_a_closeout(repo_root=tmp_path)


@pytest.mark.parametrize(
  ("relative", "old", "new", "message"),
  [
    (
      "automation/action_pins.py",
      MIT_SPDX_MARKER,
      "SPDX-License-Identifier: Apache-2.0",
      "MIT SPDX project header differs",
    ),
    (
      "run-clang-tidy.py",
      "SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception",
      MIT_SPDX_MARKER,
      "run-clang-tidy.py upstream SPDX marking differs",
    ),
    (
      "bindings/javascript/package.json",
      '"license": "MIT"',
      '"license": "ISC"',
      "npm project license metadata differs",
    ),
    ("bindings/python/pyproject.toml", 'license = "MIT"', 'license = "ISC"', "Python license metadata differs"),
    ("toolbox/build_npm.py", '"license": "MIT",', '"license": "ISC",', "staged npm license differs"),
    (
      "bindings/python/test/wheel/verify.py",
      'metadata["License-Expression"] == "MIT"',
      'metadata["License-Expression"] == "ISC"',
      "wheel License-Expression expectation differs",
    ),
    (
      "bindings/javascript/README.md",
      "licensed under MIT",
      "licensed permissively",
      "package README MIT scope differs",
    ),
    (
      "bindings/python/README.md",
      "THIRD_PARTY_NOTICES.txt",
      "NOTICE.txt",
      "package README third-party exception differs",
    ),
    (
      "README.md",
      "Standalone third-party files and data",
      "Other files",
      "README third-party exception pointer differs",
    ),
    (
      "AGENTS.md",
      "Project-authored material: MIT;",
      "License: permissive;",
      "AGENTS.md repository license scope differs",
    ),
    ("statistics/algo3_ytliu0_golden.py", GPL_V3, "GPL" + "v4", "residual GPL allowlist differs"),
  ],
)
def test_a4_text_surface_mutations_fail(tmp_path, relative, old, new, message):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / relative, old, new)

  with pytest.raises(RuntimeError, match=message):
    verify_batch_a_closeout(repo_root=tmp_path)


def test_a4_license_mutation_fails(tmp_path):
  materialize_inputs(tmp_path)
  (tmp_path / "LICENSE").write_bytes(MIT_LICENSE_BYTES + b"changed\n")

  with pytest.raises(RuntimeError, match="root LICENSE is not canonical MIT text"):
    verify_batch_a_closeout(repo_root=tmp_path)


def test_a4_npm_lock_mutation_fails(tmp_path):
  materialize_inputs(tmp_path)
  package_lock = tmp_path / "bindings/javascript/package-lock.json"
  payload = json.loads(package_lock.read_text(encoding="utf-8"))
  payload["packages"][""]["license"] = "ISC"
  write_json(package_lock, payload)

  with pytest.raises(RuntimeError, match="npm root lock license differs"):
    verify_batch_a_closeout(repo_root=tmp_path)


def test_a4_gate_allows_future_version_and_release_notes(tmp_path):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "project.py", 'BUILD_VERSION: Final[str] = "0.7.0"', 'BUILD_VERSION: Final[str] = "0.8.0"')
  release_notes = tmp_path / "docs/RELEASE_NOTES.md"
  replace_once(release_notes, "## [v0.7.0]", "## [v0.8.0]")
  replace_once(
    release_notes,
    "Project-authored material is now licensed under MIT",
    "This release contains future changes",
  )

  assert verify_batch_a_closeout(repo_root=tmp_path) == CloseoutCounts(57, 90, 47, 2, 14)


def test_mit_spdx_population_gate_includes_unheaded_retained_hosts(tmp_path):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "src/CMakeLists.txt", MIT_SPDX_MARKER, "")

  with pytest.raises(RuntimeError, match="MIT SPDX host marking differs"):
    verify_batch_a_closeout(repo_root=tmp_path)


def test_t03_terms_capture_must_match_the_approved_evidence(tmp_path):
  materialize_inputs(tmp_path)
  contract_path = tmp_path / "automation/windows_toolchain_contract.json"
  contract = json.loads(contract_path.read_text(encoding="utf-8"))
  contract["approved_evidence"]["native"]["terms"]["terms_text_captured"] = False
  write_json(contract_path, contract)

  with pytest.raises(RuntimeError, match="T03 native terms capture differs"):
    verify_batch_a_closeout(repo_root=tmp_path)


@pytest.mark.parametrize("producer", ["native", "wheel"])
def test_t03_terms_document_identities_must_match_the_approved_evidence(tmp_path, producer):
  materialize_inputs(tmp_path)
  contract_path = tmp_path / "automation/windows_toolchain_contract.json"
  contract = json.loads(contract_path.read_text(encoding="utf-8"))
  contract["approved_evidence"][producer]["terms"]["documents"][0]["sha256"] = "0" * 64
  write_json(contract_path, contract)

  with pytest.raises(RuntimeError, match=f"T03 {producer} terms document identities differ"):
    verify_batch_a_closeout(repo_root=tmp_path)


def test_mit_spdx_host_identity_swap_fails(tmp_path):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "checks.py", "Ningqi Wang (0xf3cd)", "Anonymous")
  extra = tmp_path / "automation/a4_probe_extra.py"
  extra.write_text(
    f"# Ningqi Wang (0xf3cd)\n# https://github.com/0xf3cd/celestial-calendar\n# {MIT_SPDX_MARKER}\n",
    encoding="utf-8",
  )

  with pytest.raises(RuntimeError, match="MIT SPDX project-header inventory differs"):
    verify_batch_a_closeout(repo_root=tmp_path)


@pytest.mark.parametrize(
  ("old", "new", "message"),
  [
    ("under its source", "under changed source", "r22 retained marking hash differs"),
    (
      "// terms and outside the project MIT grant; written permission",
      "// terms; written permission",
      "r22 MIT boundary is missing",
    ),
  ],
)
def test_retained_marking_body_mutation_fails(tmp_path, old, new, message):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "src/astro/earth.hpp", old, new)

  with pytest.raises(RuntimeError, match=message):
    verify_batch_a_closeout(repo_root=tmp_path)


def test_notice_separator_mutation_fails(tmp_path):
  materialize_inputs(tmp_path)
  notice_path = tmp_path / "THIRD_PARTY_NOTICES.txt"
  text = notice_path.read_text(encoding="utf-8")
  separator = SEPARATOR.decode()
  assert separator in text
  notice_path.write_text(text.replace(separator, "-" * 78 + "\n"), encoding="utf-8")

  with pytest.raises(RuntimeError, match="notice-emscripten notice separator differs"):
    verify_batch_a_closeout(repo_root=tmp_path)
