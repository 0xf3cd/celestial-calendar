#!/usr/bin/env python3
#
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
import re
import zipfile

from collections import Counter
from pathlib import Path
from typing import Final, Iterable

from toolbox.build_npm import PACKAGE_NAME
from toolbox.runtime_floor import validate_runtime_floor


WASM_ARCHIVE: Final[str] = "celestial-wasm.zip"
NATIVE_ARCHIVES: Final[dict[str, str]] = {
  "linux_amd64.zip": "linux_amd64",
  "linux_arm64.zip": "linux_arm64",
  "macos_arm64.zip": "macos_arm64",
  "windows_x86_64.zip": "windows_x86_64",
}
RELEASE_ARCHIVES: Final[frozenset[str]] = frozenset({WASM_ARCHIVE, *NATIVE_ARCHIVES})
PYTHON_ARTIFACTS: Final[dict[str, tuple[str, str]]] = {
  "celestial-python-manylinux-x86_64": ("manylinux", "x86_64"),
  "celestial-python-manylinux-aarch64": ("manylinux", "aarch64"),
  "celestial-python-macos-arm64": ("macos", "arm64"),
  "celestial-python-windows-amd64": ("windows", "amd64"),
}
README: Final[Path] = Path(__file__).resolve().parents[1] / "README.md"
RELEASE_DOCUMENTS: Final[tuple[Path, ...]] = (
  Path(__file__).resolve().parents[1] / "docs" / "RELEASE_NOTES.md",
  Path(__file__).resolve().parents[1] / "docs" / "CHANGELOG.md",
)
RUNTIME_MATRIX_MARKER: Final[str] = "<!-- native-runtime-matrix -->"


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


def _runtime_values(cell: str) -> dict[str, str]:
  values = {}
  for assignment in cell.strip().strip("`").split(","):
    parts = assignment.strip().split("=", maxsplit=1)
    if len(parts) != 2 or not all(parts):
      raise RuntimeError(f"Invalid native runtime matrix cell: {cell!r}")
    key, value = parts
    values[key] = value
  return values


def _runtime_matrix(readme: Path = README) -> dict[str, dict[str, dict[str, str]]]:
  """Read the native support and measured-requirement matrix from README."""
  lines = readme.read_text(encoding="utf-8").splitlines()
  try:
    start = lines.index(RUNTIME_MATRIX_MARKER) + 3
  except ValueError as error:
    raise RuntimeError("README is missing the native runtime matrix") from error
  matrix = {}
  for line in lines[start:]:
    if not line.startswith("|"):
      break
    cells = [cell.strip() for cell in line.strip("|").split("|")]
    if len(cells) != 3:
      raise RuntimeError(f"Invalid native runtime matrix row: {line}")
    artifact = cells[0].strip("`")
    matrix[artifact] = {
      "supported": _runtime_values(cells[1]),
      "measured": _runtime_values(cells[2]),
    }
  expected = set(NATIVE_ARCHIVES.values())
  if set(matrix) != expected:
    raise RuntimeError(
      f"Native runtime matrix inventory mismatch: missing={sorted(expected - set(matrix))}, "
      f"extra={sorted(set(matrix) - expected)}"
    )
  return matrix


def validate_release_document_versions(tag_name: str, documents: Iterable[Path] = RELEASE_DOCUMENTS) -> None:
  """Require each release document's first version heading to match the tag."""
  expected = tag_name.removeprefix("v")
  for document in documents:
    headings = re.findall(r"^## \[v([^]]+)\](?: - .+)?$", document.read_text(encoding="utf-8"), re.MULTILINE)
    if not headings:
      raise RuntimeError(f"Cannot find a version heading in {document}")
    if headings[0] != expected:
      raise RuntimeError(
        f"Release version mismatch in {document}: expected {expected}, found {headings[0]}"
      )


def _wheel_artifact(wheel_name: str, version: str) -> str:
  match = re.fullmatch(rf"celestial_calendar-{re.escape(version)}-py3-none-(.+)\.whl", wheel_name)
  if match is None:
    raise RuntimeError(f"Unexpected wheel filename: {wheel_name}")
  tags = match.group(1).split(".")

  for artifact_name, (family, architecture) in PYTHON_ARTIFACTS.items():
    if family == "manylinux":
      expected = f"manylinux_2_28_{architecture}"
      tag_matches = [re.fullmatch(rf"manylinux_(\d+)_(\d+)_{architecture}", tag) for tag in tags]
      valid = expected in tags and all(tag_match is not None for tag_match in tag_matches)
      valid = valid and all(
        (int(tag_match.group(1)), int(tag_match.group(2))) <= (2, 28)
        for tag_match in tag_matches
        if tag_match is not None
      )
    elif family == "macos":
      valid = tags == ["macosx_14_0_arm64"]
    else:
      valid = tags == ["win_amd64"]
    if valid:
      return artifact_name
  raise RuntimeError(f"Unexpected wheel platform in filename: {wheel_name}")


def validate_wheel_platform(wheel_name: str, artifact_name: str, version: str) -> None:
  """Match one artifact name to its one permitted wheel platform tag."""
  if _wheel_artifact(wheel_name, version) != artifact_name:
    raise RuntimeError(f"Wheel {wheel_name} does not match artifact {artifact_name}")


def validate_wheel_sidecars(downloaded: Iterable[Path], version: str, require_complete: bool = False) -> None:
  """Validate every downloaded wheel against its adjacent SHA-256 sidecar."""
  payloads: dict[str, Path] = {}
  for path in downloaded:
    if not (path.name.endswith(".whl") or path.name.endswith(".whl.sha256")):
      continue
    if path.name in payloads:
      raise RuntimeError(f"Duplicate downloaded Python release asset: {path.name}")
    if not path.is_file():
      raise RuntimeError(f"Downloaded Python release asset is not a file: {path}")
    payloads[path.name] = path

  wheels = {name for name in payloads if name.endswith(".whl")}
  sidecars = {name for name in payloads if name.endswith(".whl.sha256")}
  expected_sidecars = {f"{name}.sha256" for name in wheels}
  if sidecars != expected_sidecars:
    raise RuntimeError(
      f"Wheel sidecar inventory mismatch: missing={sorted(expected_sidecars - sidecars)}, "
      f"extra={sorted(sidecars - expected_sidecars)}"
    )

  artifacts = {}
  for wheel_name in wheels:
    artifact_name = _wheel_artifact(wheel_name, version)
    if artifact_name in artifacts:
      raise RuntimeError(f"Duplicate wheel platform: {artifact_name}")
    artifacts[artifact_name] = wheel_name
    digest = hashlib.sha256(payloads[wheel_name].read_bytes()).hexdigest()
    expected = f"{digest}  {wheel_name}\n".encode()
    if payloads[f"{wheel_name}.sha256"].read_bytes() != expected:
      raise RuntimeError(f"SHA-256 sidecar mismatch for {wheel_name}")

  if require_complete and set(artifacts) != set(PYTHON_ARTIFACTS):
    raise RuntimeError(
      f"Wheel platform inventory mismatch: missing={sorted(set(PYTHON_ARTIFACTS) - set(artifacts))}, "
      f"extra={sorted(set(artifacts) - set(PYTHON_ARTIFACTS))}"
    )


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


def validate_native_archive(
  archive_path: Path,
  artifact_name: str,
  version: str,
  expected_runtime: dict[str, dict[str, str]] | None = None,
) -> None:
  """Validate one native artifact's members, runtime floor, and producer-recorded hashes."""
  expected, runtime_members = _native_layout(artifact_name, version)
  try:
    with zipfile.ZipFile(archive_path) as archive:
      _require_members(archive, expected, archive_path.name)
      build_info = _read_json(archive, "build_info.json", archive_path.name)
      if not isinstance(build_info, dict) or build_info.get("build_version") != version:
        raise RuntimeError(f"Build version mismatch in {archive_path.name}")
      runtime_floor = build_info.get("runtime_floor")
      validate_runtime_floor(runtime_floor, archive_path.name)
      if expected_runtime is not None and runtime_floor != expected_runtime:
        raise RuntimeError(f"Runtime floor mismatch in {archive_path.name}")
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


def validate_release_archives(
  downloaded: Iterable[Path],
  version: str,
  check_documented_runtime: bool = True,
  require_wheels: bool = False,
) -> None:
  """Validate the v0.6+ product archives and any downloaded wheel sidecars."""
  downloaded = list(downloaded)
  validate_wheel_sidecars(downloaded, version, require_complete=require_wheels)
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

  runtime_matrix = _runtime_matrix() if check_documented_runtime else None
  validate_wasm_archive(archives[WASM_ARCHIVE], version)
  for filename, artifact_name in NATIVE_ARCHIVES.items():
    expected_runtime = runtime_matrix[artifact_name] if runtime_matrix is not None else None
    validate_native_archive(archives[filename], artifact_name, version, expected_runtime)
