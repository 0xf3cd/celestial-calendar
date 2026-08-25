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
import io
import json
import re
import shutil
import tarfile
import tempfile
import zipfile

from collections import Counter
from enum import Enum
from pathlib import Path, PurePosixPath
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
SOURCE_SPECS: Final[tuple[tuple[str, str, frozenset[str]], ...]] = (
  ("native_run_id", "Build and Test on Multiple Platforms", frozenset(NATIVE_ARCHIVES.values())),
  ("wasm_run_id", "WASM Build and Golden Check", frozenset({Path(WASM_ARCHIVE).stem})),
  ("python_run_id", "Python Wheels", frozenset(PYTHON_ARTIFACTS)),
)
SOURCE_WORKFLOWS: Final[dict[str, frozenset[str]]] = {
  workflow: artifacts for _run_field, workflow, artifacts in SOURCE_SPECS
}
SOURCE_ARTIFACTS: Final[frozenset[str]] = frozenset(
  artifact for _run_field, _workflow, artifacts in SOURCE_SPECS for artifact in artifacts
)
README: Final[Path] = Path(__file__).resolve().parents[1] / "README.md"
ROOT_LICENSE: Final[Path] = Path(__file__).resolve().parents[1] / "LICENSE"
RELEASE_DOCUMENTS: Final[tuple[Path, ...]] = (
  Path(__file__).resolve().parents[1] / "docs" / "RELEASE_NOTES.md",
  Path(__file__).resolve().parents[1] / "docs" / "CHANGELOG.md",
)
RUNTIME_MATRIX_MARKER: Final[str] = "<!-- native-runtime-matrix -->"


class LicenseValidation(Enum):
  LEGACY = "legacy"
  MEMBERS = "members"
  REPOSITORY = "repository"


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


def _require_license(
  archive: zipfile.ZipFile,
  member: str,
  archive_name: str,
  expected_license: bytes | None,
) -> None:
  license_members = [name for name in archive.namelist() if PurePosixPath(name).name == "LICENSE"]
  if license_members != [member]:
    raise RuntimeError(f"Invalid LICENSE members in {archive_name}: {license_members}")
  if expected_license is not None and archive.read(member) != expected_license:
    raise RuntimeError(f"LICENSE in {archive_name} does not match the repository LICENSE")


def _require_npm_license(tarball: bytes, tarball_name: str, expected_license: bytes | None) -> None:
  try:
    with tarfile.open(fileobj=io.BytesIO(tarball), mode="r:gz") as archive:
      members = [
        member for member in archive.getmembers() if member.isfile() and PurePosixPath(member.name).name == "LICENSE"
      ]
      names = [member.name for member in members]
      if names != ["package/LICENSE"]:
        raise RuntimeError(f"Invalid LICENSE members in {tarball_name}: {names}")
      source = archive.extractfile(members[0])
      if source is None or (expected_license is not None and source.read() != expected_license):
        raise RuntimeError(f"LICENSE in {tarball_name} does not match the repository LICENSE")
  except tarfile.TarError as error:
    raise RuntimeError(f"Invalid npm tarball {tarball_name}: {error}") from error


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
    raise RuntimeError(f"Unexpected wheel filename for release {version}: {wheel_name}")
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
      valid = tags == [f"macosx_14_0_{architecture}"]
    else:
      valid = tags == [f"win_{architecture}"]
    if valid:
      return artifact_name
  raise RuntimeError(f"Unexpected wheel platform in filename: {wheel_name}")


def validate_wheel_platform(wheel_name: str, artifact_name: str, version: str) -> None:
  """Match one artifact name to its one permitted wheel platform tag."""
  try:
    actual_artifact = _wheel_artifact(wheel_name, version)
  except RuntimeError as error:
    raise RuntimeError(f"Wheel {wheel_name} does not match artifact {artifact_name}") from error
  if actual_artifact != artifact_name:
    raise RuntimeError(f"Wheel {wheel_name} does not match artifact {artifact_name}")


def validate_wheel_sidecars(
  downloaded: Iterable[Path],
  version: str,
  require_complete: bool = False,
  license_validation: LicenseValidation = LicenseValidation.REPOSITORY,
) -> None:
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
  expected_license = ROOT_LICENSE.read_bytes() if license_validation is LicenseValidation.REPOSITORY else None
  for wheel_name in wheels:
    artifact_name = _wheel_artifact(wheel_name, version)
    if artifact_name in artifacts:
      raise RuntimeError(f"Duplicate wheel platform: {artifact_name}")
    artifacts[artifact_name] = wheel_name
    if license_validation is not LicenseValidation.LEGACY:
      try:
        with zipfile.ZipFile(payloads[wheel_name]) as archive:
          _require_license(
            archive,
            f"celestial_calendar-{version}.dist-info/licenses/LICENSE",
            wheel_name,
            expected_license,
          )
      except zipfile.BadZipFile as error:
        raise RuntimeError(f"Invalid wheel archive {wheel_name}: {error}") from error
    digest = hashlib.sha256(payloads[wheel_name].read_bytes()).hexdigest()
    expected = f"{digest}  {wheel_name}\n".encode()
    if payloads[f"{wheel_name}.sha256"].read_bytes() != expected:
      raise RuntimeError(f"SHA-256 sidecar mismatch for {wheel_name}")

  if require_complete and set(artifacts) != set(PYTHON_ARTIFACTS):
    raise RuntimeError(
      f"Wheel platform inventory mismatch: missing={sorted(set(PYTHON_ARTIFACTS) - set(artifacts))}"
    )


def npm_archive_payload(
  archive_path: Path,
  version: str,
  license_validation: LicenseValidation = LicenseValidation.REPOSITORY,
) -> dict[str, bytes]:
  """Validate one WASM archive and return its registry publication payload."""
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
      if license_validation is not LicenseValidation.LEGACY:
        expected.add("LICENSE")
      _require_members(archive, expected, archive_path.name)
      expected_license = ROOT_LICENSE.read_bytes() if license_validation is LicenseValidation.REPOSITORY else None
      if license_validation is not LicenseValidation.LEGACY:
        _require_license(archive, "LICENSE", archive_path.name, expected_license)

      tarball = archive.read(tarball_name)
      digest = hashlib.sha256(tarball).hexdigest()
      expected_sidecar = f"{digest}  {tarball_name}\n".encode()
      if archive.read("npm-pack.sha256") != expected_sidecar:
        raise RuntimeError(f"SHA-256 sidecar mismatch in {archive_path.name}")
      if license_validation is not LicenseValidation.LEGACY:
        _require_npm_license(tarball, tarball_name, expected_license)
      return {
        tarball_name: tarball,
        "npm-pack.json": archive.read("npm-pack.json"),
        "npm-pack.sha256": archive.read("npm-pack.sha256"),
      }
  except zipfile.BadZipFile as error:
    raise RuntimeError(f"Invalid ZIP archive {archive_path.name}: {error}") from error


def validate_wasm_archive(
  archive_path: Path,
  version: str,
  license_validation: LicenseValidation = LicenseValidation.REPOSITORY,
) -> None:
  """Validate the exact WASM/npm artifact contract without changing the ZIP."""
  npm_archive_payload(archive_path, version, license_validation)


def _native_layout(
  artifact_name: str,
  version: str,
  license_validation: LicenseValidation = LicenseValidation.REPOSITORY,
) -> tuple[set[str], dict[str, str]]:
  match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version)
  if match is None:
    raise RuntimeError(f"Native archive validation requires a major.minor.patch version, got {version!r}")
  major = match.group(1)
  soversion = f"{major}.{match.group(2)}" if major == "0" else major

  fixed = {"build_info.json", "cpu_info.json", "include/celestial.h"}
  if license_validation is not LicenseValidation.LEGACY:
    fixed.add("LICENSE")
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
  license_validation: LicenseValidation = LicenseValidation.REPOSITORY,
) -> None:
  """Validate one native artifact's members, runtime floor, and producer-recorded hashes."""
  expected, runtime_members = _native_layout(artifact_name, version, license_validation)
  try:
    with zipfile.ZipFile(archive_path) as archive:
      _require_members(archive, expected, archive_path.name)
      if license_validation is not LicenseValidation.LEGACY:
        expected_license = ROOT_LICENSE.read_bytes() if license_validation is LicenseValidation.REPOSITORY else None
        _require_license(archive, "LICENSE", archive_path.name, expected_license)
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
  license_validation: LicenseValidation = LicenseValidation.REPOSITORY,
) -> None:
  """Validate release archives and wheels under the selected LICENSE contract."""
  downloaded = list(downloaded)
  validate_wheel_sidecars(
    downloaded,
    version,
    require_complete=require_wheels,
    license_validation=license_validation,
  )
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
  validate_wasm_archive(
    archives[WASM_ARCHIVE],
    version,
    license_validation,
  )
  for filename, artifact_name in NATIVE_ARCHIVES.items():
    expected_runtime = runtime_matrix[artifact_name] if runtime_matrix is not None else None
    validate_native_archive(
      archives[filename],
      artifact_name,
      version,
      expected_runtime,
      license_validation,
    )


# JSON booleans are Python integers; manifest schemas, IDs, and sizes require exact integer values.
def _validate_release_sources(sources: object, commit: str) -> list[dict]:
  """Validate the three producer identities embedded in a release manifest."""
  if not isinstance(sources, list) or len(sources) != 3:
    raise RuntimeError("Release source manifest must contain exactly three workflows")

  workflow_ids = set()
  workflow_names = set()
  run_ids = set()
  artifact_ids = set()
  artifact_names = set()
  for source in sources:
    if not isinstance(source, dict):
      raise RuntimeError("Invalid release source entry")
    workflow = source.get("workflow")
    run = source.get("run")
    artifacts = source.get("artifacts")
    if not isinstance(workflow, dict) or not isinstance(run, dict) or not isinstance(artifacts, list):
      raise RuntimeError("Invalid release source workflow or run identity")
    workflow_id = workflow.get("id")
    workflow_name = workflow.get("name")
    run_id = run.get("id")
    valid_workflow = type(workflow_id) is int and workflow_id > 0 and isinstance(workflow_name, str)
    valid_run = type(run_id) is int and run_id > 0 and run.get("head_sha") == commit
    if not valid_workflow or not valid_run:
      raise RuntimeError("Invalid release source workflow or run identity")
    if workflow_id in workflow_ids or workflow_name in workflow_names:
      raise RuntimeError(f"Duplicate release source workflow: {workflow_name}")
    if run_id in run_ids:
      raise RuntimeError(f"Duplicate release source run ID: {run_id}")
    workflow_ids.add(workflow_id)
    workflow_names.add(workflow_name)
    run_ids.add(run_id)
    source_artifact_names = set()
    for artifact in artifacts:
      if not isinstance(artifact, dict):
        raise RuntimeError("Invalid release source artifact identity")
      artifact_id = artifact.get("id")
      artifact_name = artifact.get("name")
      artifact_size = artifact.get("size")
      artifact_digest = artifact.get("digest")
      valid_artifact_id = type(artifact_id) is int and artifact_id > 0
      valid_artifact_size = type(artifact_size) is int and artifact_size > 0
      valid_artifact_digest = (
        isinstance(artifact_digest, str)
        and re.fullmatch(r"sha256:[0-9a-f]{64}", artifact_digest) is not None
      )
      if (
        not valid_artifact_id
        or not isinstance(artifact_name, str)
        or not valid_artifact_size
        or not valid_artifact_digest
      ):
        raise RuntimeError("Invalid release source artifact identity")
      if artifact_id in artifact_ids:
        raise RuntimeError(f"Duplicate release source artifact ID: {artifact_id}")
      if artifact_name in artifact_names:
        raise RuntimeError(f"Duplicate release source artifact name: {artifact_name}")
      artifact_ids.add(artifact_id)
      source_artifact_names.add(artifact_name)
      artifact_names.add(artifact_name)
    expected_artifacts = SOURCE_WORKFLOWS.get(workflow_name)
    if expected_artifacts is None or source_artifact_names != set(expected_artifacts):
      raise RuntimeError(f"Release source workflow artifact mismatch: {workflow_name}")
  if workflow_names != set(SOURCE_WORKFLOWS):
    raise RuntimeError("Release source workflow inventory mismatch")
  if artifact_names != set(SOURCE_ARTIFACTS):
    raise RuntimeError(
      f"Release source artifact inventory mismatch: "
      f"missing={sorted(set(SOURCE_ARTIFACTS) - artifact_names)}, "
      f"extra={sorted(artifact_names - set(SOURCE_ARTIFACTS))}"
    )
  return sources


def _release_sources(path: Path, commit: str) -> list[dict]:
  try:
    payload = json.loads(path.read_text(encoding="utf-8"))
  except (json.JSONDecodeError, UnicodeDecodeError) as error:
    raise RuntimeError(f"Invalid release source manifest: {error}") from error
  if not isinstance(payload, dict):
    raise RuntimeError("Release source manifest identity mismatch")
  schema = payload.get("schema")
  if type(schema) is not int or schema != 1 or payload.get("commit") != commit:
    raise RuntimeError("Release source manifest identity mismatch")
  return _validate_release_sources(payload.get("sources"), commit)


def _copy_bytes(destination: Path, content: bytes) -> None:
  if destination.exists():
    raise FileExistsError(f"Refusing to overwrite release candidate file: {destination}")
  destination.parent.mkdir(parents=True, exist_ok=True)
  destination.write_bytes(content)


def validate_release_candidate(candidate: Path, tag_name: str, commit: str) -> dict:
  """Reconcile a frozen candidate with its manifest and original release contracts."""
  version = tag_name.removeprefix("v")
  if tag_name != f"v{version}" or re.fullmatch(r"\d+\.\d+\.\d+", version) is None:
    raise RuntimeError(f"Invalid release tag: {tag_name}")
  expected_directories = {"github", "pypi", "npm", "evidence"}
  if not candidate.is_dir() or {path.name for path in candidate.iterdir()} != expected_directories:
    raise RuntimeError("Release candidate directory inventory mismatch")
  if any(not path.is_dir() or path.is_symlink() for path in candidate.iterdir()):
    raise RuntimeError("Release candidate entries must be regular directories")

  manifest_path = candidate / "evidence" / "manifest.json"
  if not manifest_path.is_file() or manifest_path.is_symlink():
    raise RuntimeError("Release candidate manifest must be a regular file")
  try:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
  except (json.JSONDecodeError, UnicodeDecodeError) as error:
    raise RuntimeError(f"Invalid release candidate manifest: {error}") from error
  if not isinstance(manifest, dict):
    raise RuntimeError("Release candidate identity mismatch")
  schema = manifest.get("schema")
  if (
    type(schema) is not int
    or schema != 1
    or manifest.get("tag") != tag_name
    or manifest.get("version") != version
    or manifest.get("commit") != commit
  ):
    raise RuntimeError("Release candidate identity mismatch")
  _validate_release_sources(manifest.get("sources"), commit)

  files = manifest.get("files")
  if not isinstance(files, dict) or "evidence/manifest.json" in files:
    raise RuntimeError("Invalid release candidate file manifest")
  top_level = set(candidate.iterdir())
  all_entries = list(candidate.rglob("*"))
  actual_files = {
    path.relative_to(candidate).as_posix(): path
    for path in all_entries
    if path.is_file() and path != manifest_path
  }
  non_files = [
    path
    for path in all_entries
    if path not in top_level and path != manifest_path and (path.is_dir() or path.is_symlink())
  ]
  if non_files or set(files) != set(actual_files):
    raise RuntimeError("Release candidate file inventory mismatch")
  for relative, identity in files.items():
    if not isinstance(identity, dict):
      raise RuntimeError(f"Invalid release candidate file identity: {relative}")
    size = identity.get("size")
    sha256 = identity.get("sha256")
    valid_size = type(size) is int and size >= 0
    valid_digest = isinstance(sha256, str) and re.fullmatch(r"[0-9a-f]{64}", sha256) is not None
    if not valid_size or not valid_digest:
      raise RuntimeError(f"Invalid release candidate file identity: {relative}")
    content = actual_files[relative].read_bytes()
    if len(content) != size or hashlib.sha256(content).hexdigest() != sha256:
      raise RuntimeError(f"Release candidate file mismatch: {relative}")

  github = candidate / "github"
  pypi = candidate / "pypi"
  npm = candidate / "npm"
  evidence = candidate / "evidence"
  github_files = list(github.iterdir())
  validate_release_archives(
    github_files,
    version,
    require_wheels=True,
    license_validation=LicenseValidation.REPOSITORY,
  )
  wheels = sorted(pypi.iterdir())
  if len(wheels) != len(PYTHON_ARTIFACTS) or any(not path.name.endswith(".whl") for path in wheels):
    raise RuntimeError("PyPI candidate must contain exactly four wheels")
  sidecars = [evidence / f"{wheel.name}.sha256" for wheel in wheels]
  expected_github = {
    *RELEASE_ARCHIVES,
    *(wheel.name for wheel in wheels),
    *(path.name for path in sidecars),
    "CHANGELOG.md",
  }
  if {path.name for path in github_files} != expected_github:
    raise RuntimeError("GitHub candidate asset inventory mismatch")
  expected_evidence = {
    *(path.name for path in sidecars),
    "npm-pack.json",
    "npm-pack.sha256",
    "RELEASE_NOTES.md",
    "manifest.json",
  }
  if {path.name for path in evidence.iterdir()} != expected_evidence:
    raise RuntimeError("Release candidate evidence inventory mismatch")
  validate_wheel_sidecars(
    [*wheels, *sidecars],
    version,
    require_complete=True,
    license_validation=LicenseValidation.REPOSITORY,
  )
  for wheel, sidecar in zip(wheels, sidecars, strict=True):
    if wheel.read_bytes() != (github / wheel.name).read_bytes():
      raise RuntimeError(f"PyPI/GitHub wheel mismatch: {wheel.name}")
    if sidecar.read_bytes() != (github / sidecar.name).read_bytes():
      raise RuntimeError(f"Evidence/GitHub wheel sidecar mismatch: {sidecar.name}")

  npm_files = list(npm.iterdir())
  if len(npm_files) != 1 or not npm_files[0].name.endswith(".tgz"):
    raise RuntimeError("npm candidate must contain exactly one tarball")
  npm_payload = npm_archive_payload(
    github / WASM_ARCHIVE,
    version,
    license_validation=LicenseValidation.REPOSITORY,
  )
  tarball = npm_files[0]
  if npm_payload.get(tarball.name) != tarball.read_bytes():
    raise RuntimeError("npm candidate tarball does not match the WASM archive")
  for sidecar_name in ("npm-pack.json", "npm-pack.sha256"):
    if npm_payload[sidecar_name] != (evidence / sidecar_name).read_bytes():
      raise RuntimeError(f"npm evidence mismatch: {sidecar_name}")
  validate_release_document_versions(tag_name, (evidence / "RELEASE_NOTES.md", github / "CHANGELOG.md"))
  return manifest


def stage_release_candidate(
  release_assets: Path,
  source_manifest: Path,
  save_to: Path,
  tag_name: str,
  commit: str,
  release_notes: Path = RELEASE_DOCUMENTS[0],
) -> Path:
  """Partition one validated release inventory into GitHub, npm, PyPI, and evidence."""
  version = tag_name.removeprefix("v")
  if tag_name != f"v{version}" or re.fullmatch(r"\d+\.\d+\.\d+", version) is None:
    raise RuntimeError(f"Invalid release tag: {tag_name}")
  if save_to.exists():
    raise FileExistsError(f"Refusing to overwrite release candidate: {save_to}")
  if not release_assets.is_dir():
    raise RuntimeError(f"Release assets directory does not exist: {release_assets}")

  assets = list(release_assets.iterdir())
  non_files = sorted(path.name for path in assets if not path.is_file() or path.is_symlink())
  if non_files:
    raise RuntimeError(f"GitHub Release assets must be regular files: {non_files}")
  validate_release_archives(
    assets,
    version,
    require_wheels=True,
    license_validation=LicenseValidation.REPOSITORY,
  )
  wheels = sorted(path for path in assets if path.name.endswith(".whl"))
  sidecars = sorted(path for path in assets if path.name.endswith(".whl.sha256"))
  expected_names = {
    *RELEASE_ARCHIVES,
    *(path.name for path in wheels),
    *(path.name for path in sidecars),
    "CHANGELOG.md",
  }
  actual_names = {path.name for path in assets}
  if len(actual_names) != len(assets) or actual_names != expected_names:
    raise RuntimeError(
      f"GitHub Release asset inventory mismatch: missing={sorted(expected_names - actual_names)}, "
      f"extra={sorted(actual_names - expected_names)}"
    )
  changelog = release_assets / "CHANGELOG.md"
  validate_release_document_versions(tag_name, (release_notes, changelog))
  sources = _release_sources(source_manifest, commit)
  npm_payload = npm_archive_payload(
    release_assets / WASM_ARCHIVE,
    version,
    license_validation=LicenseValidation.REPOSITORY,
  )
  tarball_name = next(name for name in npm_payload if name.endswith(".tgz"))

  save_to.parent.mkdir(parents=True, exist_ok=True)
  staging = Path(tempfile.mkdtemp(prefix=f".{save_to.name}-", dir=save_to.parent))
  try:
    for asset in assets:
      _copy_bytes(staging / "github" / asset.name, asset.read_bytes())
    for wheel in wheels:
      _copy_bytes(staging / "pypi" / wheel.name, wheel.read_bytes())
    _copy_bytes(staging / "npm" / tarball_name, npm_payload[tarball_name])
    for sidecar in sidecars:
      _copy_bytes(staging / "evidence" / sidecar.name, sidecar.read_bytes())
    _copy_bytes(staging / "evidence" / "npm-pack.json", npm_payload["npm-pack.json"])
    _copy_bytes(staging / "evidence" / "npm-pack.sha256", npm_payload["npm-pack.sha256"])
    _copy_bytes(staging / "evidence" / "RELEASE_NOTES.md", release_notes.read_bytes())

    files = {}
    for path in sorted(path for path in staging.rglob("*") if path.is_file()):
      relative = path.relative_to(staging).as_posix()
      content = path.read_bytes()
      files[relative] = {"size": len(content), "sha256": hashlib.sha256(content).hexdigest()}
    manifest = {
      "schema": 1,
      "tag": tag_name,
      "version": version,
      "commit": commit,
      "sources": sources,
      "files": files,
    }
    _copy_bytes(
      staging / "evidence" / "manifest.json",
      (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(),
    )
    validate_release_candidate(staging, tag_name, commit)
    staging.replace(save_to)
  except Exception:
    shutil.rmtree(staging, ignore_errors=True)
    raise
  return save_to / "evidence" / "manifest.json"
