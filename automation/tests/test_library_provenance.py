# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import json
import subprocess
import sys

import pytest

from automation import library_provenance as provenance
from automation.utils import ProcReturn


@pytest.fixture
def source_tree(tmp_path):
  root = tmp_path / "repo"
  root.mkdir()
  subprocess.run(["git", "init", str(root)], check=True, capture_output=True)
  for name in ("src/shared_lib/CMakeLists.txt", "src/calendar/jieqi.hpp", "src/shared_lib/lib_jieqi.cpp"):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("source fixture\n", encoding="utf-8")
  build = root / "build"
  (build / "shared_lib").mkdir(parents=True)
  (build / "CMakeCache.txt").write_text(f"CMAKE_HOME_DIRECTORY:INTERNAL={root / 'src'}\n", encoding="utf-8")
  extension = {"linux": ".so", "darwin": ".dylib", "win32": ".dll"}[sys.platform]
  library = build / "shared_lib" / ("libcelestial_calendar" + extension)
  library.write_bytes(b"previous artifact")
  return root, build, library


def build_fixture(root, build, library, *, mutate=None, retcode=0):
  calls = []

  def run(command, **kwargs):
    calls.append(command)
    library.write_bytes(b"new artifact")
    if mutate is not None:
      mutate()
    return ProcReturn(retcode, "", "")

  result = provenance.build_library_project(root, build, 2, run)
  return result, calls


def test_first_receipt_requires_fresh_build(source_tree):
  root, build, library = source_tree
  result, calls = build_fixture(root, build, library)
  assert result == 0
  assert "--clean-first" in calls[0]
  receipt = provenance.verify_library(library, root)
  assert receipt["source"]["commit"] is None
  assert receipt["artifact"]["sha256"] == provenance.file_sha256(library)


def test_unchanged_verified_build_can_be_incremental(source_tree):
  root, build, library = source_tree
  build_fixture(root, build, library)
  before = provenance.receipt_path(library).read_bytes()
  result, calls = build_fixture(root, build, library)
  assert result == 0
  assert "--clean-first" not in calls[0]
  assert provenance.receipt_path(library).read_bytes() == before


def test_old_binary_with_new_sources_is_rejected_and_rebuilt(source_tree):
  root, build, library = source_tree
  build_fixture(root, build, library)
  (root / "src/calendar/jieqi.hpp").write_text("changed numerical input\n", encoding="utf-8")
  with pytest.raises(RuntimeError, match="snapshot"):
    provenance.verify_library(library, root)
  result, calls = build_fixture(root, build, library)
  assert result == 0
  assert "--clean-first" in calls[0]
  provenance.verify_library(library, root)


@pytest.mark.parametrize("failure", ["missing", "json", "artifact", "snapshot", "commit", "configuration"])
def test_missing_or_inconsistent_provenance_fails(source_tree, failure):
  root, build, library = source_tree
  build_fixture(root, build, library)
  path = provenance.receipt_path(library)
  if failure == "missing":
    path.unlink()
  elif failure == "json":
    path.write_text("not JSON", encoding="utf-8")
  elif failure == "artifact":
    library.write_bytes(b"a different native library")
  else:
    receipt = json.loads(path.read_text(encoding="utf-8"))
    if failure == "snapshot":
      receipt["source"]["snapshot_sha256"] = "0" * 64
    elif failure == "commit":
      receipt["source"]["commit"] = "not-a-source-commit"
    else:
      receipt["configuration_sha256"] = None
    path.write_text(json.dumps(receipt), encoding="utf-8")
  with pytest.raises(RuntimeError, match="provenance"):
    provenance.verify_library(library, root)


def test_failed_build_invalidates_previous_receipt(source_tree):
  root, build, library = source_tree
  build_fixture(root, build, library)
  result, _ = build_fixture(root, build, library, retcode=7)
  assert result == 7
  assert not provenance.receipt_path(library).exists()


def test_source_changes_during_build_do_not_get_a_receipt(source_tree):
  root, build, library = source_tree

  def change():
    (root / "src/calendar/jieqi.hpp").write_text("changed while compiling\n", encoding="utf-8")

  with pytest.raises(RuntimeError, match="changed during"):
    build_fixture(root, build, library, mutate=change)
  assert not provenance.receipt_path(library).exists()


def test_wrong_build_tree_is_rejected(source_tree):
  root, build, library = source_tree
  (build / "CMakeCache.txt").write_text("CMAKE_HOME_DIRECTORY:INTERNAL=/different/source\n", encoding="utf-8")
  with pytest.raises(RuntimeError, match="does not belong"):
    build_fixture(root, build, library)


def test_new_production_input_invalidates_snapshot(source_tree):
  root, build, library = source_tree
  build_fixture(root, build, library)
  (root / "src/calendar/new_coefficients.inc").write_text("new include dependency\n", encoding="utf-8")
  with pytest.raises(RuntimeError, match="snapshot"):
    provenance.verify_library(library, root)
