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
import zipfile

from collections import Counter
from pathlib import Path
from typing import Final, Iterable

from toolbox.build_npm import PACKAGE_NAME


WASM_ARCHIVE: Final[str] = "celestial-wasm.zip"


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


def validate_wasm_release_archive(downloaded: Iterable[Path], version: str) -> None:
  """Require and validate the WASM/npm archive among downloaded release artifacts."""
  archives = [path for path in downloaded if path.name == WASM_ARCHIVE]
  if len(archives) != 1:
    raise RuntimeError(f"Expected one downloaded {WASM_ARCHIVE}, found {len(archives)}")
  if not archives[0].is_file():
    raise RuntimeError(f"Downloaded release archive is not a file: {archives[0]}")
  validate_wasm_archive(archives[0], version)
