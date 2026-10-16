# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import json
import copy
import math
import os
from pathlib import Path
import shlex
import shutil
import subprocess

import pytest

from bindings.python.test.consumer.chart_replay import assert_chart_replay


ROOT = Path(__file__).resolve().parents[2]
EXAMPLE = json.loads((ROOT / "docs/examples/chart_snapshot/data.json").read_text(encoding="utf-8"))
NUMERIC_FIELDS = [
  *(("times", name) for name in EXAMPLE["times"]),
  *(
    ("bodies", index, name)
    for index, body in enumerate(EXAMPLE["bodies"])
    for name, value in body.items()
    if type(value) in (float, int)
  ),
  *(("houses", name) for name, value in EXAMPLE["houses"].items() if type(value) in (float, int)),
  *(("houses", "cusps_deg", index) for index in range(len(EXAMPLE["houses"]["cusps_deg"]))),
]


@pytest.mark.parametrize("path", NUMERIC_FIELDS)
def test_replay_rejects_each_changed_numeric_field(path):
  actual = copy.deepcopy(EXAMPLE)
  owner = actual
  for key in path[:-1]:
    owner = owner[key]
  owner[path[-1]] += 1.0
  with pytest.raises(AssertionError, match="replay residual"):
    assert_chart_replay(actual, EXAMPLE)


@pytest.mark.parametrize("value", [math.nan, math.inf, -math.inf])
def test_replay_rejects_nonfinite_fields(value):
  actual = copy.deepcopy(EXAMPLE)
  actual["bodies"][2]["longitude_deg"] = value
  with pytest.raises(AssertionError, match="non-finite replay field"):
    assert_chart_replay(actual, EXAMPLE)


def test_replay_preserves_presence_identity_and_cardinality():
  mutations = (
    lambda value: value["bodies"].pop(),
    lambda value: value["houses"]["cusps_deg"].pop(),
    lambda value: value["bodies"][2].update(target="VENUS"),
    lambda value: value["bodies"][2].update(distance_au=None),
    lambda value: value["bodies"][10].update(distance_au=1.0),
    lambda value: value["times"].update(extra=0.0),
  )
  for mutate in mutations:
    actual = copy.deepcopy(EXAMPLE)
    mutate(actual)
    with pytest.raises(AssertionError):
      assert_chart_replay(actual, EXAMPLE)


def test_replay_angles_cross_zero_without_a_large_residual():
  actual, expected = copy.deepcopy(EXAMPLE), copy.deepcopy(EXAMPLE)
  actual["bodies"][2]["longitude_deg"] = 0.0
  expected["bodies"][2]["longitude_deg"] = 359.9999999999
  actual["houses"]["cusps_deg"][6] = 0.0
  expected["houses"]["cusps_deg"][6] = 359.9999999999
  assert_chart_replay(actual, expected)


def test_native_replay_requires_a_compiler(monkeypatch, tmp_path):
  monkeypatch.delenv("CXX", raising=False)
  monkeypatch.setattr(shutil, "which", lambda name: None)
  with pytest.raises(pytest.fail.Exception, match=r"requires a C\+\+23 compiler"):
    test_published_chart_example_replays_native_facade(tmp_path)


def test_published_chart_example_replays_native_facade(tmp_path):
  root = Path(__file__).resolve().parents[2]
  compiler = shlex.split(os.environ.get("CXX", ""))
  if not compiler:
    resolved = shutil.which("clang++-22") or shutil.which("clang++") or shutil.which("g++")
    if not resolved:
      pytest.fail("Native chart replay requires a C++23 compiler")
    compiler = [resolved]
  binary = tmp_path / "chart_snapshot_example"
  command = [
    *compiler,
    "-std=c++23",
    "-O2",
    "-Isrc/astro",
    "-Isrc/calendar",
    "-Isrc/util",
    "docs/examples/chart_snapshot/chart_snapshot_fixture.cpp",
    "-o",
    str(binary),
  ]
  subprocess.run(command, cwd=root, check=True, capture_output=True, text=True)
  actual = json.loads(subprocess.check_output([str(binary)], cwd=root, text=True))
  expected = json.loads((root / "docs/examples/chart_snapshot/data.json").read_text(encoding="utf-8"))
  assert actual.keys() == expected.keys()
  assert actual["input"] == expected["input"]
  assert actual["role"] == expected["role"]
  assert_chart_replay(actual, expected)
