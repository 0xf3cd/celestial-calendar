# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import re
import shutil
import string
import subprocess
import sys
import tomllib

from pathlib import Path

from toolbox import build_npm


ROOT = Path(__file__).resolve().parents[2]


def test_one_isolated_version_reaches_cli_wheel_and_staged_npm(tmp_path, monkeypatch):
  version = "9.8.7"
  shutil.copytree(ROOT / "automation", tmp_path / "automation", ignore=shutil.ignore_patterns("tests", "__pycache__"))
  (tmp_path / "src/shared_lib").mkdir(parents=True)
  for relative in ("src/CMakeLists.txt", "src/shared_lib/CMakeLists.txt", "Requirements.txt"):
    shutil.copyfile(ROOT / relative, tmp_path / relative)
  source = (ROOT / "project.py").read_text()
  source, count = re.subn(r'BUILD_VERSION: Final\[str\] = "[^"]+"', f'BUILD_VERSION: Final[str] = "{version}"', source)
  assert count == 1
  (tmp_path / "project.py").write_text(source)
  result = subprocess.run(
    [sys.executable, "-S", "-B", str(tmp_path / "project.py"), "--version"], capture_output=True, text=True
  )
  assert result.returncode == 0, result.stdout + result.stderr
  assert result.stdout.strip() == version

  metadata_path = Path("bindings/python/pyproject.toml")
  (tmp_path / metadata_path).parent.mkdir(parents=True)
  shutil.copyfile(ROOT / metadata_path, tmp_path / metadata_path)
  config = tomllib.loads((tmp_path / metadata_path).read_text())
  metadata = next(item for item in config["tool"]["dynamic-metadata"] if item["field"] == "version")
  assert metadata["provider"] == "scikit_build_core.metadata.regex"
  metadata_input = (tmp_path / metadata_path).parent / metadata["input"]
  wheel_version = re.search(metadata["regex"], metadata_input.read_text())["value"]
  assert wheel_version == version
  generated = next(item for item in config["tool"]["scikit-build"]["generate"] if item["path"].endswith("_version.py"))
  assert string.Template(generated["template"]).substitute(version=wheel_version).strip() == f'VERSION = "{version}"'

  monkeypatch.setattr(build_npm, "PROJ_ROOT", tmp_path)
  assert build_npm.project_version() == version
  for name in (build_npm.PACKAGE_NAME, build_npm.ALIAS_NAME):
    manifest = build_npm.staging_manifest(build_npm.project_version(), name)
    assert manifest["version"] == version and "private" not in manifest
    if name == build_npm.ALIAS_NAME:
      assert manifest["dependencies"] == {build_npm.PACKAGE_NAME: version}
