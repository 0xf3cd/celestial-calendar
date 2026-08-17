#!/usr/bin/env python3
#
# Validate release archive payloads without extracting or modifying them.
#
#########################################################################################
#
# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import hashlib
import json
import re
import zipfile

from collections import Counter
from pathlib import Path
from typing import Final, Iterable

from toolbox.build_npm import PACKAGE_NAME


WASM_ARCHIVE: Final[str] = "celestial-wasm.zip"
NATIVE_ARCHIVES: Final[dict[str, str]] = {
  "linux_amd64.zip": "linux_amd64",
  "linux_arm64.zip": "linux_arm64",
  "macos_arm64.zip": "macos_arm64",
  "windows_x86_64.zip": "windows_x86_64",
}
RELEASE_ARCHIVES: Final[frozenset[str]] = frozenset({WASM_ARCHIVE, *NATIVE_ARCHIVES})


def _require_members(archive: zipfile.ZipFile, expected: set[str], archive_name: str) -> None:
  names = archive.namelist()
  duplicates = sorted(name for name, count in Counter(names).items() if count > 1)
  actual = set(names)
  missing = sorted(expected - actual)
  extra = sorted(actual - expected)
  if duplicates or missing or extra:
    raise RuntimeError(
      f"Invalid members in {archive_name}: duplicates={duplicates}, missing={missing}, extra={extra}"
    )


def _read_json(archive: zipfile.ZipFile, member: str, archive_name: str):
  try:
    return json.loads(archive.read(member))
  except (json.JSONDecodeError, UnicodeDecodeError) as error:
    raise RuntimeError(f"Invalid {member} in {archive_name}: {error}") from error


def validate_wasm_archive(archive_path: Path, version: str) -> None:
  """Validate the exact WASM/npm artifact contract without changing the ZIP."""
  try:
    with zipfile.ZipFile(archive_path) as archive:
      names = archive.namelist()
      duplicates = sorted(name for name, count in Counter(names).items() if count > 1)
      if duplicates:
        raise RuntimeError(f"Duplicate archive member in {archive_path.name}: {duplicates}")
      required_metadata = {"npm-pack.json", "npm-pack.sha256"}
      missing_metadata = sorted(required_metadata - set(names))
      if missing_metadata:
        raise RuntimeError(f"Missing npm metadata in {archive_path.name}: {missing_metadata}")

      pack = _read_json(archive, "npm-pack.json", archive_path.name)
      if not isinstance(pack, list) or len(pack) != 1 or not isinstance(pack[0], dict):
        raise RuntimeError(f"npm-pack.json in {archive_path.name} must describe exactly one package")
      package = pack[0]
      tarball_name = package.get("filename")
      if (
        package.get("name") != PACKAGE_NAME
        or package.get("version") != version
        or not isinstance(tarball_name, str)
        or Path(tarball_name).name != tarball_name
        or "\\" in tarball_name
        or not tarball_name.endswith(".tgz")
      ):
        raise RuntimeError(f"Invalid npm package identity in {archive_path.name}")

      expected = {
        "celestial-jieqi.mjs",
        "celestial-jieqi.wasm",
        tarball_name,
        "npm-pack.json",
        "npm-pack.sha256",
      }
      _require_members(archive, expected, archive_path.name)

      tarball = archive.read(tarball_name)
      digest = hashlib.sha256(tarball).hexdigest()
      expected_sidecar = f"{digest}  {tarball_name}\n".encode()
      if archive.read("npm-pack.sha256") != expected_sidecar:
        raise RuntimeError(f"SHA-256 sidecar mismatch in {archive_path.name}")
  except zipfile.BadZipFile as error:
    raise RuntimeError(f"Invalid ZIP archive {archive_path.name}: {error}") from error


def _native_layout(artifact_name: str, version: str) -> tuple[set[str], dict[str, str]]:
  match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version)
  if match is None:
    raise RuntimeError(f"Native archive validation requires a major.minor.patch version, got {version!r}")
  major = match.group(1)
  soversion = f"{major}.{match.group(2)}" if major == "0" else major

  fixed = {"build_info.json", "cpu_info.json", "include/celestial.h"}
  if artifact_name in {"linux_amd64", "linux_arm64"}:
    runtime_members = {
      "lib/libcelestial_calendar.so": "libcelestial_calendar.so",
      f"lib/libcelestial_calendar.so.{soversion}": f"libcelestial_calendar.so.{soversion}",
      f"lib/libcelestial_calendar.so.{version}": f"libcelestial_calendar.so.{version}",
    }
  elif artifact_name == "macos_arm64":
    runtime_members = {
      "lib/libcelestial_calendar.dylib": "libcelestial_calendar.dylib",
      f"lib/libcelestial_calendar.{soversion}.dylib": f"libcelestial_calendar.{soversion}.dylib",
      f"lib/libcelestial_calendar.{version}.dylib": f"libcelestial_calendar.{version}.dylib",
    }
  elif artifact_name == "windows_x86_64":
    runtime_members = {"bin/celestial_calendar.dll": "celestial_calendar.dll"}
    fixed.add("lib/celestial_calendar.lib")
  else:
    raise RuntimeError(f"Unknown native artifact: {artifact_name}")
  return fixed | set(runtime_members), runtime_members


def validate_native_archive(archive_path: Path, artifact_name: str, version: str) -> None:
  """Validate one native artifact's members and producer-recorded runtime hashes."""
  expected, runtime_members = _native_layout(artifact_name, version)
  try:
    with zipfile.ZipFile(archive_path) as archive:
      _require_members(archive, expected, archive_path.name)
      build_info = _read_json(archive, "build_info.json", archive_path.name)
      if not isinstance(build_info, dict) or build_info.get("build_version") != version:
        raise RuntimeError(f"Build version mismatch in {archive_path.name}")
      hashes = build_info.get("sha256")
      if not isinstance(hashes, dict) or set(hashes) != set(runtime_members.values()):
        raise RuntimeError(f"Runtime hash inventory mismatch in {archive_path.name}")

      for member, basename in runtime_members.items():
        recorded = hashes[basename]
        actual = hashlib.sha256(archive.read(member)).hexdigest()
        if not isinstance(recorded, str) or recorded != actual:
          raise RuntimeError(f"Runtime library hash mismatch for {member} in {archive_path.name}")
  except zipfile.BadZipFile as error:
    raise RuntimeError(f"Invalid ZIP archive {archive_path.name}: {error}") from error


def validate_release_archives(downloaded: Iterable[Path], version: str) -> None:
  """Require and validate the five v0.6+ product archives among downloaded assets."""
  archives: dict[str, Path] = {}
  for path in downloaded:
    if path.name not in RELEASE_ARCHIVES:
      continue
    if path.name in archives:
      raise RuntimeError(f"Duplicate downloaded release archive: {path.name}")
    if not path.is_file():
      raise RuntimeError(f"Downloaded release archive is not a file: {path}")
    archives[path.name] = path

  missing = sorted(RELEASE_ARCHIVES - set(archives))
  if missing:
    raise RuntimeError(f"Missing downloaded release archives: {missing}")

  validate_wasm_archive(archives[WASM_ARCHIVE], version)
  for filename, artifact_name in NATIVE_ARCHIVES.items():
    validate_native_archive(archives[filename], artifact_name, version)
