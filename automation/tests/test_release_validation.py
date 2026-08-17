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
from toolbox.release_validation import validate_release_archives


VERSION = project_version()
MAJOR_MINOR = ".".join(VERSION.split(".")[:2])
TARBALL = f"0xf3cd-celestial-{VERSION}.tgz"
NATIVE_MEMBERS = {
  "linux_amd64.zip": [
    "build_info.json",
    "cpu_info.json",
    "include/celestial.h",
    "lib/libcelestial_calendar.so",
    f"lib/libcelestial_calendar.so.{MAJOR_MINOR}",
    f"lib/libcelestial_calendar.so.{VERSION}",
  ],
  "linux_arm64.zip": [
    "build_info.json",
    "cpu_info.json",
    "include/celestial.h",
    "lib/libcelestial_calendar.so",
    f"lib/libcelestial_calendar.so.{MAJOR_MINOR}",
    f"lib/libcelestial_calendar.so.{VERSION}",
  ],
  "macos_arm64.zip": [
    "build_info.json",
    "cpu_info.json",
    "include/celestial.h",
    "lib/libcelestial_calendar.dylib",
    f"lib/libcelestial_calendar.{MAJOR_MINOR}.dylib",
    f"lib/libcelestial_calendar.{VERSION}.dylib",
  ],
  "windows_x86_64.zip": [
    "build_info.json",
    "cpu_info.json",
    "include/celestial.h",
    "bin/celestial_calendar.dll",
    "lib/celestial_calendar.lib",
  ],
}


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


def native_members(filename):
  members = []
  hashes = {}
  for name in NATIVE_MEMBERS[filename]:
    content = f"payload:{name}".encode()
    members.append((name, content))
    if name.endswith((".so", f".so.{MAJOR_MINOR}", f".so.{VERSION}", ".dylib", ".dll")):
      hashes[name.rsplit("/", maxsplit=1)[-1]] = hashlib.sha256(content).hexdigest()
  build_info = {"build_version": VERSION, "sha256": hashes}
  return [
    (name, json.dumps(build_info).encode() if name == "build_info.json" else content)
    for name, content in members
  ]


def write_release_archives(directory):
  paths = [write_wasm_archive(directory)]
  paths.extend(write_zip(directory / filename, native_members(filename)) for filename in NATIVE_MEMBERS)
  return paths


def test_release_archives_validate_without_modification(tmp_path):
  archives = write_release_archives(tmp_path)
  before = {path.name: path.read_bytes() for path in archives}

  validate_release_archives([*archives, tmp_path / "CHANGELOG.md"], VERSION)

  assert {path.name: path.read_bytes() for path in archives} == before


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
  archives = write_release_archives(tmp_path)
  wasm = write_zip(tmp_path / "celestial-wasm.zip", mutation(wasm_members()))
  before = wasm.read_bytes()

  with pytest.raises(RuntimeError):
    validate_release_archives(archives, VERSION)

  assert wasm.read_bytes() == before


@pytest.mark.parametrize(
  "mutation",
  [
    lambda members: members[:-1],
    lambda members: [*members, ("unexpected.txt", b"extra")],
    lambda members: [*members, members[-1]],
    lambda members: [
      (name, b"changed library" if name == "lib/libcelestial_calendar.so" else content)
      for name, content in members
    ],
    lambda members: [
      (
        name,
        json.dumps({"build_version": VERSION, "sha256": {}}).encode()
        if name == "build_info.json"
        else content,
      )
      for name, content in members
    ],
  ],
)
def test_native_archive_mutations_fail_without_modification(tmp_path, mutation):
  archives = write_release_archives(tmp_path)
  native = write_zip(tmp_path / "linux_amd64.zip", mutation(native_members("linux_amd64.zip")))
  before = native.read_bytes()

  with pytest.raises(RuntimeError):
    validate_release_archives(archives, VERSION)

  assert native.read_bytes() == before


def test_release_archive_inventory_is_complete(tmp_path):
  archives = write_release_archives(tmp_path)

  with pytest.raises(RuntimeError, match="Missing downloaded release archives"):
    validate_release_archives(archives[:-1], VERSION)
