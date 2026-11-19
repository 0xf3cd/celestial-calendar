# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Exercise count/fill and absent-storage rules in the fixed-input C parity exporter."""

import ctypes
from types import SimpleNamespace

import pytest

from toolbox.make_position_events_golden import capture


class NodeRecord(ctypes.Structure):
  _fields_ = [
    ("present_fields", ctypes.c_uint32),
    ("distance_au", ctypes.c_double),
    ("longitude_deg", ctypes.c_double),
  ]

  def __getattribute__(self, name):
    if name == "distance_au":
      raise AssertionError("absent distance must not be read")
    return super().__getattribute__(name)


def exporter_fixture(*, error=0, queried=0, total=1, written=1, fill_total=1):
  calls = []

  def positions(codes, length, time, count, slots, capacity):
    calls.append((list(codes)[:length], time, capacity))
    count._obj.value = total if slots is None else fill_total
    if slots is None:
      return queried
    for record in slots:
      record.present_fields = 1
      record.longitude_deg = 123.5
    return written

  library = SimpleNamespace(celestial_positions=positions, celestial_last_error_kind=lambda: error)
  abi = SimpleNamespace(CelestialPositionRecord=NodeRecord, C_CODES={"CELESTIAL_PRESENT_DISTANCE": 2})
  case = {"operation": "position.ecliptic", "targets": [10], "jde_tt": 2451545.0}
  return library, abi, case, calls


def test_absent_distance_and_binary64():
  library, abi, case, calls = exporter_fixture()
  result = capture(library, abi, case)
  assert result == {
    "error": 0,
    "count": 1,
    "written": 1,
    "records": [{"present_fields": 1, "distance_au": None, "longitude_deg": "0x405ee00000000000"}],
  }
  assert calls == [([10], 2451545.0, 0), ([10], 2451545.0, 1)]


@pytest.mark.parametrize("changes", [{"queried": 1}, {"written": 0}, {"fill_total": 2}, {"error": 1, "total": 1}])
def test_malformed_count_fill(changes):
  library, abi, case, _ = exporter_fixture(**changes)
  with pytest.raises(RuntimeError):
    capture(library, abi, case)


@pytest.mark.parametrize("error", [0, 1, 2, 3, 4])
def test_empty_and_failed_count_pass(error):
  library, abi, case, calls = exporter_fixture(error=error, total=0)
  assert capture(library, abi, case) == {"error": error, "count": 0, "records": []}
  assert len(calls) == 1
