# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Complete altitude queries, raw C parity and malformed/allocation failure controls."""

import ctypes
import math
from dataclasses import FrozenInstanceError, replace
from unittest.mock import patch

import celestial_calendar as c
from celestial_calendar import _abi, _altitude, _binding
from protocol_seams import CountFill, bindings, raises


START, END = 2451545.0, 2451546.0
SITE = c.Observer(0.0, -77.0, 0.0)
CONDITION = c.AltitudeCondition(c.Target.SUN, c.GeometricLevel(0.0), c.AltitudeSense.ABOVE)


def raw_records(export, prefix, site=SITE, start=START, end=END):
  args = (*prefix, site.latitude_deg, site.longitude_deg, site.elevation_m, start, end)
  count = ctypes.c_uint32()
  raw = _binding.FUNCTIONS[export]
  assert raw(*args, ctypes.byref(count), None, 0) == 0 and _binding.last_error_kind() == 0
  records = (_abi.CelestialAltitudeRecord * count.value)()
  assert raw(*args, ctypes.byref(count), records, count.value) == count.value and _binding.last_error_kind() == 0
  return tuple(records)


def changed(records, index, **fields):
  result = [_abi.CelestialAltitudeRecord.from_buffer_copy(record) for record in records]
  for name, value in fields.items():
    setattr(result[index], name, value)
  return result


def fixture():
  return (
    _abi.CelestialAltitudeRecord(
      tag=0, target_code=0, position_source_code=1, delta_t_model_code=5, sense_code=1, initially_satisfied=1
    ),
    _abi.CelestialAltitudeRecord(tag=1, direction_code=1, jde_tt=START + 0.25),
    _abi.CelestialAltitudeRecord(tag=1, direction_code=0, jde_tt=START + 0.75),
  )


def cases():
  records = fixture()
  windows = (
    *records,
    _abi.CelestialAltitudeRecord(tag=2, jde_tt=START, end_jde_tt=START + 0.25),
    _abi.CelestialAltitudeRecord(tag=2, jde_tt=START + 0.75, end_jde_tt=END),
  )
  transit = (
    _abi.CelestialAltitudeRecord(
      tag=3, target_code=0, position_source_code=1, delta_t_model_code=5, jde_tt=START + 0.5
    ),
  )
  return (
    ("celestial_altitude_events", lambda: c.altitude_events(SITE, CONDITION, START, END), records),
    ("celestial_observing_windows", lambda: c.observing_windows(SITE, (CONDITION,), START, END), windows),
    ("celestial_upper_transits", lambda: c.upper_transits(SITE, c.Target.SUN, START, END), transit),
  )


def run_real():
  for target in tuple(c.Target)[:10]:
    condition = replace(CONDITION, target=target, level=c.GeometricLevel(10.0))
    site = replace(SITE, latitude_deg=90.0)
    result = c.altitude_events(site, condition, START, START + 0.25)
    encoded = _altitude.encode_condition(condition)
    records = raw_records("celestial_altitude_events", (ctypes.byref(encoded),), site, START, START + 0.25)
    state = result.condition
    assert state.condition == condition and type(state.initially_satisfied) is bool
    assert state.initially_satisfied == bool(records[0].initially_satisfied)
    assert state.source == c.ModelKey(c.ModelKind.POSITION_SOURCE, records[0].position_source_code)
    assert state.delta_t_model == c.ModelKey(c.ModelKind.DELTA_T, 5)
    assert state.geometric_level_deg == records[0].geometric_level_deg == 10.0
    assert [(e.jde_tt, e.condition_index) for e in result.events] == [
      (r.jde_tt, r.condition_index) for r in records[1:]
    ]
    raises(FrozenInstanceError, lambda state=state: setattr(state, "initially_satisfied", False))

  result = c.altitude_events(SITE, CONDITION, START, END)
  assert [e.direction for e in result.events] == [c.EventDirection.DECREASING, c.EventDirection.INCREASING]
  assert result.condition.initially_satisfied
  split = c.altitude_events(SITE, CONDITION, result.events[0].jde_tt, END)
  assert split.events[0] == result.events[0] and not split.condition.initially_satisfied
  conditions = (CONDITION, replace(CONDITION, level=c.GeometricLevel(-6.0)))
  windows = c.observing_windows(SITE, iter(conditions), START, END)
  assert len(windows.conditions) == 2 and len(windows.events) == 4 and len(windows.windows) == 2
  assert windows.conditions[0].condition == conditions[0] and windows.conditions[1].condition == conditions[1]
  opposite = replace(CONDITION, sense=c.AltitudeSense.BELOW)
  assert not c.observing_windows(SITE, (CONDITION, opposite), START, END).windows
  duplicate = c.observing_windows(SITE, (CONDITION, CONDITION), START, END)
  assert len(duplicate.events) == 4 and duplicate.windows == c.observing_windows(SITE, (CONDITION,), START, END).windows
  assert c.observing_windows(SITE, (), 13689325.25, 13689325.5).windows == (c.ObservingWindow(13689325.25, 13689325.5),)

  for model in c.RefractionModel:
    condition = replace(CONDITION, level=c.RefractedLevel(10.0, c.RefractionParams(model, 5.0, 900.0)))
    result = c.altitude_events(replace(SITE, latitude_deg=90.0), condition, START, START + 0.25)
    assert result.condition.condition == condition and 9.0 < result.condition.geometric_level_deg < 10.0
  transits = c.upper_transits(SITE, c.Target.MARS, START, END)
  records = raw_records("celestial_upper_transits", (4,))
  assert len(transits) == len(records) == 1
  assert transits[0].jde_tt == records[0].jde_tt and transits[0].target is c.Target.MARS
  assert transits[0].source == c.ModelKey(c.ModelKind.POSITION_SOURCE, 3)
  print("PASS altitude physical targets, raw C parity, poles, refraction, start replay and equal-time intersections")


def run_errors():
  unresolved = replace(CONDITION, target=c.Target.PLUTO, level=c.GeometricLevel(48.605252016244656))
  assert raises(
    c.CelestialError, lambda: c.altitude_events(replace(SITE, latitude_deg=30.0), unresolved, START, START + 0.25)
  ).recorded
  assert raises(
    c.CelestialError,
    lambda: c.observing_windows(
      replace(SITE, latitude_deg=30.0),
      (replace(CONDITION, level=c.GeometricLevel(-89.0)), unresolved),
      START,
      START + 0.25,
    ),
  ).recorded
  for target in tuple(c.Target)[10:]:
    raises(ValueError, lambda target=target: c.upper_transits(SITE, target, START, END))
  for condition in (
    replace(CONDITION, level=c.GeometricLevel(90.0)),
    replace(CONDITION, level=c.GeometricLevel(math.nan)),
    replace(CONDITION, level=c.RefractedLevel(10.0, c.RefractionParams(c.RefractionModel.BENNETT, -273.0))),
  ):
    raises(ValueError, lambda condition=condition: c.altitude_events(SITE, condition, START, END))
  outside_proof = replace(CONDITION, level=c.RefractedLevel(10.0, c.RefractionParams(c.RefractionModel.BENNETT, -91.0)))
  assert raises(c.CelestialError, lambda: c.altitude_events(SITE, outside_proof, START, END)).recorded
  for condition in (
    None,
    replace(CONDITION, target="sun"),
    replace(CONDITION, sense="above"),
    replace(CONDITION, level=0.0),
    replace(CONDITION, level=c.GeometricLevel(True)),
    replace(CONDITION, level=c.RefractedLevel(10.0, None)),
  ):
    raises(TypeError, lambda condition=condition: c.altitude_events(SITE, condition, START, END))
  raises(TypeError, lambda: c.altitude_events(None, CONDITION, START, END))
  raises(ValueError, lambda: c.upper_transits(SITE, c.Target.SUN, END, START))
  raises(ValueError, lambda: c.upper_transits(SITE, c.Target.PLUTO, 2488069.5, 2488069.75))
  for target in (c.Target.SUN, c.Target.MOON, c.Target.MARS, c.Target.JUPITER, c.Target.SATURN):
    condition = replace(CONDITION, target=target, level=c.GeometricLevel(10.0))
    result = c.altitude_events(replace(SITE, latitude_deg=90.0), condition, 2488070.125, 2488070.25)
    assert result.condition.condition.target is target and not result.events
  assert raises(
    c.CelestialError,
    lambda: c.observing_windows(
      replace(SITE, latitude_deg=90.0), (replace(CONDITION, level=c.GeometricLevel(10.0)),) * 2048, START, START + 0.25
    ),
  ).recorded
  consumed = []

  def unbounded():
    while True:
      consumed.append(1)
      yield CONDITION

  error = raises(c.CelestialError, lambda: c.observing_windows(SITE, unbounded(), START, END))
  assert not error.recorded and len(consumed) == 2049
  print("PASS altitude whole-query numerical failures, invalid inputs, provider domain and bounded iterable admission")


def run_protocol():
  for export, action, records in cases():
    with bindings(**{export: CountFill(records), "celestial_last_error_kind": lambda: 0}):
      assert action() is not None
    for fake in (
      CountFill(records, count_written=1),
      CountFill(records, fill_total=len(records) + 1),
      CountFill(records, written=len(records) - 1),
      CountFill(records, count=1000000),
    ):
      with bindings(**{export: fake, "celestial_last_error_kind": lambda: 0}):
        assert not raises(c.CelestialError, action).recorded
    for phase in (1, 2):
      for kind in (2, 3, 4):
        fake = CountFill(records)
        with bindings(
          **{
            export: fake,
            "celestial_last_error_kind": lambda kind=kind, fake=fake, phase=phase: (
              kind if len(fake.calls) == phase else 0
            ),
            "celestial_last_error": lambda: b"",
          }
        ):
          assert raises(c.CelestialError, action).recorded and len(fake.calls) == phase
    for index, record in enumerate(records):
      for name, _ in record._fields_:
        if name == "tag":
          mutations = (99,)
        elif name in (
          "reserved",
          "target_code",
          "position_source_code",
          "delta_t_model_code",
          "refraction_code",
          "sense_code",
          "condition_index",
          "direction_code",
          "initially_satisfied",
        ):
          mutations = (99,)
        else:
          mutations = (math.nan, math.inf)
        for value in mutations:
          fake = CountFill(changed(records, index, **{name: value}))
          with bindings(**{export: fake, "celestial_last_error_kind": lambda: 0}):
            assert not raises(c.CelestialError, action).recorded, (export, index, name, value)
  for export, action, _ in cases():
    with bindings(**{export: CountFill(()), "celestial_last_error_kind": lambda: 0}):
      if export == "celestial_upper_transits":
        assert action() == ()
      else:
        assert not raises(c.CelestialError, action).recorded
  export, action, records = cases()[1]
  for corrupt in (
    records[:-1],
    changed(records, 3, jde_tt=START + 0.1),
    changed(records, 1, jde_tt=END),
    changed(records, 2, jde_tt=records[1].jde_tt),
    (records[1], records[0], *records[2:]),
  ):
    with bindings(**{export: CountFill(corrupt), "celestial_last_error_kind": lambda: 0}):
      assert not raises(c.CelestialError, action).recorded
  tangent = _abi.CelestialAltitudeRecord(tag=1, direction_code=2, jde_tt=START + 0.5)
  stream = (fixture()[0], tangent, _abi.CelestialAltitudeRecord(tag=2, jde_tt=START, end_jde_tt=END))
  with bindings(celestial_observing_windows=CountFill(stream), celestial_last_error_kind=lambda: 0):
    result = c.observing_windows(SITE, (CONDITION,), START, END)
    assert result.events[0].direction is c.EventDirection.TANGENT and len(result.windows) == 1
  print("PASS altitude count/fill disagreement, native failures at either pass and every record-field corruption")


def run_allocations():
  for export, action, records in cases():

    class BrokenRecord(type(ctypes.Structure)):
      def __mul__(self, count):
        raise MemoryError("injected output allocation")

    class NoStorage(ctypes.Structure, metaclass=BrokenRecord):
      _fields_ = _abi.CelestialAltitudeRecord._fields_

    fake = CountFill(records)
    with bindings(**{export: fake, "celestial_last_error_kind": lambda: 0}):
      with patch.object(_abi, "CelestialAltitudeRecord", NoStorage):
        assert not raises(c.CelestialError, action).recorded and len(fake.calls) == 1
      name = "UpperTransit" if export == "celestial_upper_transits" else "AltitudeConditionState"
      with patch.object(_altitude, name, side_effect=MemoryError("injected decoded allocation")):
        assert not raises(c.CelestialError, action).recorded
  with patch.object(_altitude, "encode_condition", side_effect=MemoryError("injected input allocation")):
    assert not raises(c.CelestialError, lambda: c.altitude_events(SITE, CONDITION, START, END)).recorded
    assert not raises(c.CelestialError, lambda: c.observing_windows(SITE, (CONDITION,), START, END)).recorded
  print("PASS altitude input/output/owned-result allocation failures return no partial result")


def main():
  run_real()
  run_errors()
  run_protocol()
  run_allocations()


if __name__ == "__main__":
  main()
