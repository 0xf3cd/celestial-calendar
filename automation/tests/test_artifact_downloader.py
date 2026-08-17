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
import re
import zipfile

from pathlib import Path
from types import SimpleNamespace

import pytest
import yaml

import automation.github as github_module
import toolbox.artifact_downloader as artifact_downloader_module

from automation.github import GitHub
from toolbox.artifact_downloader import (
  ARTIFACT_SOURCES,
  PYTHON_ARTIFACTS,
  find_artifact_run,
  flatten_python_artifacts,
  project_version,
  validate_artifact_inventory,
)


PROJECT_VERSION = project_version()
NATIVE_WORKFLOW = Path(__file__).parents[2] / ".github" / "workflows" / "build_and_test.yml"


class Response:
  def raise_for_status(self):
    pass

  def json(self):
    return {
      "workflow_runs": [{
        "id": 7,
        "name": "WASM Build and Golden Check",
        "run_number": 11,
        "status": "completed",
        "conclusion": "success",
        "event": "workflow_dispatch",
        "head_sha": "tagged-sha",
        "workflow_id": 13,
        "url": "https://example.invalid/run/7",
        "created_at": "2026-08-15T00:00:00Z",
        "updated_at": "2026-08-15T00:01:00Z",
      }],
    }


class ArtifactResponse:
  def __init__(self, artifacts, total_count=None):
    self.artifacts = artifacts
    self.total_count = len(artifacts) if total_count is None else total_count

  def raise_for_status(self):
    pass

  def json(self):
    return {"total_count": self.total_count, "artifacts": self.artifacts}


class FailingDownloadResponse:
  def __enter__(self):
    return self

  def __exit__(self, *_args):
    pass

  def raise_for_status(self):
    pass

  def iter_content(self, chunk_size):
    assert chunk_size == 8192
    yield b"partial"
    raise OSError("connection lost")


def run(run_id: int, event: str) -> GitHub.Run:
  return GitHub.Run(
    run_id,
    "WASM Build and Golden Check",
    run_id,
    "completed",
    "success",
    event,
    "tagged-sha",
    13,
    f"https://example.invalid/run/{run_id}",
    "2026-08-15T00:00:00Z",
    "2026-08-15T00:01:00Z",
  )


def test_workflow_run_event_is_parsed(monkeypatch):
  monkeypatch.setattr(github_module, "gen_headers", lambda: {})
  monkeypatch.setattr(github_module.requests, "get", lambda *_args, **_kwargs: Response())

  runs, pages = GitHub.list_workflow_runs(13)

  assert pages == 1
  assert runs[0].event == "workflow_dispatch"


def test_artifact_lookup_rejects_pull_request_run(monkeypatch):
  pull_request = run(8, "pull_request")
  dispatched = run(7, "workflow_dispatch")
  monkeypatch.setattr(GitHub, "list_workflow_runs", lambda _workflow_id: ([pull_request, dispatched], 1))
  workflow = GitHub.Workflow(13, "WASM Build and Golden Check", "active", "", "", "")

  assert find_artifact_run(workflow, "tagged-sha") is dispatched


def test_artifact_api_preserves_duplicate_names(monkeypatch):
  artifacts = [
    {"name": "same", "archive_download_url": "https://example.invalid/one"},
    {"name": "same", "archive_download_url": "https://example.invalid/two"},
  ]
  monkeypatch.setattr(github_module, "gen_headers", lambda: {})
  monkeypatch.setattr(github_module.requests, "get", lambda *_args, **_kwargs: ArtifactResponse(artifacts))

  assert GitHub.get_artifacts_download_urls(7) == [
    ("same", "https://example.invalid/one"),
    ("same", "https://example.invalid/two"),
  ]


def test_artifact_api_rejects_truncated_response(monkeypatch):
  artifacts = [{"name": "one", "archive_download_url": "https://example.invalid/one"}]
  monkeypatch.setattr(github_module, "gen_headers", lambda: {})
  monkeypatch.setattr(
    github_module.requests,
    "get",
    lambda *_args, **_kwargs: ArtifactResponse(artifacts, total_count=2),
  )

  with pytest.raises(RuntimeError, match="returned 1 of 2"):
    GitHub.get_artifacts_download_urls(7)


@pytest.mark.parametrize(
  ("artifact_urls", "expected", "message"),
  [
    ([("one", "url")], frozenset({"one", "two"}), "missing"),
    ([("one", "url"), ("two", "url")], frozenset({"one"}), "extra"),
    ([("one", "url"), ("one", "other")], frozenset({"one"}), "duplicates"),
  ],
)
def test_artifact_inventory_is_exact(artifact_urls, expected, message):
  with pytest.raises(RuntimeError, match=message):
    validate_artifact_inventory("Workflow", 7, artifact_urls, expected, set())


def test_artifact_inventory_rejects_cross_source_collision():
  with pytest.raises(RuntimeError, match="cross-source"):
    validate_artifact_inventory("Workflow", 7, [("same", "url")], frozenset({"same"}), {"same"})


def test_native_workflow_artifact_inventory_matches_collector():
  workflow = yaml.safe_load(NATIVE_WORKFLOW.read_text(encoding="utf-8"))
  jobs = workflow["jobs"]
  linux = jobs["linux-docker"]
  linux_names = {
    entry["platform"].replace("/", "_") for entry in linux["strategy"]["matrix"]["include"]
  }
  linux_pack = next(step for step in linux["steps"] if step.get("id") == "shared_lib")
  assert "artifact_name=${OS}_${ARCH}" in linux_pack["run"]

  uploaded = set(linux_names)
  for job_name in ("macos", "windows"):
    job = jobs[job_name]
    pack = next(step for step in job["steps"] if step.get("id") == "shared_lib")
    names = set(re.findall(r"artifact_name=([a-z0-9_]+)", pack["run"]))
    assert len(names) == 1
    uploaded.update(names)

  for job_name in ("linux-docker", "macos", "windows"):
    upload = next(
      step
      for step in jobs[job_name]["steps"]
      if str(step.get("uses", "")).startswith("actions/upload-artifact@")
    )
    assert upload["with"]["name"] == "${{ steps.shared_lib.outputs.artifact_name }}"

  expected = next(names for name, names in ARTIFACT_SOURCES if name == workflow["name"])
  assert uploaded == expected


def test_download_rejects_existing_destination_before_request(monkeypatch, tmp_path):
  destination = tmp_path / "artifact.zip"
  destination.write_bytes(b"keep")

  def unexpected_request(*_args, **_kwargs):
    raise AssertionError("request must not start")

  monkeypatch.setattr(github_module.requests, "get", unexpected_request)
  with pytest.raises(FileExistsError, match="Refusing to overwrite"):
    GitHub.download_one_artifact("artifact", "https://example.invalid/artifact", tmp_path)
  assert destination.read_bytes() == b"keep"


def test_failed_download_leaves_no_final_or_partial_file(monkeypatch, tmp_path):
  monkeypatch.setattr(github_module, "gen_headers", lambda: {})
  monkeypatch.setattr(github_module.requests, "get", lambda *_args, **_kwargs: FailingDownloadResponse())

  with pytest.raises(OSError, match="connection lost"):
    GitHub.download_one_artifact("artifact", "https://example.invalid/artifact", tmp_path)

  assert list(tmp_path.iterdir()) == []


def test_parallel_download_rejects_duplicate_names_before_writes(tmp_path):
  with pytest.raises(RuntimeError, match="Duplicate artifact names"):
    GitHub.download_artifact_urls([("same", "one"), ("same", "two")], tmp_path)
  assert list(tmp_path.iterdir()) == []


@pytest.mark.parametrize(
  ("unzip", "validation_error"),
  [(False, None), (True, RuntimeError("invalid archive"))],
  ids=["success", "failure"],
)
def test_release_collector_validates_archives_before_python_flatten(
  monkeypatch,
  tmp_path,
  unzip,
  validation_error,
):
  workflows = {
    name: GitHub.Workflow(index, name, "active", "", "", "")
    for index, (name, _expected) in enumerate(ARTIFACT_SOURCES, start=1)
  }
  artifact_names = {
    index: expected
    for index, (_name, expected) in enumerate(ARTIFACT_SOURCES, start=1)
  }
  order = []
  downloaded_paths = []

  monkeypatch.setattr(
    artifact_downloader_module,
    "parse_args",
    lambda: SimpleNamespace(run_id=0, save_to=tmp_path, parallel=4, unzip=unzip),
  )
  monkeypatch.setattr(artifact_downloader_module, "validate_args", lambda _args: None)
  monkeypatch.setattr(artifact_downloader_module, "release_commit_sha", lambda: "tagged-sha")
  monkeypatch.setattr(artifact_downloader_module, "artifact_workflow", lambda name: workflows[name])
  monkeypatch.setattr(
    artifact_downloader_module,
    "find_artifact_run",
    lambda workflow, _sha: run(workflow.id, "workflow_dispatch"),
  )
  monkeypatch.setattr(
    GitHub,
    "get_artifacts_download_urls",
    lambda run_id: [(name, f"https://example.invalid/{name}") for name in artifact_names[run_id]],
  )

  def download(artifact_urls, save_to, _parallel):
    paths = []
    for name, _url in artifact_urls:
      path = save_to / f"{name}.zip"
      path.write_bytes(b"zip")
      paths.append(path)
    downloaded_paths.extend(paths)
    return paths

  def validate(paths, version):
    order.append("validate")
    assert version == PROJECT_VERSION
    assert {path.stem for path in paths} == set().union(*(expected for _name, expected in ARTIFACT_SOURCES))
    if validation_error is not None:
      raise validation_error

  def flatten(paths, save_to):
    order.append("flatten")
    assert save_to == tmp_path
    assert all(path.is_file() for path in paths)
    return []

  monkeypatch.setattr(GitHub, "download_artifact_urls", download)
  monkeypatch.setattr(artifact_downloader_module, "validate_release_archives", validate)
  monkeypatch.setattr(artifact_downloader_module, "flatten_python_artifacts", flatten)

  if validation_error is None:
    artifact_downloader_module.main()
    assert order == ["validate", "flatten"]
  else:
    with pytest.raises(RuntimeError, match="invalid archive"):
      artifact_downloader_module.main()
    assert order == ["validate"]
    assert downloaded_paths
    assert all(path.read_bytes() == b"zip" for path in downloaded_paths)


def python_wheels():
  return {
    "celestial-python-manylinux-x86_64": (
      f"celestial_calendar-{PROJECT_VERSION}-py3-none-manylinux_2_26_x86_64.manylinux_2_28_x86_64.whl"
    ),
    "celestial-python-manylinux-aarch64": (
      f"celestial_calendar-{PROJECT_VERSION}-py3-none-manylinux_2_28_aarch64.whl"
    ),
    "celestial-python-macos-arm64": f"celestial_calendar-{PROJECT_VERSION}-py3-none-macosx_14_0_arm64.whl",
    "celestial-python-windows-amd64": f"celestial_calendar-{PROJECT_VERSION}-py3-none-win_amd64.whl",
  }


def write_python_artifacts(directory, *, mutation=None):
  paths = []
  for artifact_name, wheel_name in python_wheels().items():
    wheel = f"wheel:{artifact_name}".encode()
    digest = hashlib.sha256(wheel).hexdigest()
    sidecar = f"{digest}  {wheel_name}\n".encode()
    members = {wheel_name: wheel, f"{wheel_name}.sha256": sidecar}
    if mutation is not None and artifact_name == "celestial-python-manylinux-x86_64":
      members = mutation(members, wheel_name)
    archive_path = directory / f"{artifact_name}.zip"
    with zipfile.ZipFile(archive_path, "w") as archive:
      for name, content in members.items():
        archive.writestr(name, content)
    paths.append(archive_path)
  assert set(path.stem for path in paths) == set(PYTHON_ARTIFACTS)
  return paths


def test_python_artifacts_flatten_after_global_validation(monkeypatch, tmp_path):
  archives = write_python_artifacts(tmp_path)
  temporary_directory = artifact_downloader_module.tempfile.TemporaryDirectory

  def stage_on_destination(*, prefix, dir):
    assert dir == tmp_path
    return temporary_directory(prefix=prefix, dir=dir)

  monkeypatch.setattr(artifact_downloader_module.tempfile, "TemporaryDirectory", stage_on_destination)

  flattened = flatten_python_artifacts(archives, tmp_path)

  assert len(flattened) == 8
  assert all(path.is_file() for path in flattened)
  assert all(not path.exists() for path in archives)
  assert len(list(tmp_path.glob("*.whl"))) == 4
  assert len(list(tmp_path.glob("*.whl.sha256"))) == 4


@pytest.mark.parametrize(
  "mutation",
  [
    lambda members, wheel_name: {**members, f"{wheel_name}.sha256": b"0" * 64 + b"  wrong.whl\n"},
    lambda members, _wheel_name: {**members, "unexpected.txt": b"extra"},
    lambda members, wheel_name: {f"{wheel_name}.sha256": members[f"{wheel_name}.sha256"]},
  ],
)
def test_python_artifact_mutations_write_no_flattened_payload(tmp_path, mutation):
  archives = write_python_artifacts(tmp_path, mutation=mutation)

  with pytest.raises(RuntimeError):
    flatten_python_artifacts(archives, tmp_path)

  assert list(tmp_path.glob("*.whl")) == []
  assert all(path.exists() for path in archives)


def test_python_artifact_rejects_duplicate_archive_member(tmp_path):
  archives = write_python_artifacts(tmp_path)
  archive_path = tmp_path / "celestial-python-manylinux-x86_64.zip"
  wheel_name = python_wheels()["celestial-python-manylinux-x86_64"]
  wheel = b"wheel"
  digest = hashlib.sha256(wheel).hexdigest()
  with zipfile.ZipFile(archive_path, "w") as archive:
    archive.writestr(wheel_name, wheel)
    with pytest.warns(UserWarning, match="Duplicate name"):
      archive.writestr(wheel_name, wheel)
    archive.writestr(f"{wheel_name}.sha256", f"{digest}  {wheel_name}\n")

  with pytest.raises(RuntimeError, match="Duplicate archive member"):
    flatten_python_artifacts(archives, tmp_path)

  assert list(tmp_path.glob("*.whl")) == []


@pytest.mark.parametrize(
  "wheel_name",
  [
    f"celestial_calendar-{PROJECT_VERSION}-py3-none-macosx_14_0_arm64.whl",
    f"celestial_calendar-{PROJECT_VERSION}-py3-none-manylinux_2_26_x86_64.whl",
    f"celestial_calendar-{PROJECT_VERSION}-py3-none-manylinux_2_31_x86_64.whl",
    "celestial_calendar-0.5.0-py3-none-manylinux_2_28_x86_64.whl",
  ],
)
def test_python_artifact_rejects_wrong_platform_or_version(tmp_path, wheel_name):
  def rename_wheel(members, original_name):
    wheel = members[original_name]
    digest = hashlib.sha256(wheel).hexdigest()
    return {wheel_name: wheel, f"{wheel_name}.sha256": f"{digest}  {wheel_name}\n".encode()}

  archives = write_python_artifacts(tmp_path, mutation=rename_wheel)

  with pytest.raises(RuntimeError):
    flatten_python_artifacts(archives, tmp_path)

  assert list(tmp_path.glob("*.whl")) == []
  assert all(path.exists() for path in archives)


def test_python_flatten_rejects_existing_destination_before_any_write(tmp_path):
  archives = write_python_artifacts(tmp_path)
  existing = tmp_path / next(iter(python_wheels().values()))
  existing.write_bytes(b"keep")

  with pytest.raises(FileExistsError, match="Refusing to overwrite"):
    flatten_python_artifacts(archives, tmp_path)

  assert existing.read_bytes() == b"keep"
  assert len(list(tmp_path.glob("*.whl"))) == 1
  assert list(tmp_path.glob("*.whl.sha256")) == []


def test_python_flatten_rejects_payload_basename_collision(monkeypatch, tmp_path):
  colliding_name = python_wheels()["celestial-python-manylinux-aarch64"]

  def collide(members, wheel_name):
    wheel = members[wheel_name]
    digest = hashlib.sha256(wheel).hexdigest()
    return {
      colliding_name: wheel,
      f"{colliding_name}.sha256": f"{digest}  {colliding_name}\n".encode(),
    }

  archives = write_python_artifacts(tmp_path, mutation=collide)
  monkeypatch.setattr(artifact_downloader_module, "validate_wheel_platform", lambda *_args: None)

  with pytest.raises(RuntimeError, match="Duplicate flattened Python payload basename"):
    flatten_python_artifacts(archives, tmp_path)

  assert list(tmp_path.glob("*.whl")) == []
  assert all(path.exists() for path in archives)
