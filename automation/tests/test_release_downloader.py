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

import pytest

import toolbox.release_downloader as release_downloader_module

from automation.github import GitHub
from toolbox.release_downloader import archive_validation_version


def release(tag_name):
  return GitHub.Release(7, tag_name, False, False, "", "", tag_name, "", "", "", "", "", [])


@pytest.mark.parametrize(
  ("tag_name", "expected"),
  [("v0.5.0", None), ("v0.6.0", "0.6.0"), ("v1.2.3", "1.2.3")],
)
def test_archive_validation_starts_at_v060(tag_name, expected):
  assert archive_validation_version(tag_name) == expected


def test_archive_validation_rejects_unknown_tag_shape():
  with pytest.raises(RuntimeError, match="Cannot determine the archive contract"):
    archive_validation_version("latest")


def run_download(monkeypatch, tmp_path, tag_name):
  selected = release(tag_name)
  downloaded = [tmp_path / "celestial-wasm.zip", tmp_path / "CHANGELOG.md", tmp_path / "src.zip"]
  calls = []

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
    lambda paths, version: calls.append((paths, version)),
  )

  release_downloader_module.main()
  return downloaded, calls


def test_v060_release_download_reuses_archive_validation(monkeypatch, tmp_path):
  downloaded, calls = run_download(monkeypatch, tmp_path, "v0.6.0")

  assert calls == [(downloaded, "0.6.0")]


def test_historical_release_download_keeps_legacy_behavior(monkeypatch, tmp_path):
  _downloaded, calls = run_download(monkeypatch, tmp_path, "v0.5.0")

  assert calls == []
