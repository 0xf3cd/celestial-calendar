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
import sys
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
      f"{dist_info}/METADATA": (
        f"Metadata-Version: 2.4\nName: celestial-calendar\nVersion: {version}\n"
        "Requires-Python: >=3.11\nLicense-Expression: MIT\n"
        "Project-URL: Repository, https://github.com/0xf3cd/celestial-calendar\n"
      ).encode(),
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
