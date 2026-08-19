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

from toolbox import release_validation
from toolbox.artifact_downloader import project_version
from toolbox.build_npm import PACKAGE_NAME
from toolbox.release_validation import _native_layout, _runtime_matrix, validate_release_archives


VERSION = project_version()
MAJOR, MINOR, _PATCH = VERSION.split(".")
SOVERSION = f"{MAJOR}.{MINOR}" if MAJOR == "0" else MAJOR
TARBALL = f"0xf3cd-celestial-{VERSION}.tgz"
NATIVE_MEMBERS = {
  "linux_amd64.zip": [
    "build_info.json",
    "cpu_info.json",
    "include/celestial.h",
    "lib/libcelestial_calendar.so",
    f"lib/libcelestial_calendar.so.{SOVERSION}",
    f"lib/libcelestial_calendar.so.{VERSION}",
  ],
  "linux_arm64.zip": [
    "build_info.json",
    "cpu_info.json",
    "include/celestial.h",
    "lib/libcelestial_calendar.so",
    f"lib/libcelestial_calendar.so.{SOVERSION}",
    f"lib/libcelestial_calendar.so.{VERSION}",
  ],
  "macos_arm64.zip": [
    "build_info.json",
    "cpu_info.json",
    "include/celestial.h",
    "lib/libcelestial_calendar.dylib",
    f"lib/libcelestial_calendar.{SOVERSION}.dylib",
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
RUNTIME_FLOORS = {
  "linux_amd64.zip": {
    "supported": {"glibc": "2.28", "glibcxx": "3.4.21"},
    "measured": {"glibc": "2.26", "glibcxx": "3.4.21"},
  },
  "linux_arm64.zip": {
    "supported": {"glibc": "2.28", "glibcxx": "3.4.21"},
    "measured": {"glibc": "2.17", "glibcxx": "3.4.21"},
  },
  "macos_arm64.zip": {"supported": {"macos": "14.0"}, "measured": {"macos": "14.0"}},
  "windows_x86_64.zip": {
    "supported": {"windows": "not_declared"},
    "measured": {"msvc_runtime": "static"},
  },
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


def wasm_members(tarball_name=TARBALL, package_name=PACKAGE_NAME, package_version=VERSION):
  tarball = b"npm tarball"
  digest = hashlib.sha256(tarball).hexdigest()
  pack = [{"name": package_name, "version": package_version, "filename": tarball_name}]
  return [
    ("celestial-jieqi.mjs", b"mjs"),
    ("celestial-jieqi.wasm", b"wasm"),
    (tarball_name, tarball),
    ("npm-pack.json", json.dumps(pack).encode()),
    ("npm-pack.sha256", f"{digest}  {tarball_name}\n".encode()),
  ]


def write_wasm_archive(directory):
  return write_zip(directory / "celestial-wasm.zip", wasm_members())


def native_members(filename, build_version=VERSION):
  members = []
  hashes = {}
  for name in NATIVE_MEMBERS[filename]:
    content = f"payload:{name}".encode()
    members.append((name, content))
    if name.endswith((".so", f".so.{SOVERSION}", f".so.{VERSION}", ".dylib", ".dll")):
      hashes[name.rsplit("/", maxsplit=1)[-1]] = hashlib.sha256(content).hexdigest()
  build_info = {
    "build_version": build_version,
    "runtime_floor": RUNTIME_FLOORS[filename],
    "sha256": hashes,
  }
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


def test_wasm_tarball_name_comes_from_pack_metadata(tmp_path):
  archives = write_release_archives(tmp_path)
  write_zip(tmp_path / "celestial-wasm.zip", wasm_members(tarball_name="custom.tgz"))

  validate_release_archives(archives, VERSION)


@pytest.mark.parametrize(
  ("package_name", "package_version"),
  [("wrong-name", VERSION), (PACKAGE_NAME, "9.9.9")],
)
def test_wasm_archive_rejects_wrong_package_identity(tmp_path, package_name, package_version):
  archives = write_release_archives(tmp_path)
  write_zip(
    tmp_path / "celestial-wasm.zip",
    wasm_members(package_name=package_name, package_version=package_version),
  )

  with pytest.raises(RuntimeError, match="Invalid npm package identity"):
    validate_release_archives(archives, VERSION)


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


def test_native_archive_rejects_wrong_build_version(tmp_path):
  archives = write_release_archives(tmp_path)
  write_zip(tmp_path / "linux_amd64.zip", native_members("linux_amd64.zip", build_version="9.9.9"))

  with pytest.raises(RuntimeError, match="Build version mismatch"):
    validate_release_archives(archives, VERSION)


def test_readme_runtime_matrix_matches_reference_values():
  assert _runtime_matrix() == {
    filename.removesuffix(".zip"): floor for filename, floor in RUNTIME_FLOORS.items()
  }


@pytest.mark.parametrize("field", ["supported", "measured"])
def test_native_archive_rejects_runtime_floor_drift(tmp_path, field):
  archives = write_release_archives(tmp_path)
  filename = "linux_amd64.zip"
  members = native_members(filename)
  mutated = []
  for name, content in members:
    if name == "build_info.json":
      build_info = json.loads(content)
      build_info["runtime_floor"][field]["glibc"] = "2.29" if field == "supported" else "2.25"
      content = json.dumps(build_info).encode()
    mutated.append((name, content))
  write_zip(tmp_path / filename, mutated)

  with pytest.raises(RuntimeError, match="Runtime floor mismatch"):
    validate_release_archives(archives, VERSION)


def test_native_archive_rejects_measured_requirement_above_support(tmp_path):
  archives = write_release_archives(tmp_path)
  filename = "linux_amd64.zip"
  members = native_members(filename)
  mutated = []
  for name, content in members:
    if name == "build_info.json":
      build_info = json.loads(content)
      build_info["runtime_floor"]["measured"]["glibc"] = "2.34"
      content = json.dumps(build_info).encode()
    mutated.append((name, content))
  write_zip(tmp_path / filename, mutated)

  with pytest.raises(RuntimeError, match="Measured glibc requirement 2.34 exceeds supported 2.28"):
    validate_release_archives(archives, VERSION, check_documented_runtime=False)


def test_self_validation_does_not_read_checkout_runtime_matrix(tmp_path, monkeypatch):
  archives = write_release_archives(tmp_path)
  monkeypatch.setattr(
    release_validation,
    "_runtime_matrix",
    lambda: pytest.fail("self-validation read the checkout README"),
  )

  validate_release_archives(archives, VERSION, check_documented_runtime=False)


@pytest.mark.parametrize("filename", NATIVE_MEMBERS)
def test_each_native_archive_is_required_and_validated(tmp_path, filename):
  archives = write_release_archives(tmp_path)

  with pytest.raises(RuntimeError, match="Missing downloaded release archives"):
    validate_release_archives([path for path in archives if path.name != filename], VERSION)

  write_zip(tmp_path / filename, [*native_members(filename), ("unexpected.txt", b"extra")])
  with pytest.raises(RuntimeError, match=f"Invalid members in {filename}"):
    validate_release_archives(archives, VERSION)


def test_release_archive_inventory_rejects_duplicate_names(tmp_path):
  archives = write_release_archives(tmp_path)

  with pytest.raises(RuntimeError, match="Duplicate downloaded release archive"):
    validate_release_archives([*archives, archives[0]], VERSION)


def test_native_soversion_switches_to_major_at_v1():
  linux_members, _linux_runtime = _native_layout("linux_amd64", "1.0.0")
  macos_members, _macos_runtime = _native_layout("macos_arm64", "1.0.0")

  assert "lib/libcelestial_calendar.so.1" in linux_members
  assert "lib/libcelestial_calendar.so.1.0" not in linux_members
  assert "lib/libcelestial_calendar.1.dylib" in macos_members
  assert "lib/libcelestial_calendar.1.0.dylib" not in macos_members
