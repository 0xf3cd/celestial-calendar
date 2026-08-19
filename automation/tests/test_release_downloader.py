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

from types import SimpleNamespace
from pathlib import Path

import pytest
import yaml

import toolbox.release_downloader as release_downloader_module

from automation.github import GitHub
from toolbox.release_downloader import archive_validation_version


RELEASE_WORKFLOW = Path(__file__).parents[2] / ".github" / "workflows" / "release.yml"


def release(tag_name):
  return GitHub.Release(7, tag_name, False, False, "", "", tag_name, "", "", "", "", "", [])


@pytest.mark.parametrize(
  ("tag_name", "expected"),
  [("v0.5.0", None), ("v0.6.0", "0.6.0"), ("v0.10.0", "0.10.0"), ("v1.2.3", "1.2.3")],
)
def test_archive_validation_starts_at_v060(tag_name, expected):
  assert archive_validation_version(tag_name) == expected


@pytest.mark.parametrize("tag_name", ["latest", "v0.6.0-rc1"])
def test_archive_validation_rejects_unknown_tag_shape(tag_name):
  with pytest.raises(RuntimeError, match="Cannot determine the archive contract"):
    archive_validation_version(tag_name)


def run_download(monkeypatch, tmp_path, tag_name, validator=None):
  selected = release(tag_name)
  downloaded = [tmp_path / "celestial-wasm.zip", tmp_path / "CHANGELOG.md", tmp_path / "src.zip"]
  calls = []
  for path in downloaded:
    path.write_bytes(b"downloaded")

  def record_validation(paths, version, check_documented_runtime, require_wheels):
    calls.append((paths, version, check_documented_runtime, require_wheels))

  monkeypatch.setattr(
    release_downloader_module,
    "parse_args",
    lambda: SimpleNamespace(id=None, tag=tag_name, save_to=tmp_path, parallel=4),
  )
  monkeypatch.setattr(release_downloader_module, "validate_args", lambda _args: None)
  monkeypatch.setattr(release_downloader_module, "find_release", lambda _keyword: selected)
  monkeypatch.setattr(GitHub, "download_release", lambda _id, _save_to, _parallel: downloaded)
  monkeypatch.setattr(
    release_downloader_module,
    "validate_release_archives",
    validator or record_validation,
  )

  release_downloader_module.main()
  return downloaded, calls


def test_v060_release_download_reuses_archive_validation(monkeypatch, tmp_path):
  downloaded, calls = run_download(monkeypatch, tmp_path, "v0.6.0")

  assert calls == [(downloaded, "0.6.0", False, True)]


def test_release_download_validates_against_tag_version(monkeypatch, tmp_path):
  downloaded, calls = run_download(monkeypatch, tmp_path, "v1.2.3")

  assert calls == [(downloaded, "1.2.3", False, True)]


def test_release_download_preserves_assets_when_validation_fails(monkeypatch, tmp_path):
  downloaded = [tmp_path / "celestial-wasm.zip", tmp_path / "CHANGELOG.md", tmp_path / "src.zip"]

  def reject_archives(paths, version, check_documented_runtime, require_wheels):
    assert paths == downloaded
    assert version == "0.6.0"
    assert check_documented_runtime is False
    assert require_wheels is True
    raise RuntimeError("invalid archive")

  with pytest.raises(RuntimeError, match="invalid archive"):
    run_download(monkeypatch, tmp_path, "v0.6.0", validator=reject_archives)

  assert all(path.read_bytes() == b"downloaded" for path in downloaded)


def test_historical_release_download_keeps_legacy_behavior(monkeypatch, tmp_path):
  _downloaded, calls = run_download(monkeypatch, tmp_path, "v0.5.0")

  assert calls == []


def test_release_workflow_installs_dependencies_before_downloading_artifacts():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  steps = workflow["jobs"]["create_release"]["steps"]
  names = [step.get("name") for step in steps]

  setup = next(step for step in steps if step.get("name") == "Set up Python")
  assert setup["with"]["python-version"] == "3.12"
  assert names.index("Install Python Dependencies") < names.index("Download Artifacts")
  install = next(step for step in steps if step.get("name") == "Install Python Dependencies")
  assert install["run"] == "python3 -m pip install -r Requirements.txt"


def test_release_workflow_validates_document_versions():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  steps = workflow["jobs"]["create_release"]["steps"]
  sanity = next(step for step in steps if step.get("name") == "Sanity Check on Version")
  assert "validate_release_document_versions" in sanity["run"]
  assert '"$TAG_NAME"' in sanity["run"]
