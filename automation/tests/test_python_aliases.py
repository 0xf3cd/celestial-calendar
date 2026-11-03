# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import base64
import csv
import hashlib
import io
import importlib.util
import tomllib
import zipfile

from pathlib import Path
from types import SimpleNamespace

import pytest

from toolbox import build_python_aliases as aliases


ROOT = Path(__file__).resolve().parents[2]


def alias_members(name, version):
  project = tomllib.loads((ROOT / "bindings/python/pyproject.toml").read_text())["project"]
  metadata = [
    "Metadata-Version: 2.4",
    f"Name: {name}",
    f"Version: {version}",
    f"Summary: {project['description']}",
    "Requires-Python: >=3.11",
    "License-Expression: MIT",
    "License-File: LICENSE",
    f"Requires-Dist: celestial-calendar=={version}",
    "Description-Content-Type: text/markdown",
    *(f"Classifier: {value}" for value in project["classifiers"]),
    *(f"Project-URL: {label}, {url}" for label, url in project["urls"].items()),
    "",
    aliases.alias_readme(name),
  ]
  info = f"{name}-{version}.dist-info"
  return {
    f"{name}/__init__.py": (ROOT / "bindings/python-aliases/__init__.py").read_bytes(),
    f"{name}/py.typed": b"",
    f"{info}/METADATA": "\n".join(metadata).encode(),
    f"{info}/WHEEL": b"Wheel-Version: 1.0\nRoot-Is-Purelib: true\nTag: py3-none-any\n",
    f"{info}/licenses/LICENSE": (ROOT / "LICENSE").read_bytes(),
  }


def alias_wheel_bytes(name, version, mutation=None):
  members = alias_members(name, version)
  if mutation is not None:
    mutation(members)
  record = f"{name}-{version}.dist-info/RECORD"
  rows = []
  for member, content in members.items():
    digest = base64.urlsafe_b64encode(hashlib.sha256(content).digest()).rstrip(b"=").decode()
    rows.append((member, f"sha256={digest}", str(len(content))))
  rows.append((record, "", ""))
  text = io.StringIO()
  csv.writer(text, lineterminator="\n").writerows(rows)
  members[record] = text.getvalue().encode()
  output = io.BytesIO()
  with zipfile.ZipFile(output, "w") as archive:
    for member, content in members.items():
      archive.writestr(member, content)
  return output.getvalue()


def write_alias_wheel(directory, name, version, mutation=None):
  wheel = directory / f"{name}-{version}-py3-none-any.whl"
  wheel.write_bytes(alias_wheel_bytes(name, version, mutation))
  sidecar = wheel.with_name(f"{wheel.name}.sha256")
  sidecar.write_text(f"{hashlib.sha256(wheel.read_bytes()).hexdigest()}  {wheel.name}\n")
  return wheel, sidecar


def test_exact_requested_names_and_pure_tags():
  assert aliases.ALIAS_NAMES == ("cele", "wentian")
  assert aliases.PYPI_PACKAGES == ("celestial-calendar", "cele", "wentian")
  assert aliases.ALIAS_ARTIFACT == "celestial-python-aliases"
  for name in ("cele", "wentian"):
    assert aliases.alias_wheel_name(name, "9.8.7") == f"{name}-9.8.7-py3-none-any.whl"
  with pytest.raises(ValueError):
    aliases.alias_wheel_name("celestial", "9.8.7")


@pytest.mark.parametrize("name", ["cele", "wentian"])
def test_staged_alias_version_dependency_and_backend_derive_together(name):
  actual = tomllib.loads(aliases.alias_configuration(name, "9.8.7"))
  primary = tomllib.loads((ROOT / "bindings/python/pyproject.toml").read_text())
  assert actual["build-system"] == primary["build-system"]
  assert actual["project"]["name"] == name
  assert actual["project"]["version"] == "9.8.7"
  assert actual["project"]["dependencies"] == ["celestial-calendar==9.8.7"]
  assert actual["project"]["license-files"] == ["LICENSE"]
  for field in ("urls", "authors", "requires-python", "classifiers", "keywords", "license", "description"):
    assert actual["project"][field] == primary["project"][field]
  assert actual["tool"]["scikit-build"] == {"wheel": {"cmake": False, "packages": [f"src/{name}"]}}
  assert "dynamic" not in actual["project"]


@pytest.mark.parametrize("name", ["cele", "wentian"])
@pytest.mark.parametrize(
  "mutation",
  ["none", "source", "marker", "license", "notice", "native", "dependency", "metadata", "tag", "readme"],
)
def test_alias_payload_contract_rejects_contamination_and_drift(tmp_path, name, mutation):
  version = "9.8.7"
  info = f"{name}-{version}.dist-info"

  def change(members):
    if mutation == "source":
      members[f"{name}/__init__.py"] += b"\nCANARY = True\n"
    elif mutation == "marker":
      del members[f"{name}/py.typed"]
    elif mutation == "license":
      members[f"{info}/licenses/LICENSE"] = b"wrong"
    elif mutation == "notice":
      members[f"{info}/licenses/THIRD_PARTY_NOTICES.txt"] = b"not alias content"
    elif mutation == "native":
      members[f"{name}/_native.so"] = b"not allowed"
    elif mutation == "dependency":
      members[f"{info}/METADATA"] = members[f"{info}/METADATA"].replace(b"==9.8.7", b">=9.8.7")
    elif mutation == "metadata":
      members[f"{info}/METADATA"] += b"\nName: another\n"
    elif mutation == "tag":
      members[f"{info}/WHEEL"] = members[f"{info}/WHEEL"].replace(b"any", b"win_amd64")
    elif mutation == "readme":
      members[f"{info}/METADATA"] += b"unexpected text\n"

  wheel, _ = write_alias_wheel(tmp_path, name, version, change)
  if mutation == "none":
    aliases.validate_alias_wheel(wheel, name, version)
  else:
    with pytest.raises(RuntimeError):
      aliases.validate_alias_wheel(wheel, name, version)


@pytest.mark.parametrize("name", ["cele", "wentian"])
def test_alias_requirement_allows_normalized_name_and_whitespace(tmp_path, name):
  def change(members):
    metadata = f"{name}-9.8.7.dist-info/METADATA"
    members[metadata] = members[metadata].replace(b"celestial-calendar==9.8.7", b"Celestial_Calendar == 9.8.7")

  wheel, _ = write_alias_wheel(tmp_path, name, "9.8.7", change)
  aliases.validate_alias_wheel(wheel, name, "9.8.7")


def test_alias_record_hash_duplicate_and_self_hash_are_rejected(tmp_path):
  wheel, _ = write_alias_wheel(tmp_path, "cele", "9.8.7")
  with zipfile.ZipFile(wheel) as archive:
    members = {name: archive.read(name) for name in archive.namelist()}
  record = "cele-9.8.7.dist-info/RECORD"
  for content in (
    b"broken,row\n",
    members[record] * 2,
    members[record].replace(b"sha256=", b"sha512="),
    members[record].replace(f"{record},,\n".encode(), f"{record},sha256=wrong,1\n".encode()),
  ):
    with zipfile.ZipFile(wheel, "w") as archive:
      for name, original in members.items():
        archive.writestr(name, content if name == record else original)
    with pytest.raises(RuntimeError, match="RECORD"):
      aliases.validate_alias_wheel(wheel, "cele", "9.8.7")


def test_builder_uses_one_version_and_builds_each_alias_once(monkeypatch, tmp_path):
  calls = []

  def run(command, **kwargs):
    calls.append(command)
    if command[-1] == "--version":
      return SimpleNamespace(stdout="9.8.7\n")
    assert "--wheel" in command and "--no-isolation" in command and "--sdist" not in command
    directory = Path(command[command.index("--outdir") + 1])
    source = Path(command[-1])
    project = tomllib.loads((source / "pyproject.toml").read_text())["project"]
    assert project["version"] == "9.8.7" and project["dependencies"] == ["celestial-calendar==9.8.7"]
    write_alias_wheel(directory, project["name"], project["version"])
    return SimpleNamespace(returncode=0)

  monkeypatch.setattr(aliases.subprocess, "run", run)
  output = tmp_path / "build"
  wheels = aliases.build(output)
  assert [p.name for p in wheels] == ["cele-9.8.7-py3-none-any.whl", "wentian-9.8.7-py3-none-any.whl"]
  assert len(calls) == 3
  assert len(list((output / "artifact").iterdir())) == 4
  with pytest.raises(FileExistsError):
    aliases.build(output)


@pytest.mark.parametrize("fail", [False, True])
def test_missing_native_control_probes_each_alias_and_restores_the_binary(monkeypatch, tmp_path, fail):
  spec = importlib.util.spec_from_file_location("alias_consumer", ROOT / "bindings/python-aliases/test/consumer.py")
  consumer = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(consumer)
  package = tmp_path / "celestial_calendar"
  native = package / "_native" / "_celestial_calendar.so"
  native.parent.mkdir(parents=True)
  native.write_bytes(b"native bytes")
  calls = []

  def run(command, cwd, success=True):
    assert not native.exists() and not success
    calls.append(command[-1])
    if fail:
      raise RuntimeError("probe interrupted")
    return "OSError: native implementation missing"

  monkeypatch.setattr(consumer, "run", run)
  if fail:
    with pytest.raises(RuntimeError, match="probe interrupted"):
      consumer.missing_native_contract(tmp_path / "python", package, tmp_path)
    assert calls == ["import cele"]
  else:
    consumer.missing_native_contract(tmp_path / "python", package, tmp_path)
    assert calls == ["import cele", "import wentian"]
  assert native.read_bytes() == b"native bytes"
  assert list(native.parent.iterdir()) == [native]
