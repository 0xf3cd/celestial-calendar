# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import ctypes
import shutil
import sys

from pathlib import Path
from types import SimpleNamespace

import pytest

from automation import ctypes_smoke


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize("defect", [None, "illumination", "elongation", "angle", "phase-order", "lunar-month"])
def test_actual_wrapper_parity_rejects_field_and_buffer_defects(tmp_path, monkeypatch, defect):
  source = (ROOT / "statistics/common.py").read_text()
  if defect:
    old, new = {
      "illumination": ("illumination=result.illumination", "illumination=result.elongation_deg"),
      "elongation": ("elongation_deg=result.elongation_deg", "elongation_deg=result.illumination"),
      "angle": ("MoonPositionAngle(angle_deg=result.angle_deg)", "MoonPositionAngle(angle_deg=0.0)"),
      "phase-order": ("jdes=jdes, moments=moments", "jdes=list(reversed(jdes)), moments=moments"),
      "lunar-month": (
        "month=result.month,\n    is_leap=result.is_leap",
        "month=result.day,\n    is_leap=result.is_leap",
      ),
    }[defect]
    assert source.count(old) == 1
    source = source.replace(old, new)
  (tmp_path / "statistics").mkdir()
  (tmp_path / "statistics/common.py").write_text(source)
  (tmp_path / "statistics/usno_data.txt").touch()
  abi = Path("bindings/python/src/celestial_calendar/_abi.py")
  (tmp_path / abi).parent.mkdir(parents=True)
  shutil.copyfile(ROOT / abi, tmp_path / abi)
  build = tmp_path / "build/shared_lib"
  build.mkdir(parents=True)
  extension = {"linux": ".so", "darwin": ".dylib", "win32": ".dll"}[sys.platform]
  (build / ("libcelestial_calendar" + extension)).touch()

  def raw(name, args):
    if name == "celestial_moon_phase_moments":
      year, phase, count, slots, capacity = args
      assert year == 2024
      count._obj.value = 3
      for index in range(min(capacity, 3)):
        slots[index] = 2451545.0 + phase + index * 0.2345
      return min(capacity, 3)
    if name == "celestial_gregorian_to_lunar":
      return SimpleNamespace(valid=args[2] != 13, year=2023, month=4, is_leap=args[2] == 3, day=12)
    if name == "celestial_lunar_to_gregorian":
      return SimpleNamespace(
        valid=args[1] == 2023 and args[2:5] in ((2, True, 1), (3, False, 1)), year=2025, month=4, day=12
      )
    if name == "celestial_get_supported_lunar_year_range":
      return SimpleNamespace(valid=True, start=1601, end=2198)
    if name == "celestial_moon_illumination":
      return SimpleNamespace(valid=True, illumination=0.2345, elongation_deg=112.2)
    if name == "celestial_moon_position_angle":
      return SimpleNamespace(valid=True, angle_deg=89.5)
    if name == "celestial_local_apparent_sidereal_time":
      return SimpleNamespace(valid=args[0] != 1000000.0, value=78.2)
    if name == "celestial_jde_to_ut1":
      return SimpleNamespace(valid=True, year=2024, month=1, day=1, fraction=0.5)
    raise AssertionError(f"unexpected raw call {name}")

  class Library:
    def __getattr__(self, name):
      def function(*args):
        return raw(name, args)

      setattr(self, name, function)
      return function

  monkeypatch.setattr(ctypes, "CDLL", lambda path: Library())
  monkeypatch.setattr(ctypes_smoke.paths, "proj_root", lambda: tmp_path)
  monkeypatch.setattr(ctypes_smoke, "check_runtime_layout", lambda *args, **kwargs: 0)
  assert ctypes_smoke.check_ctypes_smoke() == (1 if defect else 0)
