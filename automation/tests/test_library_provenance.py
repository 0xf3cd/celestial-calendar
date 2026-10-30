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
from toolbox import make_bindings_golden


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


def import_revision(root):
  """Import a fixture revision without depending on the caller's Git identity or hooks."""
  parent = subprocess.run(["git", "rev-parse", "--verify", "HEAD"], cwd=root, capture_output=True, check=False)
  stream = b"commit refs/heads/main\ncommitter Fixture <fixture@example.invalid> 0 +0000\ndata 8\nfixture\n\n"
  if parent.returncode == 0:
    stream += b"from " + parent.stdout.strip() + b"\n"
  stream += b"deleteall\n"
  files = [root / ".gitignore", *sorted((root / "src").rglob("*"))]
  if (root / "README.md").exists():
    files.append(root / "README.md")
  for path in files:
    if path.is_file():
      data = path.read_bytes()
      stream += f"M 100644 inline {path.relative_to(root).as_posix()}\ndata {len(data)}\n".encode()
      stream += data + b"\n"
  subprocess.run(["git", "fast-import", "--quiet"], cwd=root, input=stream, check=True, capture_output=True)
  subprocess.run(["git", "symbolic-ref", "HEAD", "refs/heads/main"], cwd=root, check=True)
  subprocess.run(["git", "read-tree", "HEAD"], cwd=root, check=True)
  return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()


@pytest.fixture
def committed_source_tree(source_tree):
  root, build, library = source_tree
  subprocess.run(["git", "config", "core.autocrlf", "false"], cwd=root, check=True)
  (root / ".gitignore").write_bytes(b"build/\n")
  for path in (root / "src").rglob("*"):
    if path.is_file():
      path.write_bytes(path.read_bytes().replace(b"\r\n", b"\n"))
  commit = import_revision(root)
  assert subprocess.check_output(["git", "status", "--porcelain"], cwd=root) == b""
  return root, build, library, commit


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


def test_unstaged_production_deletion_is_part_of_the_snapshot(source_tree):
  root, build, library = source_tree
  subprocess.run(["git", "add", "src"], cwd=root, check=True)
  build_fixture(root, build, library)
  (root / "src/shared_lib/lib_jieqi.cpp").unlink()
  assert "src/shared_lib/lib_jieqi.cpp" not in provenance.source_snapshot(root)["files"]
  with pytest.raises(RuntimeError, match="snapshot"):
    provenance.verify_library(library, root)
  result, calls = build_fixture(root, build, library)
  assert result == 0
  assert "--clean-first" in calls[0]
  provenance.verify_library(library, root)


def test_clean_receipt_keeps_its_build_commit_after_head_moves(committed_source_tree):
  root, build, library, commit = committed_source_tree
  build_fixture(root, build, library)
  assert provenance.verify_library(library, root)["source"]["commit"] == commit
  (root / "README.md").write_text("non-production update\n", encoding="utf-8")
  assert import_revision(root) != commit
  assert provenance.verify_library(library, root)["source"]["commit"] == commit
  _, calls = build_fixture(root, build, library)
  assert "--clean-first" not in calls[0]
  assert provenance.verify_library(library, root)["source"]["commit"] == commit


def test_valid_commit_with_different_source_bytes_is_rejected(committed_source_tree):
  root, build, library, old_commit = committed_source_tree
  (root / "src/calendar/jieqi.hpp").write_text("new production source\n", encoding="utf-8")
  new_commit = import_revision(root)
  build_fixture(root, build, library)
  path = provenance.receipt_path(library)
  receipt = json.loads(path.read_text(encoding="utf-8"))
  assert receipt["source"]["commit"] == new_commit
  receipt["source"]["commit"] = old_commit
  path.write_text(json.dumps(receipt), encoding="utf-8")
  with pytest.raises(RuntimeError, match="source commit does not contain"):
    provenance.verify_library(library, root)


@pytest.mark.parametrize("converted", [False, True])
def test_only_byte_identical_commit_sources_receive_a_commit(committed_source_tree, converted):
  root, build, library, commit = committed_source_tree
  if converted:
    subprocess.run(["git", "config", "core.autocrlf", "true"], cwd=root, check=True)
    path = root / "src/calendar/jieqi.hpp"
    path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
    subprocess.run(["git", "add", "--renormalize", "src"], cwd=root, check=True)
  assert subprocess.check_output(["git", "status", "--porcelain"], cwd=root) == b""
  snapshot = provenance.source_snapshot(root)
  assert snapshot["commit"] == (None if converted else commit)
  assert snapshot["files"]["src/calendar/jieqi.hpp"] == provenance.file_sha256(root / "src/calendar/jieqi.hpp")
  build_fixture(root, build, library)
  assert provenance.verify_library(library, root)["source"]["commit"] == (None if converted else commit)


def test_missing_cmake_cache_has_a_recovery_message(source_tree):
  root, build, library = source_tree
  (build / "CMakeCache.txt").unlink()
  with pytest.raises(RuntimeError, match="project.py --cmake"):
    build_fixture(root, build, library)


def test_unversioned_library_is_preferred_to_accumulated_versions(tmp_path):
  extension = {"linux": ".so", "darwin": ".dylib", "win32": ".dll"}[sys.platform]
  unversioned = tmp_path / ("libcelestial_calendar" + extension)
  unversioned.write_bytes(b"current")
  for name in ("libcelestial_calendar.0.8.0" + extension, unversioned.name + ".bak"):
    (tmp_path / name).write_bytes(b"not selected")
  assert provenance.find_shared_lib(tmp_path) == unversioned


@pytest.mark.parametrize(
  "platform,name,foreign",
  [
    ("linux", "libcelestial_calendar.so.1.0.0", "celestial_calendar.dll"),
    ("darwin", "libcelestial_calendar.1.0.0.dylib", "libcelestial_calendar.so"),
    ("win32", "celestial_calendar.dll", "libcelestial_calendar.dylib"),
  ],
)
def test_golden_generator_uses_the_shared_platform_finder(tmp_path, monkeypatch, platform, name, foreign):
  library = tmp_path / name
  library.write_bytes(b"selected library")
  (tmp_path / foreign).write_bytes(b"foreign library")
  monkeypatch.setattr(sys, "platform", platform)
  monkeypatch.setattr(provenance.paths, "shared_lib_dir", lambda: tmp_path)
  loaded = []

  def verify(artifact, root):
    assert artifact == library
    assert root == provenance.paths.proj_root()
    loaded.append("verified")

  def load(path):
    loaded.append(path)
    raise RuntimeError("stopped before loading the fixture")

  monkeypatch.setattr(make_bindings_golden, "verify_library", verify)
  monkeypatch.setattr(make_bindings_golden.ctypes, "CDLL", load)
  with pytest.raises(RuntimeError, match="stopped before loading"):
    make_bindings_golden.generate()
  assert loaded == ["verified", str(library)]


@pytest.mark.parametrize("failure", ["missing", "artifact", "source"])
def test_golden_generator_rejects_unverified_accumulated_libraries(source_tree, monkeypatch, failure):
  root, build, unversioned = source_tree
  library = unversioned.with_name("libcelestial_calendar.so.0.9.0")
  unversioned.rename(library)
  monkeypatch.setattr(sys, "platform", "linux")
  build_fixture(root, build, library)
  if failure == "missing":
    provenance.receipt_path(library).unlink()
  elif failure == "artifact":
    library.write_bytes(b"stale artifact")
  else:
    (root / "src/calendar/jieqi.hpp").write_text("changed source\n", encoding="utf-8")
  monkeypatch.setattr(provenance.paths, "shared_lib_dir", lambda: library.parent)
  monkeypatch.setattr(provenance.paths, "proj_root", lambda: root)

  def forbidden(*args):
    pytest.fail("loaded an unverified accumulated library")

  monkeypatch.setattr(make_bindings_golden.ctypes, "CDLL", forbidden)
  with pytest.raises(RuntimeError, match="provenance"):
    make_bindings_golden.generate()


@pytest.mark.parametrize("missing", [False, True])
def test_golden_generator_propagates_a_missing_library(tmp_path, monkeypatch, missing):
  folder = tmp_path / "missing" if missing else tmp_path
  monkeypatch.setattr(provenance.paths, "shared_lib_dir", lambda: folder)

  def forbidden(*args):
    pytest.fail("loaded a library after discovery failed")

  monkeypatch.setattr(make_bindings_golden.ctypes, "CDLL", forbidden)
  with pytest.raises(FileNotFoundError):
    make_bindings_golden.generate()
