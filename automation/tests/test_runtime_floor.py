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
from types import SimpleNamespace

import pytest
import yaml

from toolbox import runtime_floor
from toolbox.release_validation import _runtime_matrix


REPO = Path(__file__).resolve().parents[2]
BUILD_WORKFLOW = REPO / ".github" / "workflows" / "build_and_test.yml"
WHEEL_WORKFLOW = REPO / ".github" / "workflows" / "python-wheel.yml"


def test_parse_elf_versions_selects_natural_maximum():
  # Excerpt from readelf --version-info on run 32228080991's linux_amd64 artifact.
  output = """
    Name: GLIBC_2.2.5  Flags: none  Version: 17
    Name: GLIBC_2.26   Flags: none  Version: 12
    Name: GLIBCXX_3.4  Flags: none  Version: 11
    Name: GLIBCXX_3.4.21  Flags: none  Version: 4
    Name: CXXABI_1.3.9  Flags: none  Version: 7
    Name: GCC_4.3.0  Flags: none  Version: 13
  """

  assert runtime_floor.parse_elf_versions(output) == {"glibc": "2.26", "glibcxx": "3.4.21"}


@pytest.mark.parametrize("output", ["GLIBC_2.28", "GLIBCXX_3.4.21", ""])
def test_parse_elf_versions_requires_both_families(output):
  with pytest.raises(RuntimeError, match="GLIBC and GLIBCXX"):
    runtime_floor.parse_elf_versions(output)


def test_parse_macos_versions_selects_natural_maximum():
  output = "minos 9.0\nminos 14.0\n"

  assert runtime_floor.parse_macos_versions(output) == {"macos": "14.0"}


def test_parse_windows_imports_rejects_dynamic_runtime():
  for dependency in ("VCRUNTIME140.dll", "MSVCP140.dll", "msvcp140d.dll", "vcruntime140.dll"):
    with pytest.raises(RuntimeError, match=r"dynamic Visual C\+\+ runtime"):
      runtime_floor.parse_windows_imports(f"Name: {dependency}")

  output = "Name: KERNEL32.dll\nName: api-ms-win-crt-runtime-l1-1-0.dll\nName: ucrtbase.dll"
  assert runtime_floor.parse_windows_imports(output) == {"msvc_runtime": "static"}


@pytest.mark.parametrize(
  ("artifact", "command", "output", "expected"),
  [
    ("linux_amd64", "readelf", "GLIBC_2.28 GLIBCXX_3.4.21", {"glibc": "2.28", "glibcxx": "3.4.21"}),
    ("linux_arm64", "readelf", "GLIBC_2.17 GLIBCXX_3.4.21", {"glibc": "2.17", "glibcxx": "3.4.21"}),
    ("macos_arm64", "otool", "minos 14.0", {"macos": "14.0"}),
    ("windows_x86_64", "llvm-readobj", "Name: KERNEL32.dll", {"msvc_runtime": "static"}),
  ],
)
def test_inspect_uses_platform_object_tool(monkeypatch, tmp_path, artifact, command, output, expected):
  binary = tmp_path / "library"
  calls = []

  def run(args, **kwargs):
    calls.append((args, kwargs))
    return SimpleNamespace(stdout=output)

  monkeypatch.setattr(runtime_floor.subprocess, "run", run)

  assert runtime_floor.inspect(artifact, binary) == expected
  expected_args = [command, "--version-info", str(binary)] if command == "readelf" else None
  if command == "otool":
    expected_args = [command, "-l", str(binary)]
  elif command == "llvm-readobj":
    expected_args = [command, "--coff-imports", str(binary)]
  assert calls == [
    (
      expected_args,
      {"check": True, "text": True, "stdout": runtime_floor.subprocess.PIPE, "stderr": runtime_floor.subprocess.STDOUT},
    )
  ]


def test_inspect_rejects_unknown_artifact(tmp_path):
  with pytest.raises(RuntimeError, match="Unknown native artifact"):
    runtime_floor.inspect("freebsd_amd64", tmp_path / "library")


def test_record_runtime_floor_updates_build_info(tmp_path, monkeypatch):
  binary = tmp_path / "libcelestial_calendar.so"
  binary.write_bytes(b"library")
  build_info = tmp_path / "build_info.json"
  original = {"build_version": "0.6.0", "sha256": {"libcelestial_calendar.so": "digest"}}
  build_info.write_text(f"{json.dumps(original)}\n", encoding="utf-8")
  monkeypatch.setattr(
    runtime_floor,
    "inspect",
    lambda _artifact, _binary: {"glibc": "2.26", "glibcxx": "3.4.21"},
  )

  runtime_floor.record_runtime_floor("linux_amd64", binary, build_info)

  expected = original | {"runtime_floor": {
    "supported": {"glibc": "2.28", "glibcxx": "3.4.21"},
    "measured": {"glibc": "2.26", "glibcxx": "3.4.21"},
  }}
  assert json.loads(build_info.read_text(encoding="utf-8")) == expected
  assert build_info.read_bytes().endswith(b"\n")


def test_record_runtime_floor_rejects_invalid_inputs(tmp_path):
  binary = tmp_path / "library"
  binary.write_bytes(b"library")
  build_info = tmp_path / "build_info.json"
  build_info.write_text("{}", encoding="utf-8")

  with pytest.raises(RuntimeError, match="Unknown native artifact"):
    runtime_floor.record_runtime_floor("freebsd_amd64", binary, build_info)
  with pytest.raises(RuntimeError, match="Native runtime library is not a file"):
    runtime_floor.record_runtime_floor("linux_amd64", tmp_path / "missing", build_info)

  for content, message in (("not json", "Invalid build info"), ("[]", "Build info must be a JSON object")):
    build_info.write_text(content, encoding="utf-8")
    with pytest.raises(RuntimeError, match=message):
      runtime_floor.record_runtime_floor("linux_amd64", binary, build_info)


def test_record_runtime_floor_rejects_requirement_above_support(tmp_path, monkeypatch):
  binary = tmp_path / "library"
  binary.write_bytes(b"library")
  build_info = tmp_path / "build_info.json"
  build_info.write_text("{}", encoding="utf-8")
  monkeypatch.setattr(
    runtime_floor,
    "inspect",
    lambda _artifact, _binary: {"glibc": "2.34", "glibcxx": "3.4.21"},
  )

  with pytest.raises(RuntimeError, match="Measured glibc requirement 2.34 exceeds supported 2.28"):
    runtime_floor.record_runtime_floor("linux_amd64", binary, build_info)


def test_native_and_wheel_workflows_share_manylinux_digests():
  pattern = r"quay\.io/pypa/manylinux_2_28_(x86_64|aarch64)@sha256:[0-9a-f]{64}"
  native_workflow = yaml.safe_load(BUILD_WORKFLOW.read_text(encoding="utf-8"))
  wheel_text = WHEEL_WORKFLOW.read_text(encoding="utf-8")
  wheel_workflow = yaml.safe_load(wheel_text)
  assert native_workflow["jobs"]["linux-docker"]["name"] == (
    "linux-docker (${{ matrix.platform }}, ${{ matrix.runner }})"
  )
  native_images = {
    row["platform"].removeprefix("linux/").replace("amd64", "x86_64").replace("arm64", "aarch64"):
      row["image"]
    for row in native_workflow["jobs"]["linux-docker"]["strategy"]["matrix"]["include"]
  }
  wheel_images = {
    row["identifier"].removeprefix("cp311-manylinux_"): row["image"]
    for row in wheel_workflow["jobs"]["manylinux"]["strategy"]["matrix"]["include"]
  }

  assert native_images == wheel_images
  assert set(native_images) == {"x86_64", "aarch64"}
  assert all(re.fullmatch(pattern, image) for image in native_images.values())
  assert "CIBW_MANYLINUX_X86_64_IMAGE: manylinux_2_28" not in wheel_text
  assert "CIBW_MANYLINUX_AARCH64_IMAGE: manylinux_2_28" not in wheel_text


def test_macos_workflows_match_supported_runtime():
  supported = runtime_floor.SUPPORTED_RUNTIME["macos_arm64"]["macos"]
  native_workflow = yaml.safe_load(BUILD_WORKFLOW.read_text(encoding="utf-8"))
  wheel_workflow = yaml.safe_load(WHEEL_WORKFLOW.read_text(encoding="utf-8"))
  native_targets = [
    step["env"]["MACOSX_DEPLOYMENT_TARGET"]
    for step in native_workflow["jobs"]["macos"]["steps"]
    if "MACOSX_DEPLOYMENT_TARGET" in step.get("env", {})
  ]
  wheel_env = wheel_workflow["jobs"]["macos-arm64"]["env"]

  assert native_targets == [supported, supported]
  assert wheel_env["MACOSX_DEPLOYMENT_TARGET"] == supported
  assert f"MACOSX_DEPLOYMENT_TARGET={supported}" in wheel_env["CIBW_ENVIRONMENT_MACOS"]


def test_supported_runtime_matches_readme():
  assert runtime_floor.SUPPORTED_RUNTIME == {
    artifact: runtime["supported"] for artifact, runtime in _runtime_matrix().items()
  }


def test_native_workflow_records_each_runtime_floor():
  workflow = BUILD_WORKFLOW.read_text(encoding="utf-8")

  assert workflow.count("toolbox/runtime_floor.py") == 3
  assert '--artifact "$ARTIFACT_NAME"' in workflow
  assert "--artifact macos_arm64" in workflow
  assert "--artifact windows_x86_64" in workflow
