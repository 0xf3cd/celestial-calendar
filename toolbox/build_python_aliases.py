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
import base64
import csv
import hashlib
import io
import json
import re
import subprocess
import sys
import tomllib
import zipfile

from email.parser import BytesParser
from pathlib import Path
from typing import Final


PROJ_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
PRIMARY_PACKAGE: Final[str] = "celestial-calendar"
ALIAS_NAMES: Final[tuple[str, ...]] = ("cele", "wentian")
ALIAS_ARTIFACT: Final[str] = "celestial-python-aliases"
PYPI_PACKAGES: Final[tuple[str, ...]] = (PRIMARY_PACKAGE, *ALIAS_NAMES)


def alias_wheel_name(name: str, version: str) -> str:
  """Return the sole permitted pure-wheel identity for an alias."""
  if name not in ALIAS_NAMES or re.fullmatch(r"\d+\.\d+\.\d+", version) is None:
    raise ValueError(f"Invalid Python alias identity: {name}@{version}")
  return f"{name}-{version}-py3-none-any.whl"


def alias_readme(name: str, root: Path = PROJ_ROOT) -> str:
  return (root / "bindings/python-aliases/README.md.in").read_text(encoding="utf-8").replace("@NAME@", name)


def alias_configuration(name: str, version: str, root: Path = PROJ_ROOT) -> str:
  """Derive alias build metadata and the dependency pin from the primary package."""
  alias_wheel_name(name, version)
  source = tomllib.loads((root / "bindings/python/pyproject.toml").read_text(encoding="utf-8"))
  project = source["project"]
  metadata = {
    "name": name,
    "version": version,
    "description": project["description"],
    "readme": "README.md",
    "requires-python": project["requires-python"],
    "license": project["license"],
    "license-files": ["LICENSE"],
    "keywords": project["keywords"],
    "classifiers": project["classifiers"],
    "dependencies": [f"{PRIMARY_PACKAGE}=={version}"],
  }
  lines = ["[build-system]"]
  lines.extend(f"{key} = {json.dumps(value)}" for key, value in source["build-system"].items())
  lines.extend(["", "[project]"])
  lines.extend(f"{key} = {json.dumps(value)}" for key, value in metadata.items())
  authors = [
    "{" + ", ".join(f"{key} = {json.dumps(value)}" for key, value in author.items()) + "}"
    for author in project["authors"]
  ]
  lines.extend([f"authors = [{', '.join(authors)}]", "", "[project.urls]"])
  lines.extend(f"{key} = {json.dumps(value)}" for key, value in project["urls"].items())
  lines.extend(
    [
      "",
      "[tool.scikit-build]",
      "wheel.cmake = false",
      f'wheel.packages = ["src/{name}"]',
    ]
  )
  return "\n".join(lines) + "\n"


def validate_alias_wheel(wheel: Path, name: str, version: str, root: Path = PROJ_ROOT) -> None:
  """Verify an alias's complete payload, metadata, source bytes and RECORD."""
  if wheel.name != alias_wheel_name(name, version):
    raise RuntimeError(f"Unexpected Python alias wheel: {wheel.name}")
  dist_info = f"{name}-{version}.dist-info"
  expected = {
    f"{name}/__init__.py": (root / "bindings/python-aliases/__init__.py").read_bytes(),
    f"{name}/py.typed": b"",
    f"{dist_info}/licenses/LICENSE": (root / "LICENSE").read_bytes(),
  }
  record_name = f"{dist_info}/RECORD"
  members = {*expected, f"{dist_info}/METADATA", f"{dist_info}/WHEEL", record_name}
  try:
    with zipfile.ZipFile(wheel) as archive:
      names = archive.namelist()
      if len(names) != len(set(names)) or set(names) != members:
        raise RuntimeError(f"Python alias member inventory mismatch: {wheel.name}")
      for member, content in expected.items():
        if archive.read(member) != content:
          raise RuntimeError(f"Python alias source/license mismatch: {member}")

      metadata = BytesParser().parsebytes(archive.read(f"{dist_info}/METADATA"))
      project = tomllib.loads((root / "bindings/python/pyproject.toml").read_text(encoding="utf-8"))["project"]
      for key, value in {
        "Name": name,
        "Version": version,
        "Requires-Python": project["requires-python"],
        "License-Expression": project["license"],
        "Summary": project["description"],
        "Description-Content-Type": "text/markdown",
      }.items():
        if metadata.get_all(key) != [value]:
          raise RuntimeError(f"Python alias metadata mismatch: {name} {key}")
      requirements = metadata.get_all("Requires-Dist") or []
      if (
        len(requirements) != 1
        or re.fullmatch(rf"(?i:celestial[-_.]+calendar)\s*==\s*{re.escape(version)}", requirements[0]) is None
      ):
        raise RuntimeError(f"Python alias must require only {PRIMARY_PACKAGE}=={version}: {name}")
      if metadata.get_all("Provides-Extra") or metadata.get_all("License-File") != ["LICENSE"]:
        raise RuntimeError(f"Python alias extras/license metadata mismatch: {name}")
      urls = [f"{label}, {url}" for label, url in project["urls"].items()]
      if sorted(metadata.get_all("Project-URL") or []) != sorted(urls):
        raise RuntimeError(f"Python alias project URLs mismatch: {name}")
      if sorted(metadata.get_all("Classifier") or []) != sorted(project["classifiers"]):
        raise RuntimeError(f"Python alias classifiers mismatch: {name}")
      if metadata.get_payload() != alias_readme(name, root):
        raise RuntimeError(f"Python alias README mismatch: {name}")

      wheel_metadata = BytesParser().parsebytes(archive.read(f"{dist_info}/WHEEL"))
      for key, value in {"Wheel-Version": "1.0", "Root-Is-Purelib": "true", "Tag": "py3-none-any"}.items():
        if wheel_metadata.get_all(key) != [value]:
          raise RuntimeError(f"Python alias WHEEL metadata mismatch: {name} {key}")

      records = list(csv.reader(io.StringIO(archive.read(record_name).decode("utf-8"))))
      if any(len(row) != 3 for row in records):
        raise RuntimeError(f"Malformed Python alias RECORD: {name}")
      if len(records) != len(members) or {row[0] for row in records} != members:
        raise RuntimeError(f"Python alias RECORD inventory mismatch: {name}")
      for member, digest, size in records:
        if member == record_name:
          if digest or size:
            raise RuntimeError(f"Python alias RECORD must not hash itself: {name}")
          continue
        content = archive.read(member)
        actual = base64.urlsafe_b64encode(hashlib.sha256(content).digest()).rstrip(b"=").decode("ascii")
        if digest != f"sha256={actual}" or size != str(len(content)):
          raise RuntimeError(f"Python alias RECORD integrity mismatch: {member}")
  except (zipfile.BadZipFile, UnicodeDecodeError, csv.Error) as error:
    raise RuntimeError(f"Invalid Python alias wheel {wheel.name}: {error}") from error


def build(out_dir: Path, root: Path = PROJ_ROOT) -> tuple[Path, ...]:
  """Build each alias once and retain its exact wheel and SHA-256 sidecar."""
  if out_dir.exists():
    raise FileExistsError(f"Refusing to overwrite Python alias build: {out_dir}")
  version = subprocess.run(
    [sys.executable, str(root / "project.py"), "--version"], check=True, capture_output=True, text=True
  ).stdout.strip()
  artifact = out_dir / "artifact"
  artifact.mkdir(parents=True)
  wheels = []
  for name in ALIAS_NAMES:
    source = out_dir / "sources" / name
    package = source / "src" / name
    package.mkdir(parents=True)
    (package / "__init__.py").write_bytes((root / "bindings/python-aliases/__init__.py").read_bytes())
    (package / "py.typed").write_bytes((root / "bindings/python-aliases/py.typed").read_bytes())
    (source / "LICENSE").write_bytes((root / "LICENSE").read_bytes())
    (source / "README.md").write_text(alias_readme(name, root), encoding="utf-8", newline="\n")
    (source / "pyproject.toml").write_text(alias_configuration(name, version, root), encoding="utf-8", newline="\n")
    subprocess.run(
      [sys.executable, "-m", "build", "--wheel", "--no-isolation", "--outdir", str(artifact), str(source)],
      check=True,
    )
    wheel = artifact / alias_wheel_name(name, version)
    validate_alias_wheel(wheel, name, version, root)
    digest = hashlib.sha256(wheel.read_bytes()).hexdigest()
    wheel.with_name(f"{wheel.name}.sha256").write_text(f"{digest}  {wheel.name}\n", encoding="ascii", newline="\n")
    wheels.append(wheel)
  expected = {filename for wheel in wheels for filename in (wheel.name, f"{wheel.name}.sha256")}
  if {path.name for path in artifact.iterdir()} != expected:
    raise RuntimeError("Python alias artifact must contain exactly two wheels and their sidecars")
  return tuple(wheels)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Build the cele and wentian Python forwarding wheels.")
  parser.add_argument("--out-dir", type=Path, default=PROJ_ROOT / "build/python-aliases")
  args = parser.parse_args()
  for built in build(args.out_dir.resolve()):
    print(built)
