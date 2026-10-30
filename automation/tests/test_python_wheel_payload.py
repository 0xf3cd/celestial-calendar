# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Metadata-only wheel fixtures; native architecture and execution have separate gates."""

import base64
import csv
import hashlib
import importlib.util
import io
import json
import sys
import tomllib
import zipfile
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "bindings/python/src/celestial_calendar"


@pytest.fixture
def verifier():
  path = ROOT / "bindings/python/test/wheel/verify.py"
  spec = importlib.util.spec_from_file_location("wheel_payload_verifier", path)
  module = importlib.util.module_from_spec(spec)
  original_path = list(sys.path)
  try:
    spec.loader.exec_module(module)
  finally:
    sys.path[:] = original_path
  return module


def wheel_payload(version):
  dist_info = f"celestial_calendar-{version}.dist-info"
  project = tomllib.loads((ROOT / "bindings/python/pyproject.toml").read_text(encoding="utf-8"))["project"]
  metadata = [
    "Metadata-Version: 2.4",
    "Name: celestial-calendar",
    f"Version: {version}",
    "Requires-Python: >=3.11",
    "License-Expression: MIT",
    *[f"Classifier: {value}" for value in project["classifiers"]],
    "Keywords: " + ",".join(project["keywords"]),
    *[f"Project-URL: {label}, {url}" for label, url in project["urls"].items()],
  ]
  members = {
    f"celestial_calendar/{name}": (SOURCE / name).read_bytes()
    for name in (
      "__init__.py",
      "_binding.py",
      "_abi.py",
      "_codes.py",
      "_models.py",
      "_protocol.py",
      "capability.py",
      "chart.py",
      "events.py",
      "query.py",
      "py.typed",
    )
  }
  members.update(
    {
      "celestial_calendar/_version.py": f'VERSION = "{version}"'.encode(),
      "celestial_calendar/_native/_celestial_calendar.dll": b"metadata-only native placeholder",
      f"{dist_info}/METADATA": ("\n".join(metadata) + "\n").encode(),
      f"{dist_info}/WHEEL": b"Wheel-Version: 1.0\nRoot-Is-Purelib: false\nTag: py3-none-win_amd64\n",
      f"{dist_info}/licenses/LICENSE": (ROOT / "LICENSE").read_bytes(),
      f"{dist_info}/licenses/THIRD_PARTY_NOTICES.txt": (ROOT / "THIRD_PARTY_NOTICES.txt").read_bytes(),
    }
  )
  return members


def archive(members, version, fault=None):
  record_name = f"celestial_calendar-{version}.dist-info/RECORD"
  rows = io.StringIO()
  writer = csv.writer(rows)
  for name, data in members.items():
    if fault == "record-missing" and name == "celestial_calendar/_abi.py":
      continue
    digest = base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b"=").decode()
    size = str(len(data))
    if name == "celestial_calendar/_abi.py":
      if fault == "record-hash":
        digest = "invalid-digest"
      elif fault == "record-size":
        size = str(len(data) + 1)
      elif fault == "record-duplicate":
        writer.writerow((name, "sha256=" + digest, size))
    writer.writerow((name, "sha256=" + digest, size))
  if fault == "record-phantom":
    writer.writerow(("celestial_calendar/ghost.py", "sha256=invalid", "1"))
  elif fault == "record-malformed":
    writer.writerow(("celestial_calendar/_abi.py", "", "", "extra"))
  writer.writerow((record_name, "sha256=invalid" if fault == "record-self-hash" else "", ""))
  result = io.BytesIO()
  with zipfile.ZipFile(result, "w") as output:
    for name, data in members.items():
      output.writestr(name, data)
    if fault == "zip-duplicate":
      with pytest.warns(UserWarning, match="Duplicate name"):
        output.writestr("celestial_calendar/_abi.py", members["celestial_calendar/_abi.py"])
    output.writestr(record_name, rows.getvalue())
  result.seek(0)
  return zipfile.ZipFile(result)


def check(verifier, members, version, fault=None):
  with archive(members, version, fault) as payload:
    return verifier.verify_metadata(
      payload, Path(f"celestial_calendar-{version}-py3-none-win_amd64.whl"), version, ["win_amd64"]
    )


def test_wheel_metadata_accepts_the_exact_source_payload(verifier):
  version = verifier.project_version()
  assert check(verifier, wheel_payload(version), version) == "celestial_calendar/_native/_celestial_calendar.dll"


def test_macos_wheel_metadata_accepts_the_supported_tag_and_dylib(verifier):
  version = verifier.project_version()
  members = wheel_payload(version)
  native = "celestial_calendar/_native/_celestial_calendar.dylib"
  members[native] = members.pop("celestial_calendar/_native/_celestial_calendar.dll")
  members[f"celestial_calendar-{version}.dist-info/WHEEL"] = (
    b"Wheel-Version: 1.0\nRoot-Is-Purelib: false\nTag: py3-none-macosx_15_0_arm64\n"
  )
  with archive(members, version) as payload:
    assert (
      verifier.verify_metadata(
        payload,
        Path(f"celestial_calendar-{version}-py3-none-macosx_15_0_arm64.whl"),
        version,
        ["macosx_15_0_arm64"],
      )
      == native
    )


@pytest.mark.parametrize("minimums", ["minos 15.0", "minos 14.0", "minos 16.0", "minos 15.0\nminos 16.0", ""])
def test_macos_wheel_binary_requires_the_supported_deployment_target(verifier, tmp_path, monkeypatch, minimums):
  native = tmp_path / "library.dylib"
  outputs = {
    ("file", str(native)): "Mach-O 64-bit dynamically linked shared library arm64",
    ("otool", "-l", str(native)): minimums,
    ("otool", "-L", str(native)): f"{native}:\n/usr/lib/libSystem.B.dylib (compatibility version 1.0.0)",
    ("nm", "-gU", str(native)): "0000000000000000 T _celestial_version",
  }
  monkeypatch.setattr(verifier, "run", lambda *command: outputs[command])
  if minimums == "minos 15.0":
    verifier.verify_macos(native, {"celestial_version"})
  else:
    with pytest.raises(AssertionError):
      verifier.verify_macos(native, {"celestial_version"})


@pytest.mark.parametrize("field", ["Classifier", "Keywords", "Project-URL: Documentation"])
@pytest.mark.parametrize("fault", ["omitted", "wrong"])
def test_wheel_project_metadata_rejects_drift_with_recomputed_record(verifier, field, fault):
  version = verifier.project_version()
  members = wheel_payload(version)
  name = f"celestial_calendar-{version}.dist-info/METADATA"
  lines = members[name].decode().splitlines()
  prefix = field + (", " if field.startswith("Project-URL:") else ": ")
  index = next(index for index, line in enumerate(lines) if line.startswith(prefix))
  if fault == "omitted":
    del lines[index]
  else:
    lines[index] = prefix + "wrong-value"
  members[name] = ("\n".join(lines) + "\n").encode()
  message = {"Classifier": "classifiers", "Keywords": "keywords", "Project-URL: Documentation": "project URLs"}[field]
  with pytest.raises(AssertionError, match=f"wheel {message} differ from pyproject"):
    check(verifier, members, version)
  check(verifier, wheel_payload(version), version)


def test_wheel_project_metadata_accepts_reordered_headers_and_keywords(verifier):
  version = verifier.project_version()
  members = wheel_payload(version)
  name = f"celestial_calendar-{version}.dist-info/METADATA"
  lines = members[name].decode().splitlines()
  index = next(index for index, line in enumerate(lines) if line.startswith("Keywords: "))
  keywords = lines[index].removeprefix("Keywords: ").split(",")
  lines[index] = "Keywords: " + ", ".join(reversed(keywords))
  members[name] = ("\n".join(reversed(lines)) + "\n").encode()
  check(verifier, members, version)


def test_wheel_allowlist_requires_the_real_declaration_module(verifier):
  version = verifier.project_version()
  members = wheel_payload(version)
  del members["celestial_calendar/_abi.py"]
  with pytest.raises(AssertionError, match="wheel allowlist mismatch"):
    check(verifier, members, version)


@pytest.mark.parametrize(
  "name",
  [
    "__init__.py",
    "_binding.py",
    "_abi.py",
    "_codes.py",
    "_models.py",
    "_protocol.py",
    "capability.py",
    "chart.py",
    "events.py",
    "query.py",
    "py.typed",
  ],
)
def test_wheel_source_bytes_are_checked_even_with_a_valid_record(verifier, name):
  version = verifier.project_version()
  members = wheel_payload(version)
  members[f"celestial_calendar/{name}"] += b"\n"
  with pytest.raises(AssertionError, match="source payload mismatch|^$"):
    check(verifier, members, version)


@pytest.mark.parametrize(
  "fault,message",
  [
    ("record-missing", "RECORD mismatch"),
    ("record-hash", "RECORD integrity mismatch"),
    ("record-size", "RECORD integrity mismatch"),
    ("record-duplicate", "duplicate RECORD member"),
    ("zip-duplicate", "duplicate wheel member"),
    ("record-phantom", "RECORD mismatch"),
    ("record-malformed", "malformed RECORD row"),
    ("record-self-hash", "RECORD must not hash itself"),
  ],
)
def test_wheel_record_and_archive_integrity_are_independent_requirements(verifier, fault, message):
  version = verifier.project_version()
  members = wheel_payload(version)
  with pytest.raises(AssertionError, match=message):
    check(verifier, members, version, fault)
  check(verifier, members, version)


@pytest.mark.parametrize("fault", [None, "missing", "extra", "duplicate"])
def test_wheel_entrypoint_reconciles_exports_before_inspection(verifier, tmp_path, monkeypatch, fault):
  manifest = json.loads(verifier.MANIFEST.read_text(encoding="utf-8"))
  if fault == "missing":
    manifest["exports"].pop()
  elif fault == "extra":
    manifest["exports"].append({"name": "unexpected_export"})
  elif fault == "duplicate":
    manifest["exports"].append(manifest["exports"][0])
  source = tmp_path / "manifest.json"
  source.write_text(json.dumps(manifest), encoding="utf-8")
  monkeypatch.setattr(verifier, "MANIFEST", source)
  version = verifier.project_version()
  wheel = tmp_path / f"celestial_calendar-{version}-py3-none-manylinux_2_28_x86_64.whl"
  wheel.touch()
  monkeypatch.setattr(sys, "argv", ["verify.py", str(wheel)])
  if fault is None:
    with pytest.raises(zipfile.BadZipFile):
      verifier.main()
  else:
    with pytest.raises(AssertionError, match="C ABI export inventory"):
      verifier.main()
