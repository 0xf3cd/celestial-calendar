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

import argparse
import hashlib
import json
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
  PACKAGE_SOURCE / "types" / "index.d.ts": "index.d.ts",
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
  "index.d.ts",
  "celestial-jieqi.mjs",
  "celestial-jieqi.wasm",
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


def staging_manifest(version: str) -> dict:
  source = json.loads((PACKAGE_SOURCE / "package.json").read_text(encoding="utf-8"))
  if source.get("private") is not True or source.get("version") != "0.0.0-development":
    raise RuntimeError("development package must stay private at version 0.0.0-development")
  if source.get("dependencies") not in (None, {}):
    raise RuntimeError("JavaScript package must have zero runtime dependencies")

  source.pop("private")
  source.pop("scripts", None)
  source.pop("devDependencies", None)
  source["version"] = version
  source["exports"] = {
    ".": {
      "types": "./index.d.ts",
      "import": "./index.mjs",
      "default": "./index.mjs",
    },
  }
  source["types"] = "./index.d.ts"
  source["files"] = sorted(path for path in PACK_ALLOWLIST if path != "package.json")
  return source


def verify_manifest(manifest: dict, version: str) -> None:
  expected = {
    "name": PACKAGE_NAME,
    "version": version,
    "type": "module",
    "types": "./index.d.ts",
    "engines": {"node": ">=22"},
    "repository": REPOSITORY,
    "license": "GPL-3.0-or-later",
    "publishConfig": {"access": "public"},
  }
  for key, value in expected.items():
    if manifest.get(key) != value:
      raise RuntimeError(f"staging package {key} mismatch: {manifest.get(key)!r} != {value!r}")
  if "private" in manifest or "scripts" in manifest or "devDependencies" in manifest:
    raise RuntimeError("staging package must not carry development-only metadata")
  if manifest.get("dependencies") not in (None, {}):
    raise RuntimeError("staging package must be public with zero runtime dependencies")


def build(out_dir: Path) -> Path:
  if out_dir.exists():
    shutil.rmtree(out_dir)
  package_dir = out_dir / "package"
  package_dir.mkdir(parents=True)

  version = project_version()
  manifest = staging_manifest(version)
  verify_manifest(manifest, version)
  (package_dir / "package.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

  for source, target in PACKAGE_FILES.items():
    if not source.is_file():
      raise FileNotFoundError(f"package input does not exist: {source}")
    shutil.copy2(source, package_dir / target)

  wasm_size = (package_dir / "celestial-jieqi.wasm").stat().st_size
  if wasm_size > MAX_WASM_BYTES:
    raise RuntimeError(f"raw WASM size {wasm_size} exceeds {MAX_WASM_BYTES} bytes")

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
  if pack.get("name") != PACKAGE_NAME or pack.get("version") != version:
    raise RuntimeError("npm pack name/version does not match the staging manifest")

  tarballs = list(out_dir.glob("*.tgz"))
  tarball = out_dir / pack["filename"]
  if len(tarballs) != 1 or tarballs[0] != tarball or not tarball.is_file():
    raise RuntimeError("npm pack output must be the only top-level tarball")
  if tarball.stat().st_size > MAX_TARBALL_BYTES:
    raise RuntimeError(f"npm tarball size {tarball.stat().st_size} exceeds {MAX_TARBALL_BYTES} bytes")

  packed_files = {entry["path"] for entry in pack.get("files", [])}
  if packed_files != PACK_ALLOWLIST:
    raise RuntimeError(f"npm tarball allowlist mismatch: {sorted(packed_files)}")
  with tarfile.open(tarball, "r:gz") as archive:
    notice_members = [
      member for member in archive.getmembers() if member.isfile() and member.name == "package/THIRD_PARTY_NOTICES.txt"
    ]
    if len(notice_members) != 1:
      raise RuntimeError("packed npm tarball must contain one canonical notice")
    notice = archive.extractfile(notice_members[0])
    if notice is None or notice.read() != (PROJ_ROOT / "THIRD_PARTY_NOTICES.txt").read_bytes():
      raise RuntimeError("packed npm notice does not match the repository notice")

  (out_dir / "npm-pack.json").write_text(completed.stdout, encoding="utf-8")
  digest = hashlib.sha256(tarball.read_bytes()).hexdigest()
  sidecar = out_dir / "npm-pack.sha256"
  sidecar.write_text(f"{digest}  {tarball.name}\n", encoding="utf-8")
  expected_sidecar = f"{hashlib.sha256(tarball.read_bytes()).hexdigest()}  {tarball.name}\n"
  if sidecar.read_text(encoding="utf-8") != expected_sidecar:
    raise RuntimeError("npm tarball SHA-256 sidecar does not match the tarball")

  artifact_dir = out_dir / "artifact"
  artifact_dir.mkdir()
  artifact_files = {
    **WASM_ARTIFACT_FILES,
    tarball: tarball.name,
    out_dir / "npm-pack.json": "npm-pack.json",
    out_dir / "npm-pack.sha256": "npm-pack.sha256",
  }
  for source, target in artifact_files.items():
    shutil.copy2(source, artifact_dir / target)
  expected_artifact = WASM_ARTIFACT_ALLOWLIST | {tarball.name, "npm-pack.json", "npm-pack.sha256"}
  if {path.name for path in artifact_dir.iterdir()} != expected_artifact:
    raise RuntimeError(f"celestial-wasm artifact staging must contain exactly {len(expected_artifact)} top-level files")
  if (artifact_dir / "THIRD_PARTY_NOTICES.txt").read_bytes() != (PROJ_ROOT / "THIRD_PARTY_NOTICES.txt").read_bytes():
    raise RuntimeError("outer WASM notice does not match the repository notice")

  print(f"[ build_npm ] version={version}")
  print(f"[ build_npm ] wasm={wasm_size}/{MAX_WASM_BYTES} bytes")
  print(f"[ build_npm ] tarball={tarball.stat().st_size}/{MAX_TARBALL_BYTES} bytes")
  print(f"[ build_npm ] {tarball}")
  return tarball


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Stage and pack the @0xf3cd/celestial npm package.")
  parser.add_argument(
    "--out-dir",
    type=Path,
    default=DEFAULT_OUT_DIR,
    help=f"output directory (default {DEFAULT_OUT_DIR})",
  )
  args = parser.parse_args()
  build(args.out_dir.resolve())
