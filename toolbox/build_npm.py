#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tarfile

from pathlib import Path
from typing import Final


PROJ_ROOT: Final[Path] = Path(__file__).parent.parent
PACKAGE_SOURCE: Final[Path] = PROJ_ROOT / "bindings" / "javascript"
WASM_SOURCE: Final[Path] = PROJ_ROOT / "build" / "wasm"
DEFAULT_OUT_DIR: Final[Path] = PROJ_ROOT / "build" / "npm"
MAX_WASM_BYTES: Final[int] = 465_000
MAX_TARBALL_BYTES: Final[int] = 300_000
PACKAGE_NAME: Final[str] = "@0xf3cd/celestial"
ALIAS_NAME: Final[str] = "celestial-calendar"
ALIAS_SOURCE: Final[Path] = PROJ_ROOT / "bindings" / "javascript-alias"
NPM_METADATA: Final[dict[str, str]] = {PACKAGE_NAME: "npm-pack", ALIAS_NAME: "npm-alias-pack"}
RUNTIME_DEPENDENCY_KEYS: Final[tuple[str, ...]] = ("dependencies", "optionalDependencies", "peerDependencies")
REPOSITORY: Final[dict[str, str]] = {
  "type": "git",
  "url": "git+https://github.com/0xf3cd/celestial-calendar.git",
  "directory": "bindings/javascript",
}
PACKAGE_FILES: Final[dict[Path, str]] = {
  PACKAGE_SOURCE / "README.md": "README.md",
  PROJ_ROOT / "LICENSE": "LICENSE",
  PROJ_ROOT / "THIRD_PARTY_NOTICES.txt": "THIRD_PARTY_NOTICES.txt",
  PACKAGE_SOURCE / "src" / "index.mjs": "index.mjs",
  PACKAGE_SOURCE / "src" / "bindings.mjs": "bindings.mjs",
  PACKAGE_SOURCE / "src" / "validation.mjs": "validation.mjs",
  PACKAGE_SOURCE / "src" / "date.mjs": "date.mjs",
  PACKAGE_SOURCE / "types" / "index.d.ts": "index.d.ts",
  PACKAGE_SOURCE / "types" / "date.d.ts": "date.d.ts",
  WASM_SOURCE / "celestial-jieqi.mjs": "celestial-jieqi.mjs",
  WASM_SOURCE / "celestial-jieqi.wasm": "celestial-jieqi.wasm",
}
WASM_ARTIFACT_FILES: Final[dict[Path, str]] = {
  WASM_SOURCE / "celestial-jieqi.mjs": "celestial-jieqi.mjs",
  WASM_SOURCE / "celestial-jieqi.wasm": "celestial-jieqi.wasm",
  PROJ_ROOT / "LICENSE": "LICENSE",
  PROJ_ROOT / "THIRD_PARTY_NOTICES.txt": "THIRD_PARTY_NOTICES.txt",
}
WASM_ARTIFACT_ALLOWLIST: Final[set[str]] = {
  "celestial-jieqi.mjs",
  "celestial-jieqi.wasm",
  "LICENSE",
  "THIRD_PARTY_NOTICES.txt",
}
PACK_ALLOWLIST: Final[set[str]] = {
  "package.json",
  "README.md",
  "LICENSE",
  "THIRD_PARTY_NOTICES.txt",
  "index.mjs",
  "bindings.mjs",
  "validation.mjs",
  "date.mjs",
  "index.d.ts",
  "date.d.ts",
  "celestial-jieqi.mjs",
  "celestial-jieqi.wasm",
}
ALIAS_ALLOWLIST: Final[set[str]] = {
  "package.json",
  "README.md",
  "LICENSE",
  "index.mjs",
  "index.d.ts",
  "date.mjs",
  "date.d.ts",
}
ALIAS_FILES: Final[dict[Path, str]] = {
  **{ALIAS_SOURCE / name: name for name in ALIAS_ALLOWLIST - {"package.json", "LICENSE"}},
  PROJ_ROOT / "LICENSE": "LICENSE",
}


def project_version() -> str:
  completed = subprocess.run(
    [sys.executable, str(PROJ_ROOT / "project.py"), "--version"],
    cwd=PROJ_ROOT,
    check=True,
    capture_output=True,
    text=True,
  )
  return completed.stdout.strip()


def verify_dependencies(manifest: dict, version: str, package_name: str) -> None:
  if package_name == PACKAGE_NAME:
    if any(manifest.get(key) not in (None, {}) for key in RUNTIME_DEPENDENCY_KEYS):
      raise RuntimeError("JavaScript package must have zero runtime dependencies")
  elif package_name == ALIAS_NAME:
    if manifest.get("dependencies") != {PACKAGE_NAME: version} or any(
      manifest.get(key) not in (None, {}) for key in ("optionalDependencies", "peerDependencies")
    ):
      raise RuntimeError("alias must depend only on the exact same-version primary package")
  else:
    raise RuntimeError(f"Unknown npm package: {package_name}")


def staging_manifest(version: str, package_name: str = PACKAGE_NAME) -> dict:
  package_source = ALIAS_SOURCE if package_name == ALIAS_NAME else PACKAGE_SOURCE
  source = json.loads((package_source / "package.json").read_text(encoding="utf-8"))
  if source.get("private") is not True or source.get("version") != "0.0.0-development":
    raise RuntimeError("development package must stay private at version 0.0.0-development")
  verify_dependencies(source, "0.0.0-development", package_name)

  source.pop("private")
  source.pop("scripts", None)
  source.pop("devDependencies", None)
  source["version"] = version
  if package_name == ALIAS_NAME:
    source["dependencies"] = {PACKAGE_NAME: version}
  source["exports"] = {
    ".": {
      "types": "./index.d.ts",
      "import": "./index.mjs",
      "default": "./index.mjs",
    },
    "./date": {
      "types": "./date.d.ts",
      "import": "./date.mjs",
      "default": "./date.mjs",
    },
  }
  source["types"] = "./index.d.ts"
  allowlist = ALIAS_ALLOWLIST if package_name == ALIAS_NAME else PACK_ALLOWLIST
  source["files"] = sorted(path for path in allowlist if path != "package.json")
  return source


def verify_manifest(manifest: dict, version: str, package_name: str = PACKAGE_NAME) -> None:
  if not isinstance(manifest, dict):
    raise RuntimeError("npm manifest must be an object")
  allowlist = ALIAS_ALLOWLIST if package_name == ALIAS_NAME else PACK_ALLOWLIST
  expected = {
    "name": package_name,
    "version": version,
    "type": "module",
    "types": "./index.d.ts",
    "exports": {
      ".": {
        "types": "./index.d.ts",
        "import": "./index.mjs",
        "default": "./index.mjs",
      },
      "./date": {
        "types": "./date.d.ts",
        "import": "./date.mjs",
        "default": "./date.mjs",
      },
    },
    "files": sorted(path for path in allowlist if path != "package.json"),
    "engines": {"node": ">=22"},
    "repository": {**REPOSITORY, "directory": "bindings/javascript-alias"}
    if package_name == ALIAS_NAME
    else REPOSITORY,
    "license": "MIT",
    "publishConfig": {"access": "public"},
  }
  for key, value in expected.items():
    if manifest.get(key) != value:
      raise RuntimeError(f"staging package {key} mismatch: {manifest.get(key)!r} != {value!r}")
  if "private" in manifest or "scripts" in manifest or "devDependencies" in manifest:
    raise RuntimeError("staging package must not carry development-only metadata")
  verify_dependencies(manifest, version, package_name)


def pack_package(out_dir: Path, version: str, package_name: str) -> Path:
  package_dir = out_dir / ("alias-package" if package_name == ALIAS_NAME else "package")
  package_dir.mkdir(parents=True)

  manifest = staging_manifest(version, package_name)
  verify_manifest(manifest, version, package_name)
  (package_dir / "package.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

  files = ALIAS_FILES if package_name == ALIAS_NAME else PACKAGE_FILES
  for source, target in files.items():
    if not source.is_file():
      raise FileNotFoundError(f"package input does not exist: {source}")
    shutil.copy2(source, package_dir / target)

  if package_name == PACKAGE_NAME:
    wasm_size = (package_dir / "celestial-jieqi.wasm").stat().st_size
    if wasm_size > MAX_WASM_BYTES:
      raise RuntimeError(f"raw WASM size {wasm_size} exceeds {MAX_WASM_BYTES} bytes")
    print(f"[ build_npm ] wasm={wasm_size}/{MAX_WASM_BYTES} bytes")

  npm = shutil.which("npm")
  if npm is None:
    raise FileNotFoundError("npm is not on PATH")
  completed = subprocess.run(
    [npm, "pack", "--json", "--pack-destination", str(out_dir), str(package_dir)],
    cwd=PROJ_ROOT,
    check=True,
    capture_output=True,
    text=True,
  )
  pack_json = json.loads(completed.stdout)
  if not isinstance(pack_json, list) or len(pack_json) != 1:
    raise RuntimeError("npm pack --json must return exactly one package")
  pack = pack_json[0]
  if not isinstance(pack, dict) or pack.get("name") != package_name or pack.get("version") != version:
    raise RuntimeError("npm pack name/version does not match the staging manifest")

  filename = pack.get("filename")
  if not isinstance(filename, str) or re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9._-]*\.tgz", filename) is None:
    raise RuntimeError("Invalid npm pack filename")
  tarball = out_dir / filename
  if not tarball.is_file() or tarball.is_symlink():
    raise RuntimeError("npm pack output must be a regular tarball")
  if tarball.stat().st_size > MAX_TARBALL_BYTES:
    raise RuntimeError(f"npm tarball size {tarball.stat().st_size} exceeds {MAX_TARBALL_BYTES} bytes")

  entries = pack.get("files")
  if not isinstance(entries, list) or any(
    not isinstance(entry, dict) or not isinstance(entry.get("path"), str) for entry in entries
  ):
    raise RuntimeError("Invalid npm pack files list")
  packed_files = {entry["path"] for entry in entries}
  allowlist = ALIAS_ALLOWLIST if package_name == ALIAS_NAME else PACK_ALLOWLIST
  if packed_files != allowlist or len(entries) != len(allowlist):
    raise RuntimeError(f"npm tarball allowlist mismatch: {sorted(packed_files)}")
  with tarfile.open(tarball, "r:gz") as archive:
    members = archive.getmembers()
    if (
      len(members) != len(allowlist)
      or {member.name for member in members} != {f"package/{name}" for name in allowlist}
      or any(not member.isfile() for member in members)
    ):
      raise RuntimeError("packed npm tarball member inventory mismatch")
    for member in members:
      source = archive.extractfile(member)
      if source is None or source.read() != (package_dir / member.name.removeprefix("package/")).read_bytes():
        raise RuntimeError(f"packed npm member differs from staging: {member.name}")
    notice_members = [
      member for member in archive.getmembers() if member.isfile() and member.name == "package/THIRD_PARTY_NOTICES.txt"
    ]
    if package_name == PACKAGE_NAME and len(notice_members) != 1:
      raise RuntimeError("packed npm tarball must contain one canonical notice")
    if package_name == PACKAGE_NAME:
      notice = archive.extractfile(notice_members[0])
      if notice is None or notice.read() != (PROJ_ROOT / "THIRD_PARTY_NOTICES.txt").read_bytes():
        raise RuntimeError("packed npm notice does not match the repository notice")

  metadata = NPM_METADATA[package_name]
  (out_dir / f"{metadata}.json").write_text(completed.stdout, encoding="utf-8")
  digest = hashlib.sha256(tarball.read_bytes()).hexdigest()
  sidecar = out_dir / f"{metadata}.sha256"
  sidecar.write_text(f"{digest}  {tarball.name}\n", encoding="utf-8")
  expected_sidecar = f"{hashlib.sha256(tarball.read_bytes()).hexdigest()}  {tarball.name}\n"
  if sidecar.read_text(encoding="utf-8") != expected_sidecar:
    raise RuntimeError("npm tarball SHA-256 sidecar does not match the tarball")
  print(f"[ build_npm ] {package_name}@{version} tarball={tarball.stat().st_size}/{MAX_TARBALL_BYTES} bytes {tarball}")
  return tarball


def build(out_dir: Path) -> tuple[Path, Path]:
  if out_dir.exists():
    shutil.rmtree(out_dir)
  version = project_version()
  primary = pack_package(out_dir, version, PACKAGE_NAME)
  alias = pack_package(out_dir, version, ALIAS_NAME)
  if primary == alias or set(out_dir.glob("*.tgz")) != {primary, alias}:
    raise RuntimeError("npm output must contain exactly the two metadata-selected tarballs")

  artifact_dir = out_dir / "artifact"
  artifact_dir.mkdir()
  artifact_files = {
    **WASM_ARTIFACT_FILES,
    primary: primary.name,
    alias: alias.name,
    **{
      out_dir / f"{stem}.{suffix}": f"{stem}.{suffix}"
      for stem in NPM_METADATA.values()
      for suffix in ("json", "sha256")
    },
  }
  for source, target in artifact_files.items():
    shutil.copy2(source, artifact_dir / target)
  expected_artifact = (
    WASM_ARTIFACT_ALLOWLIST
    | {primary.name, alias.name}
    | {f"{stem}.{suffix}" for stem in NPM_METADATA.values() for suffix in ("json", "sha256")}
  )
  if {path.name for path in artifact_dir.iterdir()} != expected_artifact or any(
    not path.is_file() or path.is_symlink() for path in artifact_dir.iterdir()
  ):
    raise RuntimeError(f"celestial-wasm artifact staging must contain exactly {len(expected_artifact)} top-level files")
  if (artifact_dir / "THIRD_PARTY_NOTICES.txt").read_bytes() != (PROJ_ROOT / "THIRD_PARTY_NOTICES.txt").read_bytes():
    raise RuntimeError("outer WASM notice does not match the repository notice")

  print(f"[ build_npm ] version={version}")
  return primary, alias


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Stage and pack the primary and alias npm packages.")
  parser.add_argument(
    "--out-dir",
    type=Path,
    default=DEFAULT_OUT_DIR,
    help=f"output directory (default {DEFAULT_OUT_DIR})",
  )
  args = parser.parse_args()
  build(args.out_dir.resolve())
