# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import json
import shutil
import subprocess
import sys

from importlib import metadata
from pathlib import Path

import pytest

from automation import env, library_provenance, paths
from automation.tests.test_library_provenance import source_tree as source_tree, import_revision, build_fixture
from automation.utils import ProcReturn
from toolbox import build_info


ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(params=["standalone", "nested"])
def source_only_tree(source_tree, tmp_path, request):
  original, _, _ = source_tree
  if request.param == "nested":
    import_revision(original)
    root = original / "archive"
  else:
    root = tmp_path / "archive"
  shutil.copytree(original / "src", root / "src")
  (root / "src/CMakeLists.txt").write_text("source layout fixture\n")
  shutil.copytree(ROOT / "automation", root / "automation", ignore=shutil.ignore_patterns("tests", "__pycache__"))
  for name in ("project.py", "checks.py", "Requirements.txt"):
    shutil.copy2(ROOT / name, root / name)
  build = root / "build"
  (build / "shared_lib").mkdir(parents=True)
  (build / "CMakeCache.txt").write_text(f"CMAKE_HOME_DIRECTORY:INTERNAL={root / 'src'}\n", encoding="utf-8")
  extension = {"linux": ".so", "darwin": ".dylib", "win32": ".dll"}[sys.platform]
  library = build / "shared_lib" / ("libcelestial_calendar" + extension)
  library.write_bytes(b"fixture artifact")
  return root, build, library


def test_source_only_real_entry_points(source_only_tree):
  root, _, _ = source_only_tree
  for script, argument in (("project.py", "--help"), ("checks.py", "--help"), ("project.py", "--version")):
    result = subprocess.run([sys.executable, "-B", str(root / script), argument], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert result.stdout.strip()


@pytest.mark.parametrize("source_only", [False, True], ids=["repository", "source-only"])
def test_routine_entries_without_optional_site_packages(source_only_tree, source_only):
  root = source_only_tree[0] if source_only else ROOT
  for script, argument in (("project.py", "--help"), ("checks.py", "--help"), ("project.py", "--version")):
    result = subprocess.run(
      [sys.executable, "-S", "-B", str(root / script), argument], cwd=root, capture_output=True, text=True
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert result.stdout.strip()
  probe = (
    "import importlib.util, sys; "
    "assert importlib.util.find_spec('markdown_it') is None; "
    "assert importlib.util.find_spec('yaml') is None; "
    "import project, checks; "
    "from automation import build_project, build_docs; "
    "assert callable(build_project) and callable(build_docs); "
    "assert 'automation.provenance' not in sys.modules"
  )
  result = subprocess.run([sys.executable, "-S", "-B", "-c", probe], cwd=root, capture_output=True, text=True)
  assert result.returncode == 0, result.stdout + result.stderr


def test_eager_provenance_import_reproduces_the_routine_entry_failure(source_only_tree):
  root, _, _ = source_only_tree
  package = root / "automation/__init__.py"
  package.write_text(package.read_text() + "\nfrom .provenance import check_provenance\n")
  result = subprocess.run(
    [sys.executable, "-S", "-B", str(root / "project.py"), "--version"], cwd=root, capture_output=True, text=True
  )
  assert result.returncode != 0
  assert "ModuleNotFoundError" in result.stderr and "markdown_it" in result.stderr


def test_layout_root_requires_source_markers(source_only_tree, monkeypatch):
  root, _, _ = source_only_tree
  monkeypatch.setattr(paths, "__file__", str(root / "automation/paths.py"))
  assert paths.proj_root() == root
  (root / "src/CMakeLists.txt").unlink()
  with pytest.raises(RuntimeError, match="source layout"):
    paths.proj_root()


@pytest.mark.parametrize("mutation", ["new", "changed", "deleted", "during-build", "configuration", "artifact"])
def test_source_only_receipts_keep_source_and_artifact_guards(source_only_tree, mutation):
  root, build, library = source_only_tree
  snapshot = library_provenance.source_snapshot(root)
  assert snapshot["commit"] is None
  assert set(snapshot["files"]) == {
    "src/CMakeLists.txt",
    "src/calendar/jieqi.hpp",
    "src/shared_lib/CMakeLists.txt",
    "src/shared_lib/lib_jieqi.cpp",
  }
  assert build_fixture(root, build, library)[0] == 0
  assert library_provenance.verify_library(library, root)["source"]["commit"] is None
  if mutation == "during-build":
    with pytest.raises(RuntimeError, match="changed during"):
      build_fixture(root, build, library, mutate=lambda: (root / "src/calendar/jieqi.hpp").write_text("changed"))
    assert not library_provenance.receipt_path(library).exists()
    return
  if mutation == "configuration":
    (build / "CMakeCache.txt").write_text("CMAKE_HOME_DIRECTORY:INTERNAL=/wrong\n")
    with pytest.raises(RuntimeError, match="does not belong"):
      build_fixture(root, build, library)
    return
  if mutation == "new":
    (root / "src/new.inc").write_text("new source")
  elif mutation == "changed":
    (root / "src/calendar/jieqi.hpp").write_text("changed")
  elif mutation == "deleted":
    (root / "src/shared_lib/lib_jieqi.cpp").unlink()
  else:
    library.write_bytes(b"different artifact")
  with pytest.raises(RuntimeError, match="snapshot|artifact"):
    library_provenance.verify_library(library, root)


def test_source_only_snapshot_excludes_tests_and_benchmarks(source_only_tree):
  root, _, _ = source_only_tree
  before = library_provenance.source_snapshot(root)
  for directory in ("test", "bench"):
    (root / "src" / directory).mkdir()
    (root / "src" / directory / "fixture.cpp").write_text("not production")
  assert library_provenance.source_snapshot(root) == before


def test_source_only_rebuild_does_not_reuse_a_commit_bearing_receipt(source_only_tree):
  root, build, library = source_only_tree
  build_fixture(root, build, library)
  receipt_path = library_provenance.receipt_path(library)
  receipt = json.loads(receipt_path.read_text())
  receipt["source"]["commit"] = "0" * 40
  receipt_path.write_text(json.dumps(receipt))
  with pytest.raises(RuntimeError, match="source-only tree cannot verify a recorded source commit"):
    library_provenance.verify_library(library, root)
  result, calls = build_fixture(root, build, library)
  assert result == 0 and "--clean-first" in calls[0]
  assert library_provenance.verify_library(library, root)["source"]["commit"] is None


def test_pack_build_info_without_own_git_never_uses_parent(source_only_tree, monkeypatch):
  root, _, library = source_only_tree
  monkeypatch.setattr(build_info, "__file__", str(root / "toolbox/build_info.py"))
  monkeypatch.setattr(paths, "__file__", str(root / "automation/paths.py"))
  monkeypatch.delenv("CXX", raising=False)
  monkeypatch.delenv("CC", raising=False)
  info = build_info.pack_build_info(None)
  assert info.git_commit == ""
  assert info.build_version == "1.0.0"
  assert info.sha256 == {library.name: library_provenance.file_sha256(library)}


@pytest.mark.parametrize("broken", [False, True])
def test_git_metadata_is_preserved_or_fails_without_fallback(source_tree, monkeypatch, broken):
  root, _, _ = source_tree
  (root / "src/CMakeLists.txt").write_text("source layout fixture\n")
  commit = import_revision(root)
  for name in ("project.py", "Requirements.txt"):
    shutil.copy2(ROOT / name, root / name)
  shutil.copytree(ROOT / "automation", root / "automation", ignore=shutil.ignore_patterns("tests", "__pycache__"))
  monkeypatch.setattr(build_info, "__file__", str(root / "toolbox/build_info.py"))
  monkeypatch.setattr(paths, "__file__", str(root / "automation/paths.py"))
  monkeypatch.delenv("CXX", raising=False)
  monkeypatch.delenv("CC", raising=False)
  if broken:
    (root / ".git/HEAD").write_text("broken HEAD\n")
    with pytest.raises(subprocess.CalledProcessError):
      library_provenance.source_snapshot(root)
    with pytest.raises(RuntimeError, match="Command failed"):
      build_info.pack_build_info(None)
  else:
    assert library_provenance.source_snapshot(root)["commit"] is None
    assert build_info.pack_build_info(None).git_commit == commit


@pytest.mark.parametrize("metadata_kind", ["dangling", "directory", "file"])
def test_broken_own_git_metadata_is_not_absent(source_only_tree, monkeypatch, metadata_kind):
  root, _, _ = source_only_tree
  if metadata_kind == "dangling":
    (root / ".git").symlink_to(root / "missing-git")
  elif metadata_kind == "directory":
    (root / ".git").mkdir()
  else:
    (root / ".git").write_text("broken gitfile\n")
  monkeypatch.setattr(build_info, "__file__", str(root / "toolbox/build_info.py"))
  monkeypatch.setattr(paths, "__file__", str(root / "automation/paths.py"))
  monkeypatch.delenv("CXX", raising=False)
  monkeypatch.delenv("CC", raising=False)
  with pytest.raises((RuntimeError, subprocess.CalledProcessError)):
    library_provenance.source_snapshot(root)
  with pytest.raises(RuntimeError, match="Command failed|does not identify this source root"):
    build_info.pack_build_info(None)


def test_git_executable_is_unneeded_only_without_metadata(source_only_tree, monkeypatch, tmp_path):
  root, _, _ = source_only_tree
  monkeypatch.setattr(build_info, "__file__", str(root / "toolbox/build_info.py"))
  monkeypatch.setattr(paths, "__file__", str(root / "automation/paths.py"))
  monkeypatch.delenv("CXX", raising=False)
  monkeypatch.delenv("CC", raising=False)
  empty_path = tmp_path / "empty-path"
  empty_path.mkdir()
  monkeypatch.setenv("PATH", str(empty_path))
  assert library_provenance.source_snapshot(root)["commit"] is None
  assert build_info.pack_build_info(None).git_commit == ""
  (root / ".git").mkdir()
  with pytest.raises(FileNotFoundError):
    library_provenance.source_snapshot(root)
  with pytest.raises(FileNotFoundError):
    build_info.pack_build_info(None)


@pytest.mark.parametrize("installed", [None, "1.2.3", "1.2.4"])
@pytest.mark.parametrize("requirement", ["distribution", "distribution==1.2.3", "distribution>=1.0", "distribution[x]"])
def test_installed_requirements_and_pip_delegation(tmp_path, monkeypatch, installed, requirement):
  requirements = tmp_path / "requirements.txt"
  requirements.write_text("# fixture\n" + requirement + " # comment\n")

  def version(name):
    assert name == "distribution"
    if installed is None:
      raise metadata.PackageNotFoundError(name)
    return installed

  monkeypatch.setattr(metadata, "version", version)
  monkeypatch.setattr(paths, "python_requirements", lambda: requirements)
  calls = []
  monkeypatch.setattr(env, "run_cmd", lambda command: calls.append(command) or ProcReturn(7, "", ""))
  satisfied = installed is not None and (requirement == "distribution" or requirement == "distribution==" + installed)
  assert env.unsatisfied_requirements(requirements) == ([] if satisfied else [requirement])
  assert env.install_dependencies() == (0 if satisfied else 7)
  assert calls == ([] if satisfied else [[sys.executable, "-m", "pip", "install", "-r", str(requirements)]])


@pytest.mark.parametrize("optimized", [False, True])
@pytest.mark.parametrize("case", ["missing-build", "file-build", "empty-standards"])
def test_configuration_guards_precede_actions_under_optimized_python(tmp_path, optimized, case):
  script = """
import sys
from pathlib import Path
from automation import build, env
def forbidden(*args, **kwargs):
  raise RuntimeError('ACTION WAS CALLED')
build.clear_test_binaries = forbidden
build.build_library_project = forbidden
env.check_cpp_support = forbidden
env.check_tool = forbidden
env.install_dependencies = forbidden
target = Path(sys.argv[1])
build.BUILD_DIR = target
if sys.argv[2] == 'file-build':
  target.write_text('not a directory')
if sys.argv[2] == 'empty-standards':
  env.setup_environment(env.SetupPlan(True, [env.Tool('fixture')], []))
else:
  build.build_project()
"""
  result = subprocess.run(
    [sys.executable, *(["-O"] if optimized else []), "-B", "-c", script, str(tmp_path / "build"), case],
    cwd=ROOT,
    capture_output=True,
    text=True,
  )
  assert result.returncode != 0
  assert "ACTION WAS CALLED" not in result.stderr
  assert {
    "missing-build": "Build directory not found",
    "file-build": "not a directory",
    "empty-standards": "No C++ standards specified",
  }[case] in result.stderr
