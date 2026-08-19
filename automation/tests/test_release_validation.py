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
from toolbox.release_validation import (
  _native_layout,
  _runtime_matrix,
  validate_release_archives,
  validate_release_document_versions,
)


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


def write_wheel(directory, content=b"wheel"):
  wheel = directory / f"celestial_calendar-{VERSION}-py3-none-manylinux_2_28_x86_64.whl"
  wheel.write_bytes(content)
  sidecar = directory / f"{wheel.name}.sha256"
  sidecar.write_text(f"{hashlib.sha256(content).hexdigest()}  {wheel.name}\n", encoding="utf-8")
  return wheel, sidecar


def test_release_archives_validate_without_modification(tmp_path):
  archives = write_release_archives(tmp_path)
  before = {path.name: path.read_bytes() for path in archives}

  validate_release_archives([*archives, tmp_path / "CHANGELOG.md"], VERSION)

  assert {path.name: path.read_bytes() for path in archives} == before


def test_downloaded_wheel_sidecar_validates_without_modification(tmp_path):
  archives = write_release_archives(tmp_path)
  wheel, sidecar = write_wheel(tmp_path)
  before = {path.name: path.read_bytes() for path in (wheel, sidecar)}

  validate_release_archives([*archives, wheel, sidecar], VERSION)

  assert {path.name: path.read_bytes() for path in (wheel, sidecar)} == before


@pytest.mark.parametrize("mutation", ["missing", "mismatch", "orphan"])
def test_downloaded_wheel_sidecar_mutations_fail(tmp_path, mutation):
  archives = write_release_archives(tmp_path)
  wheel, sidecar = write_wheel(tmp_path)
  if mutation == "missing":
    sidecar.unlink()
    downloaded = [*archives, wheel]
  elif mutation == "mismatch":
    sidecar.write_text(f"{'0' * 64}  {wheel.name}\n", encoding="utf-8")
    downloaded = [*archives, wheel, sidecar]
  else:
    wheel.unlink()
    downloaded = [*archives, sidecar]

  with pytest.raises(RuntimeError, match="sidecar"):
    validate_release_archives(downloaded, VERSION)


def test_release_document_versions_match_tag(tmp_path):
  release_notes = tmp_path / "RELEASE_NOTES.md"
  changelog = tmp_path / "CHANGELOG.md"
  release_notes.write_text("Release notes\n\n## [v0.6.0] - 2026-08-17\n", encoding="utf-8")
  changelog.write_text("# Changelog\n\n## [v0.6.0] - 2026-08-17\n", encoding="utf-8")

  validate_release_document_versions("v0.6.0", (release_notes, changelog))


@pytest.mark.parametrize(
  ("contents", "message"),
  [
    ("# Release notes\n", "Cannot find a version heading"),
    ("## [v0.5.0] - 2026-08-15\n", "expected 0.6.0, found 0.5.0"),
  ],
)
def test_release_document_versions_reject_missing_or_stale_heading(tmp_path, contents, message):
  document = tmp_path / "RELEASE_NOTES.md"
  document.write_text(contents, encoding="utf-8")

  with pytest.raises(RuntimeError, match=message):
    validate_release_document_versions("v0.6.0", (document,))


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


@pytest.mark.parametrize(
  ("runtime_floor_value", "message"),
  [
    ([], "Invalid runtime floor"),
    ({"supported": {"glibc": "2.28"}}, "Invalid runtime floor"),
    ({"supported": {}, "measured": {"glibc": "2.26"}}, "Invalid runtime floor"),
    ({"supported": {"glibc": "2.28"}, "measured": {"glibc": 2.26}}, "Invalid runtime floor"),
    ({"supported": {"": "2.28"}, "measured": {"glibc": "2.26"}}, "Invalid runtime floor"),
  ],
)
def test_native_archive_rejects_invalid_runtime_floor_schema(tmp_path, runtime_floor_value, message):
  archives = write_release_archives(tmp_path)
  filename = "linux_amd64.zip"
  members = native_members(filename)
  mutated = []
  for name, content in members:
    if name == "build_info.json":
      build_info = json.loads(content)
      build_info["runtime_floor"] = runtime_floor_value
      content = json.dumps(build_info).encode()
    mutated.append((name, content))
  write_zip(tmp_path / filename, mutated)

  with pytest.raises(RuntimeError, match=message):
    validate_release_archives(archives, VERSION, check_documented_runtime=False)


@pytest.mark.parametrize(
  ("contents", "message"),
  [
    ("no matrix here\n", "README is missing the native runtime matrix"),
    (
      "<!-- native-runtime-matrix -->\n| header | row |\n|---|---|\n| only | two |\n",
      "Invalid native runtime matrix row",
    ),
    (
      "<!-- native-runtime-matrix -->\n| Artifact | Supported | Measured |\n|---|---|---|\n"
      "| `linux_amd64` | `glibc 2.28` | `glibc=2.26` |\n",
      "Invalid native runtime matrix cell",
    ),
    (
      "<!-- native-runtime-matrix -->\n| Artifact | Supported | Measured |\n|---|---|---|\n"
      "| `linux_amd64` | `glibc=2.28` | `glibc=2.26` |\n",
      "Native runtime matrix inventory mismatch",
    ),
  ],
)
def test_runtime_matrix_rejects_invalid_documentation(tmp_path, contents, message):
  readme = tmp_path / "README.md"
  readme.write_text(contents, encoding="utf-8")

  with pytest.raises(RuntimeError, match=message):
    _runtime_matrix(readme)


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


def test_self_validation_accepts_archive_runtime_schema_independent_of_checkout(tmp_path):
  archives = write_release_archives(tmp_path)
  filename = "windows_x86_64.zip"
  members = native_members(filename)
  mutated = []
  for name, content in members:
    if name == "build_info.json":
      build_info = json.loads(content)
      build_info["runtime_floor"] = {
        "supported": {"future_windows_floor": "1.0"},
        "measured": {"future_windows_floor": "0.9"},
      }
      content = json.dumps(build_info).encode()
    mutated.append((name, content))
  write_zip(tmp_path / filename, mutated)

  validate_release_archives(archives, VERSION, check_documented_runtime=False)
  with pytest.raises(RuntimeError, match="Runtime floor mismatch"):
    validate_release_archives(archives, VERSION)


def test_self_validation_compares_shared_nonversion_properties(tmp_path):
  archives = write_release_archives(tmp_path)
  filename = "windows_x86_64.zip"
  members = native_members(filename)

  def write_runtime_property(measured):
    mutated = []
    for name, content in members:
      if name == "build_info.json":
        build_info = json.loads(content)
        build_info["runtime_floor"] = {
          "supported": {"msvc_runtime": "static"},
          "measured": {"msvc_runtime": measured},
        }
        content = json.dumps(build_info).encode()
      mutated.append((name, content))
    write_zip(tmp_path / filename, mutated)

  write_runtime_property("static")
  validate_release_archives(archives, VERSION, check_documented_runtime=False)

  write_runtime_property("dynamic")
  with pytest.raises(RuntimeError, match="Measured msvc_runtime property dynamic does not match supported static"):
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
