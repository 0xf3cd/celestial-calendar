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
import math
import os
from pathlib import Path
import shlex
import shutil
import subprocess

import pytest


def test_published_chart_example_replays_native_facade(tmp_path):
  root = Path(__file__).resolve().parents[2]
  compiler = shlex.split(os.environ.get("CXX", ""))
  if not compiler:
    resolved = shutil.which("clang++-22") or shutil.which("clang++") or shutil.which("g++")
    if not resolved:
      pytest.skip("Native chart replay requires a C++23 compiler")
    compiler = [resolved]
  binary = tmp_path / "chart_snapshot_example"
  command = [
    *compiler,
    "-std=c++23",
    "-O2",
    "-Isrc/astro",
    "-Isrc/calendar",
    "-Isrc/util",
    "src/bench/chart_snapshot_fixture.cpp",
    "-o",
    str(binary),
  ]
  subprocess.run(command, cwd=root, check=True, capture_output=True, text=True)
  actual = json.loads(subprocess.check_output([str(binary)], cwd=root, text=True))
  expected = json.loads((root / "docs/examples/chart_snapshot/data.json").read_text(encoding="utf-8"))
  assert actual.keys() == expected.keys()
  assert actual["input"] == expected["input"]
  assert actual["role"] == expected["role"]
  assert actual["times"].keys() == expected["times"].keys()
  assert actual["houses"].keys() == expected["houses"].keys()
  assert len(actual["bodies"]) == len(expected["bodies"]) == 14
  assert len(actual["houses"]["cusps_deg"]) == len(expected["houses"]["cusps_deg"]) == 12

  # Replay margins cover floating-point wiring, not physical ephemeris accuracy.
  for key in ("jd_ut1", "jde_tt"):
    assert actual["times"][key] == pytest.approx(expected["times"][key], abs=1e-9, rel=0.0)
  for body, stored in zip(actual["bodies"], expected["bodies"], strict=True):
    assert body.keys() == stored.keys()
    assert body["target"] == stored["target"]
    for key in ("longitude_deg", "latitude_deg"):
      assert math.isfinite(body[key])
      assert body[key] == pytest.approx(stored[key], abs=1e-8, rel=0.0)
    assert body["longitude_rate_deg_per_tt_day"] == pytest.approx(
      stored["longitude_rate_deg_per_tt_day"], abs=1e-9, rel=0.0
    )
    if stored["distance_au"] is None:
      assert body["distance_au"] is None
    else:
      assert body["distance_au"] == pytest.approx(stored["distance_au"], abs=1e-10, rel=0.0)
  for key in ("ascendant_deg", "midheaven_deg", "descendant_deg", "imum_coeli_deg"):
    assert actual["houses"][key] == pytest.approx(expected["houses"][key], abs=1e-5, rel=0.0)
  assert actual["houses"]["cusps_deg"] == pytest.approx(expected["houses"]["cusps_deg"], abs=1e-5, rel=0.0)
