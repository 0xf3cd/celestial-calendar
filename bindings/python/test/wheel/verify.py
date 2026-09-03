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

"""Inspect one repaired wheel's metadata, payload, architecture, exports, and dependencies."""

from __future__ import annotations

import csv
import io
import json
import os
import re
import subprocess
import sys
import tempfile
import zipfile
from email.parser import BytesParser
from pathlib import Path


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
MANIFEST = HERE.parent / "abi" / "manifest.json"
sys.path.append(str(REPO))

from toolbox.windows_toolchain_evidence import validate_windows_evidence  # noqa: E402


def run(*command: str) -> str:
  """Run an inspection tool and return its output."""
  return subprocess.run(command, check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout


def project_version() -> str:
  """Read the package version from the repository SSOT."""
  project = (REPO / "project.py").read_text(encoding="utf-8")
  match = re.search(r'BUILD_VERSION: Final\[str\] = "([^"]+)"', project)
  assert match is not None, "cannot parse project.py version"
  return match.group(1)


def verify_metadata(archive: zipfile.ZipFile, wheel: Path, version: str, platform_tags: list[str]) -> str:
  """Verify filename, core metadata, WHEEL tags, and the exact member allowlist."""
  match = re.fullmatch(rf"celestial_calendar-{re.escape(version)}-py3-none-(.+)\.whl", wheel.name)
  assert match is not None, f"unexpected wheel filename: {wheel.name}"
  assert match.group(1).split(".") == platform_tags

  dist_info = f"celestial_calendar-{version}.dist-info"
  if platform_tags == ["win_amd64"]:
    native_suffix = ".dll"
  elif platform_tags == ["macosx_14_0_arm64"]:
    native_suffix = ".dylib"
  else:
    native_suffix = ".so"
  native_member = f"celestial_calendar/_native/_celestial_calendar{native_suffix}"
  expected_members = {
    "celestial_calendar/__init__.py",
    "celestial_calendar/_binding.py",
    "celestial_calendar/_version.py",
    "celestial_calendar/py.typed",
    native_member,
    f"{dist_info}/METADATA",
    f"{dist_info}/WHEEL",
    f"{dist_info}/licenses/LICENSE",
    f"{dist_info}/licenses/THIRD_PARTY_NOTICES.txt",
    f"{dist_info}/RECORD",
  }
  members = archive.namelist()
  assert len(members) == len(set(members)), "duplicate wheel member"
  file_members = {member for member in members if not member.endswith("/")}
  assert file_members == expected_members, f"wheel allowlist mismatch: {sorted(file_members ^ expected_members)}"
  assert archive.read("celestial_calendar/py.typed") == b""

  record = csv.reader(io.StringIO(archive.read(f"{dist_info}/RECORD").decode("utf-8")))
  record_members = [row[0] for row in record if len(row) == 3]
  assert len(record_members) == len(set(record_members)), "duplicate RECORD member"
  assert set(record_members) == file_members, f"RECORD mismatch: {sorted(set(record_members) ^ file_members)}"

  metadata = BytesParser().parsebytes(archive.read(f"{dist_info}/METADATA"))
  assert metadata["Name"] == "celestial-calendar"
  assert metadata["Version"] == version
  assert metadata["Requires-Python"] == ">=3.11"
  assert metadata["License-Expression"] == "GPL-3.0-or-later"
  project_urls = metadata.get_all("Project-URL") or []
  assert "Repository, https://github.com/0xf3cd/celestial-calendar" in project_urls

  wheel_metadata = BytesParser().parsebytes(archive.read(f"{dist_info}/WHEEL"))
  assert set(wheel_metadata.get_all("Tag") or []) == {f"py3-none-{tag}" for tag in platform_tags}
  assert archive.read("celestial_calendar/_version.py").decode() == f'VERSION = "{version}"'
  assert archive.read(f"{dist_info}/licenses/LICENSE") == (REPO / "LICENSE").read_bytes()
  assert (
    archive.read(f"{dist_info}/licenses/THIRD_PARTY_NOTICES.txt") == (REPO / "THIRD_PARTY_NOTICES.txt").read_bytes()
  )
  return native_member


def strong_elf_exports(native: Path) -> set[str]:
  """Read strong text exports; weak C++ runtime artifacts are deliberately excluded."""
  exports = set()
  for line in run("nm", "-D", "--defined-only", str(native)).splitlines():
    parts = line.split()
    if len(parts) >= 3 and parts[1] == "T":
      exports.add(parts[2])
  return exports


def verify_linux(native: Path, wheel: Path, version: str, platform_tags: list[str], exports: set[str]) -> None:
  """Verify manylinux policy, ELF architecture, SONAME, dependencies, and exports."""
  assert len(platform_tags) >= 1
  assert all(tag.endswith(("_x86_64", "_aarch64")) for tag in platform_tags)
  architecture = "x86_64" if all(tag.endswith("_x86_64") for tag in platform_tags) else "aarch64"
  assert all(tag.endswith(f"_{architecture}") for tag in platform_tags)
  assert f"manylinux_2_28_{architecture}" in platform_tags
  for tag in platform_tags:
    match = re.fullmatch(r"manylinux_(\d+)_(\d+)_(x86_64|aarch64)", tag)
    assert match is not None, f"unexpected Linux tag: {tag}"
    assert (int(match.group(1)), int(match.group(2))) <= (2, 28), f"tag requires newer glibc: {tag}"

  auditwheel = run("auditwheel", "show", str(wheel))
  assert f"manylinux_2_28_{architecture}" in auditwheel
  elf_header = run("readelf", "-h", str(native))
  expected_machine = "Advanced Micro Devices X86-64" if architecture == "x86_64" else "AArch64"
  assert f"Machine:                           {expected_machine}" in elf_header
  dynamic = run("readelf", "-d", str(native))
  major, minor, _patch = version.split(".")
  soname_version = f"{major}.{minor}" if major == "0" else major
  assert f"Library soname: [libcelestial_calendar.so.{soname_version}]" in dynamic
  dependencies = set(re.findall(r"Shared library: \[([^\]]+)\]", dynamic))
  allowed = {
    "libstdc++.so.6",
    "libm.so.6",
    "libgcc_s.so.1",
    "libc.so.6",
    "ld-linux-x86-64.so.2",
    "ld-linux-aarch64.so.1",
  }
  assert dependencies <= allowed, f"unexpected ELF dependencies: {sorted(dependencies - allowed)}"
  strong_exports = strong_elf_exports(native)
  assert exports <= strong_exports
  assert all(name.startswith("_Z") for name in strong_exports - exports), "unexpected non-C++ ELF export"


def verify_macos(native: Path, exports: set[str]) -> None:
  """Verify arm64-only Mach-O, deployment floor, system dependencies, and exports."""
  assert "arm64" in run("file", str(native))
  load_commands = run("otool", "-l", str(native))
  minimum_versions = re.findall(r"\bminos\s+([0-9.]+)", load_commands)
  assert minimum_versions and set(minimum_versions) == {"14.0"}
  dependencies = [line.strip().split(" ", 1)[0] for line in run("otool", "-L", str(native)).splitlines()[1:]]
  assert dependencies
  assert all(
    path.startswith(("/usr/lib/", "/System/Library/")) or path.startswith("@rpath/libcelestial_calendar")
    for path in dependencies
  )
  symbols = set()
  for line in run("nm", "-gU", str(native)).splitlines():
    parts = line.split()
    if len(parts) >= 3 and parts[-2] == "T" and parts[-1].startswith("_"):
      symbols.add(parts[-1].removeprefix("_"))
  assert exports <= symbols
  assert all(name.startswith("_Z") for name in symbols - exports), "unexpected non-C++ Mach-O export"


def verify_windows(native: Path, exports: set[str]) -> None:
  """Verify PE AMD64, exact C exports, and positive static-runtime link evidence."""
  output = run("llvm-readobj", "--file-headers", "--coff-imports", "--coff-exports", str(native))
  assert "Machine: IMAGE_FILE_MACHINE_AMD64" in output
  evidence_dir = os.environ.get("CELESTIAL_WINDOWS_EVIDENCE_DIR")
  assert evidence_dir is not None, "Windows wheel verification requires link evidence"
  assert validate_windows_evidence(Path(evidence_dir) / "report.json", native, "wheel") == {"msvc_runtime": "static"}
  exported = set(re.findall(r"Export \{[\s\S]*?\n\s*Name: ([A-Za-z_]\w*)", output))
  assert exported == exports


def main() -> None:
  """Inspect the single wheel path supplied on the command line."""
  assert len(sys.argv) == 2, "usage: verify.py <wheel>"
  wheel = Path(sys.argv[1]).resolve()
  assert wheel.is_file(), wheel
  version = project_version()
  filename_match = re.fullmatch(rf"celestial_calendar-{re.escape(version)}-py3-none-(.+)\.whl", wheel.name)
  assert filename_match is not None, wheel.name
  platform_tags = filename_match.group(1).split(".")
  exports = {entry["name"] for entry in json.loads(MANIFEST.read_text(encoding="utf-8"))["exports"]}
  assert len(exports) == 29

  with zipfile.ZipFile(wheel) as archive, tempfile.TemporaryDirectory() as temporary:
    native_member = verify_metadata(archive, wheel, version, platform_tags)
    archive.extract(native_member, temporary)
    native = Path(temporary) / native_member
    if all("manylinux" in tag for tag in platform_tags):
      verify_linux(native, wheel, version, platform_tags, exports)
    elif platform_tags == ["macosx_14_0_arm64"]:
      verify_macos(native, exports)
    elif platform_tags == ["win_amd64"]:
      verify_windows(native, exports)
    else:
      raise AssertionError(f"unsupported wheel platform tags: {platform_tags}")

  print(f"PASS exact wheel metadata, payload, and native binary: {wheel.name}")


if __name__ == "__main__":
  main()
