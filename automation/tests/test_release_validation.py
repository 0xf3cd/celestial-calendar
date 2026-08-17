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
import zipfile

import pytest

from toolbox.artifact_downloader import project_version
from toolbox.build_npm import PACKAGE_NAME
from toolbox.release_validation import validate_wasm_release_archive


VERSION = project_version()
TARBALL = f"0xf3cd-celestial-{VERSION}.tgz"


def write_zip(path, members):
  with zipfile.ZipFile(path, "w") as archive:
    seen = set()
    for name, content in members:
      if name in seen:
        with pytest.warns(UserWarning, match="Duplicate name"):
          archive.writestr(name, content)
      else:
        archive.writestr(name, content)
        seen.add(name)
  return path


def wasm_members():
  tarball = b"npm tarball"
  digest = hashlib.sha256(tarball).hexdigest()
  pack = [{"name": PACKAGE_NAME, "version": VERSION, "filename": TARBALL}]
  return [
    ("celestial-jieqi.mjs", b"mjs"),
    ("celestial-jieqi.wasm", b"wasm"),
    (TARBALL, tarball),
    ("npm-pack.json", json.dumps(pack).encode()),
    ("npm-pack.sha256", f"{digest}  {TARBALL}\n".encode()),
  ]


def write_wasm_archive(directory):
  return write_zip(directory / "celestial-wasm.zip", wasm_members())


def test_wasm_release_archive_validates_without_modification(tmp_path):
  archive = write_wasm_archive(tmp_path)
  before = archive.read_bytes()

  validate_wasm_release_archive([archive, tmp_path / "CHANGELOG.md"], VERSION)

  assert archive.read_bytes() == before


@pytest.mark.parametrize(
  "mutation",
  [
    lambda members: members[:-1],
    lambda members: [*members, ("unexpected.txt", b"extra")],
    lambda members: [*members, members[0]],
    lambda members: [
      (name, b"changed tarball" if name == TARBALL else content) for name, content in members
    ],
    lambda members: [
      (
        name,
        json.dumps([{"name": PACKAGE_NAME, "version": VERSION, "filename": "wrong.tgz"}]).encode()
        if name == "npm-pack.json"
        else content,
      )
      for name, content in members
    ],
  ],
)
def test_wasm_archive_mutations_fail_without_modification(tmp_path, mutation):
  wasm = write_zip(tmp_path / "celestial-wasm.zip", mutation(wasm_members()))
  before = wasm.read_bytes()

  with pytest.raises(RuntimeError):
    validate_wasm_release_archive([wasm], VERSION)

  assert wasm.read_bytes() == before


def test_wasm_release_archive_is_required(tmp_path):
  with pytest.raises(RuntimeError, match="Expected one downloaded celestial-wasm.zip"):
    validate_wasm_release_archive([tmp_path / "CHANGELOG.md"], VERSION)
