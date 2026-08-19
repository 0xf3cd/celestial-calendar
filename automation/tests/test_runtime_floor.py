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

import json
import re
from pathlib import Path

import pytest

from toolbox import runtime_floor


REPO = Path(__file__).resolve().parents[2]
BUILD_WORKFLOW = REPO / ".github" / "workflows" / "build_and_test.yml"
WHEEL_WORKFLOW = REPO / ".github" / "workflows" / "python-wheel.yml"


def test_parse_elf_versions_selects_natural_maximum():
  output = "GLIBC_2.9 GLIBC_2.28 GLIBCXX_3.4.9 GLIBCXX_3.4.21"

  assert runtime_floor.parse_elf_versions(output) == {"glibc": "2.28", "glibcxx": "3.4.21"}


@pytest.mark.parametrize("output", ["GLIBC_2.28", "GLIBCXX_3.4.21", ""])
def test_parse_elf_versions_requires_both_families(output):
  with pytest.raises(RuntimeError, match="GLIBC and GLIBCXX"):
    runtime_floor.parse_elf_versions(output)


def test_parse_macos_versions_selects_natural_maximum():
  output = "minos 9.0\nminos 14.0\n"

  assert runtime_floor.parse_macos_versions(output) == {"macos": "14.0"}


def test_parse_windows_imports_rejects_dynamic_runtime():
  with pytest.raises(RuntimeError, match=r"dynamic Visual C\+\+ runtime"):
    runtime_floor.parse_windows_imports("Name: VCRUNTIME140.dll")

  assert runtime_floor.parse_windows_imports("Name: KERNEL32.dll") == {"msvc_runtime": "static"}


def test_record_runtime_floor_updates_build_info(tmp_path, monkeypatch):
  binary = tmp_path / "libcelestial_calendar.so"
  binary.write_bytes(b"library")
  build_info = tmp_path / "build_info.json"
  build_info.write_text('{"build_version": "0.6.0"}\n', encoding="utf-8")
  monkeypatch.setattr(runtime_floor, "inspect", lambda _artifact, _binary: {"glibc": "2.26"})

  runtime_floor.record_runtime_floor("linux_amd64", binary, build_info)

  assert json.loads(build_info.read_text(encoding="utf-8"))["runtime_floor"] == {
    "supported": {"glibc": "2.28"},
    "measured": {"glibc": "2.26"},
  }


def test_native_and_wheel_workflows_share_manylinux_digests():
  pattern = r"quay\.io/pypa/manylinux_2_28_(?:x86_64|aarch64)@sha256:[0-9a-f]{64}"
  native_images = set(re.findall(pattern, BUILD_WORKFLOW.read_text(encoding="utf-8")))
  wheel_text = WHEEL_WORKFLOW.read_text(encoding="utf-8")

  assert len(native_images) == 2
  assert set(re.findall(pattern, wheel_text)) == native_images
  assert "CIBW_MANYLINUX_X86_64_IMAGE: manylinux_2_28" not in wheel_text
  assert "CIBW_MANYLINUX_AARCH64_IMAGE: manylinux_2_28" not in wheel_text


def test_native_workflow_records_each_runtime_floor():
  workflow = BUILD_WORKFLOW.read_text(encoding="utf-8")

  assert workflow.count("toolbox/runtime_floor.py") == 3
  assert '--artifact "$ARTIFACT_NAME"' in workflow
  assert "--artifact macos_arm64" in workflow
  assert "--artifact windows_x86_64" in workflow
