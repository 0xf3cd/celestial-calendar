# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Installed chart consumer: ingress guards, owned values and core-aligned replays."""

from __future__ import annotations

import datetime
import json
import math
from dataclasses import FrozenInstanceError, fields, replace
from enum import Enum, IntFlag
from operator import setitem
from pathlib import Path
from types import SimpleNamespace

import celestial_calendar as celestial
from celestial_calendar import _binding
from celestial_calendar import chart as chart_values
from smoke import Trap, raises, replaced_binding


REPO = Path(__file__).resolve().parents[4]
EXAMPLE = REPO / "docs/examples/chart_snapshot/data.json"
CIVIL = celestial.CivilDateTime(2026, 1, 1, 0.5)
LOCATION = celestial.GeoLocation(51.5, 0.0)


def snapshot(**changes: object) -> celestial.ChartSnapshot:
  """Use the documented example's input, with named changes for each scenario."""
  arguments = {
    "civil_dt": CIVIL,
    "scale": celestial.CivilScale.UTC,
    "location": LOCATION,
    "system": celestial.HouseSystem.PLACIDUS,
  }
  arguments.update(changes)
  return celestial.chart_snapshot(**arguments)


def native_fixture() -> _binding.ChartSnapshotV1:
  """Encode the documented replay as a decoding fixture, not a physical oracle."""
  example = json.loads(EXAMPLE.read_text(encoding="utf-8"))
  result = _binding.ChartSnapshotV1()
  result.valid = True
  result.jd_ut1 = example["times"]["jd_ut1"]
  result.jde_tt = example["times"]["jde_tt"]
  for native, body in zip(result.bodies, example["bodies"], strict=True):
    native.target_code = celestial.ChartTarget[body["target"]].value
    native.present_fields = 1 if body["distance_au"] is None else 3
    for name in ("longitude_deg", "latitude_deg", "longitude_rate_deg_per_tt_day"):
      setattr(native, name, body[name])
    native.distance_au = math.nan if body["distance_au"] is None else body["distance_au"]
  for name, value in example["houses"].items():
    if name == "cusps_deg":
      result.houses.cusps_deg[:] = value
    else:
      setattr(result.houses, name, value)
  return result


def check_snapshot(value: celestial.ChartSnapshot) -> None:
  """Check fixed cardinality, identity, units and absence through the public values."""
  assert type(value) is celestial.ChartSnapshot
  assert type(value.times) is celestial.ChartTimes and type(value.houses) is celestial.ChartHouses
  assert type(value.bodies) is tuple and type(value.houses.cusps_deg) is tuple
  assert len(value.bodies) == 14 and len(value.houses.cusps_deg) == 12
  assert tuple(body.target for body in value.bodies) == tuple(celestial.ChartTarget)
  assert all(math.isfinite(time) for time in (value.times.jd_ut1, value.times.jde_tt))
  for index, body in enumerate(value.bodies):
    assert type(body) is celestial.BodyState
    assert 0.0 <= body.longitude_deg < 360.0
    assert math.isfinite(body.latitude_deg) and math.isfinite(body.longitude_rate_deg_per_tt_day)
    if index < 10:
      assert body.distance_au is not None and body.distance_au > 0.0
    else:
      assert body.latitude_deg == 0.0 and body.distance_au is None
    assert "retrograde" not in {field.name for field in fields(body)}
  houses = value.houses
  assert all(
    0.0 <= angle < 360.0
    for angle in (
      houses.ascendant_deg,
      houses.midheaven_deg,
      houses.descendant_deg,
      houses.imum_coeli_deg,
      *houses.cusps_deg,
    )
  )
  for ascending, descending in ((10, 11), (12, 13)):
    assert (
      value.bodies[ascending].longitude_rate_deg_per_tt_day == value.bodies[descending].longitude_rate_deg_per_tt_day
    )


def run_validation_guards() -> None:
  """Every rejected raw input leaves the snapshot binding's call count at zero."""
  cases = []
  for name, values in (
    (
      "civil_dt",
      (
        None,
        {},
        datetime.date(2026, 1, 1),
        datetime.datetime(2026, 1, 1),
        datetime.datetime(2026, 1, 1, tzinfo=datetime.timezone.utc),
      ),
    ),
    ("location", (None, {}, (51.5, 0.0))),
    ("scale", (None, 0, True, "UTC", celestial.HouseSystem.EQUAL)),
    ("system", (None, 0, True, "PLACIDUS", celestial.CivilScale.UTC)),
    ("model", (None, 0, True, "algo5", celestial.LunarAlgorithm.ALGO1)),
  ):
    cases.extend(({name: value}, TypeError) for value in values)
  for name, enum_type in (("scale", celestial.CivilScale), ("system", celestial.HouseSystem)):
    unknown = int.__new__(enum_type, 99)
    unknown._value_ = 99
    unknown._name_ = "UNKNOWN"
    cases.append(({name: unknown}, ValueError))
    lookalike = int.__new__(enum_type, 0)
    lookalike._value_ = 2**32 + 1
    lookalike._name_ = "LOOKALIKE"
    cases.append(({name: lookalike}, ValueError))
  unknown_model = str.__new__(celestial.DeltaTModel, "unknown")
  unknown_model._value_ = "unknown"
  unknown_model._name_ = "UNKNOWN"
  cases.append(({"model": unknown_model}, ValueError))
  lookalike_model = str.__new__(celestial.DeltaTModel, "algo5")
  lookalike_model._value_ = "algo5"
  lookalike_model._name_ = "LOOKALIKE"
  cases.append(({"model": lookalike_model}, ValueError))

  class NumericFlag(IntFlag):
    ONE = 1

  class NumericFraction(float, Enum):
    HALF = 0.5

  class NumericLatitude(float, Enum):
    LONDON = 51.5

  class LyingInt(int):
    def __lt__(self, other):
      return False

    def __gt__(self, other):
      return False

    def __int__(self):
      return 6

  for name, value in (("year", 2026), ("month", 1), ("day", 1)):
    cases.append(({"civil_dt": replace(CIVIL, **{name: LyingInt(value + 2**32)})}, ValueError))
    cases.append(({"civil_dt": replace(CIVIL, **{name: LyingInt(value - 2**32)})}, ValueError))
  for year in (2100, 1971):
    cases.append(({"civil_dt": replace(CIVIL, year=LyingInt(year))}, ValueError))

  for name, value in (("year", 2026), ("month", 1), ("day", 1), ("fraction", 0)):
    cases.append(({"civil_dt": replace(CIVIL, **{name: NumericFlag(value)})}, TypeError))
  cases.append(({"civil_dt": replace(CIVIL, fraction=NumericFraction.HALF)}, TypeError))
  cases.append(({"location": celestial.GeoLocation(NumericFlag(51), 0.0)}, TypeError))
  cases.append(({"location": celestial.GeoLocation(51.5, NumericFlag(0))}, TypeError))
  cases.append(({"location": celestial.GeoLocation(NumericLatitude.LONDON, 0.0)}, TypeError))
  for name in ("year", "month", "day"):
    cases.extend(
      ({"civil_dt": replace(CIVIL, **{name: value})}, TypeError)
      for value in (True, 1.0, "1", None, celestial.Jieqi.YUSHUI)
    )
  for year, month, day in (
    (1884, 1, 1),
    (2100, 1, 1),
    (2026 + 2**32, 1, 1),
    (10**1000, 1, 1),
    (2026, 0, 1),
    (2026, 13, 1),
    (2026, 1 + 2**32, 1),
    (2026, 1, 0),
    (2026, 1, 32),
    (2026, 1, 1 + 2**32),
    (1900, 2, 29),
    (2023, 2, 29),
    (2026, 4, 31),
  ):
    cases.append(
      ({"civil_dt": celestial.CivilDateTime(year, month, day, 0.5), "scale": celestial.CivilScale.UT1}, ValueError)
    )
  cases.append(({"civil_dt": celestial.CivilDateTime(1971, 12, 31, 0.999999)}, ValueError))
  for fraction in (True, "0.5", None, celestial.CivilScale.UTC):
    cases.append(({"civil_dt": replace(CIVIL, fraction=fraction)}, TypeError))
  for fraction in (-0.01, 1.0, math.nan, math.inf, -math.inf, 10**1000):
    cases.append(({"civil_dt": replace(CIVIL, fraction=fraction)}, ValueError))
  for name in ("latitude_deg", "longitude_deg"):
    for value in (True, "0", None, celestial.HouseSystem.EQUAL):
      cases.append(({"location": replace(LOCATION, **{name: value})}, TypeError))
    for value in (math.nan, math.inf, -math.inf, 10**1000, -(10**1000)):
      cases.append(({"location": replace(LOCATION, **{name: value})}, ValueError))
  for latitude in (-90.0, 90.0, -90.0001, 90.0001):
    cases.append(({"location": celestial.GeoLocation(latitude, 0.0)}, ValueError))
  for longitude in (-180.0001, 180.0001):
    cases.append(({"location": celestial.GeoLocation(0.0, longitude)}, ValueError))

  trap = Trap(fail=True)
  with replaced_binding("chart_snapshot_v1", trap):
    for changes, error_type in cases:
      raises(error_type, lambda changes=changes: snapshot(**changes))
  assert trap.calls == 0
  print(f"PASS chart raw-input guards {len(cases)}/{len(cases)}; snapshot calls=0")


def run_single_read_inputs() -> None:
  class ShiftingCivil(celestial.CivilDateTime):
    def __getattribute__(self, name):
      if name in ("year", "month", "day", "fraction"):
        counts = object.__getattribute__(self, "_reads")
        counts[name] = counts.get(name, 0) + 1
        if counts[name] == 1:
          return {"year": 2026, "month": 1, "day": 1, "fraction": 0.5}[name]
      return object.__getattribute__(self, name)

  class ShiftingLocation(celestial.GeoLocation):
    def __getattribute__(self, name):
      if name in ("latitude_deg", "longitude_deg"):
        counts = object.__getattribute__(self, "_reads")
        counts[name] = counts.get(name, 0) + 1
        if counts[name] == 1:
          return {"latitude_deg": 51.5, "longitude_deg": 0.0}[name]
      return object.__getattribute__(self, name)

  civil = ShiftingCivil(1971, 13, 32, math.nan)
  location = ShiftingLocation(math.nan, math.nan)
  object.__setattr__(civil, "_reads", {})
  object.__setattr__(location, "_reads", {})
  trap = Trap(native_fixture())
  with replaced_binding("chart_snapshot_v1", trap):
    snapshot(civil_dt=civil, location=location)
  assert trap.calls == 1 and trap.args[:4] == (2026, 1, 1, 0.5)
  assert trap.args[5:7] == (51.5, 0.0)
  assert civil._reads == {"year": 1, "month": 1, "day": 1, "fraction": 1}
  assert location._reads == {"latitude_deg": 1, "longitude_deg": 1}
  print("PASS chart validates and dispatches the same single-read civil/location values")


def run_owned_values() -> None:
  """One scalar call copies all payload fields without retaining a native buffer."""
  native = native_fixture()
  trap = Trap(native)
  with replaced_binding("chart_snapshot_v1", trap):
    value = snapshot()
    assert trap.calls == 1 and trap.args == (2026, 1, 1, 0.5, 0, 51.5, 0.0, 2, 0)
    for model, code in zip(celestial.DeltaTModel, (0, 1, 2, 3, 4, 5), strict=True):
      snapshot(model=model)
      assert trap.args[-1] == code
    for scale, code in zip(celestial.CivilScale, (0, 1), strict=True):
      snapshot(scale=scale)
      assert trap.args[4] == code
    for system, code in zip(celestial.HouseSystem, (0, 1, 2), strict=True):
      snapshot(system=system)
      assert trap.args[7] == code
    for latitude in (math.nextafter(-90.0, 0.0), math.nextafter(90.0, 0.0)):
      for longitude in (-180.0, 180.0):
        snapshot(location=celestial.GeoLocation(latitude, longitude))
        assert trap.args[5:7] == (latitude, longitude)
    for civil, scale in (
      (celestial.CivilDateTime(1885, 1, 1, 0.0), celestial.CivilScale.UT1),
      (celestial.CivilDateTime(1972, 1, 1, 0.0), celestial.CivilScale.UTC),
      (celestial.CivilDateTime(2099, 12, 31, math.nextafter(1.0, 0.0)), celestial.CivilScale.UT1),
      (celestial.CivilDateTime(2000, 2, 29, 0.0), celestial.CivilScale.UT1),
    ):
      snapshot(civil_dt=civil, scale=scale)
      assert trap.args[:4] == (civil.year, civil.month, civil.day, civil.fraction)
  check_snapshot(value)

  example = json.loads(EXAMPLE.read_text(encoding="utf-8"))
  assert value.times == celestial.ChartTimes(**example["times"])
  assert value.houses == celestial.ChartHouses(
    **{**example["houses"], "cusps_deg": tuple(example["houses"]["cusps_deg"])}
  )
  for body, expected in zip(value.bodies, example["bodies"], strict=True):
    assert body == celestial.BodyState(**{**expected, "target": celestial.ChartTarget[expected["target"]]})
  for name in (
    "CivilScale",
    "ChartTarget",
    "HouseSystem",
    "GeoLocation",
    "ChartTimes",
    "BodyState",
    "ChartHouses",
    "ChartSnapshot",
  ):
    assert getattr(celestial, name) is getattr(chart_values, name)
    assert getattr(celestial, name).__module__ == "celestial_calendar.chart"
  for record in (LOCATION, value, value.times, value.houses, *value.bodies):
    field_name = fields(record)[0].name
    raises(FrozenInstanceError, lambda record=record, field_name=field_name: setattr(record, field_name, None))
  raises(TypeError, lambda: setitem(value.bodies, 0, value.bodies[0]))
  raises(TypeError, lambda: setitem(value.houses.cusps_deg, 0, 0.0))
  raises(AttributeError, lambda: value.bodies.append(value.bodies[0]))
  raises(AttributeError, lambda: value.houses.cusps_deg.append(0.0))
  before = hash(value)
  native.jd_ut1 = 1.0
  native.bodies[0].longitude_deg = 1.0
  native.houses.cusps_deg[0] = 1.0
  assert hash(value) == before and value.times.jd_ut1 == example["times"]["jd_ut1"]
  assert value.bodies[0].longitude_deg == example["bodies"][0]["longitude_deg"]
  assert value.houses.cusps_deg[0] == example["houses"]["cusps_deg"][0]
  print("PASS chart selector order, copied fields, frozen records and deeply immutable results")


def run_error_translation() -> None:
  """Invalid payloads are unread; malformed successes are protocol errors, not bad inputs."""
  for detail in (b"core joint domain rejected", b"", None):
    native = Trap(SimpleNamespace(valid=False))
    reader = Trap(detail)
    with replaced_binding("chart_snapshot_v1", native), replaced_binding("last_error", reader):
      error = raises(celestial.CelestialError, snapshot)
    assert error.operation == "chart_snapshot" and error.recorded is bool(detail)
    assert str(error) == (detail.decode() if detail else "chart_snapshot failed")
    assert native.calls == reader.calls == 1

  malformed = [SimpleNamespace(valid=True), SimpleNamespace()]
  for path, bad in (
    (("jd_ut1",), math.nan),
    (("jde_tt",), math.inf),
    (("bodies", 0, "target_code"), 99),
    (("bodies", 1, "target_code"), 0),
    (("bodies", 0, "present_fields"), 7),
    (("bodies", 0, "present_fields"), 1),
    (("bodies", 10, "present_fields"), 3),
    (("bodies", 10, "latitude_deg"), 1.0),
    (("bodies", 0, "latitude_deg"), math.nan),
    (("bodies", 0, "longitude_deg"), 360.0),
    (("bodies", 0, "distance_au"), 0.0),
    (("bodies", 0, "distance_au"), math.inf),
    (("bodies", 0, "longitude_rate_deg_per_tt_day"), math.nan),
    (("houses", "ascendant_deg"), -1.0),
    (("houses", "cusps_deg", 11), math.inf),
  ):
    native = native_fixture()
    owner = native
    for component in path[:-1]:
      owner = owner[component] if isinstance(component, int) else getattr(owner, component)
    if isinstance(path[-1], int):
      owner[path[-1]] = bad
    else:
      setattr(owner, path[-1], bad)
    malformed.append(native)
  fixture = native_fixture()
  malformed.extend(
    (
      SimpleNamespace(valid=True, bodies=tuple(fixture.bodies)[:-1], houses=fixture.houses),
      SimpleNamespace(valid=True, bodies=fixture.bodies, houses=SimpleNamespace(cusps_deg=(0.0,) * 11)),
    )
  )
  reader = Trap(fail=True)
  with replaced_binding("last_error", reader):
    for payload in malformed:
      native = Trap(payload)
      with replaced_binding("chart_snapshot_v1", native):
        error = raises(celestial.CelestialError, snapshot)
      assert error.operation == "chart_snapshot" and not error.recorded
      assert "Malformed native chart snapshot" in str(error) and native.calls == 1
  assert reader.calls == 0
  print(f"PASS chart native failure/empty text and malformed-output translation {len(malformed)}/{len(malformed)}")


def check_legacy_positions(value: celestial.ChartSnapshot) -> None:
  """Replay the unchanged Sun/Moon APIs at the snapshot's core-produced TT."""
  sun = celestial.sun_apparent_geocentric_coordinate(value.times.jde_tt)
  moon = celestial.moon_apparent_geocentric_coordinate(value.times.jde_tt)
  assert value.bodies[0].longitude_deg == sun.longitude_deg
  assert value.bodies[0].latitude_deg == sun.latitude_deg and value.bodies[0].distance_au == sun.radius_au
  assert value.bodies[1].longitude_deg == moon.longitude_deg and value.bodies[1].latitude_deg == moon.latitude_deg


def run_documented_replay() -> None:
  """Run the documented civil example through the installed binary and report replay residuals."""
  example = json.loads(EXAMPLE.read_text(encoding="utf-8"))
  inputs = example["input"]
  assert "not an independent physical-accuracy oracle" in example["role"]
  civil_text = datetime.datetime.fromisoformat(inputs["datetime"])
  assert civil_text.microsecond == 0
  civil = celestial.CivilDateTime(
    civil_text.year,
    civil_text.month,
    civil_text.day,
    (civil_text.hour * 3600 + civil_text.minute * 60 + civil_text.second) / 86400,
  )
  location = celestial.GeoLocation(inputs["latitude_north_deg"], inputs["longitude_east_deg"])
  value = snapshot(
    civil_dt=civil,
    scale=celestial.CivilScale[inputs["scale"]],
    location=location,
    system=celestial.HouseSystem[inputs["system"]],
    model=celestial.DeltaTModel[inputs["delta_t_model"]],
  )
  assert value == snapshot() == snapshot(model=celestial.DeltaTModel.DEFAULT)
  check_snapshot(value)
  check_legacy_positions(value)
  for index, expected in enumerate(example["bodies"]):
    assert value.bodies[index].target.name == expected["target"]
    assert (value.bodies[index].distance_au is None) == (expected["distance_au"] is None)
  # Report example drift without inventing a cross-platform physical or replay tolerance.
  differences = [abs(getattr(value.times, name) - expected) for name, expected in example["times"].items()]
  print(f"REPLAY chart example time max_abs_days={max(differences):.17g}")
  for name in ("longitude_deg", "latitude_deg", "distance_au", "longitude_rate_deg_per_tt_day"):
    differences = [
      abs(getattr(body, name) - expected[name])
      for body, expected in zip(value.bodies, example["bodies"], strict=True)
      if expected[name] is not None
    ]
    print(f"REPLAY chart example {name} max_abs={max(differences):.17g}")
  print("PASS installed chart documented example, DEFAULT/ALGO5 and unchanged Sun/Moon composition")


def run_core_aligned_cases() -> None:
  """Use chart_test.cpp's historical, southern, wrap/station and domain scenarios."""
  historical = celestial.CivilDateTime(1900, 6, 1, 0.5)
  for model in celestial.DeltaTModel:
    value = snapshot(
      civil_dt=historical,
      scale=celestial.CivilScale.UT1,
      system=celestial.HouseSystem.EQUAL,
      model=model,
    )
    check_snapshot(value)
    assert value.times.jd_ut1 == celestial.ut1_to_jd(historical)
    if model in (celestial.DeltaTModel.DEFAULT, celestial.DeltaTModel.ALGO5):
      assert value.times.jde_tt == celestial.ut1_to_jde(historical)
    check_legacy_positions(value)

  south = celestial.GeoLocation(-33.87, 151.21)
  for model in celestial.DeltaTModel:
    for system in celestial.HouseSystem:
      north_value = snapshot(model=model, system=system)
      south_value = snapshot(model=model, system=system, location=south)
      check_snapshot(south_value)
      assert north_value.times == south_value.times and north_value.bodies == south_value.bodies
      assert north_value.houses.ascendant_deg != south_value.houses.ascendant_deg

  west = snapshot(location=celestial.GeoLocation(0.0, -1.0), system=celestial.HouseSystem.EQUAL)
  east = snapshot(location=celestial.GeoLocation(0.0, 1.0), system=celestial.HouseSystem.EQUAL)
  assert 0.0 < math.remainder(east.houses.midheaven_deg - west.houses.midheaven_deg, 360.0) < 180.0

  # Same epochs as Chart.WrapsStationsAndNodeAntipodesReplayUnderlyingApis, converted by native helpers.
  for target, jde_tt in (
    (celestial.ChartTarget.SUN, 2460754.876816418),
    (celestial.ChartTarget.MOON, 2460681.293070456),
  ):
    before, after = (
      snapshot(civil_dt=celestial.jde_to_ut1(jde_tt + offset), scale=celestial.CivilScale.UT1, location=south)
      for offset in (-0.02, 0.02)
    )
    assert before.bodies[target.value].longitude_deg > after.bodies[target.value].longitude_deg
    assert before.bodies[target.value].longitude_rate_deg_per_tt_day > 0.0
    assert after.bodies[target.value].longitude_rate_deg_per_tt_day > 0.0
    check_legacy_positions(before)
    check_legacy_positions(after)
  for jde_tt, before_sign, after_sign in ((2460749.7828680002, 1, -1), (2460772.964472, -1, 1)):
    values = [
      snapshot(civil_dt=celestial.jde_to_ut1(jde_tt + offset), scale=celestial.CivilScale.UT1, location=south)
      for offset in (-0.25, 0.0, 0.25)
    ]
    assert values[0].bodies[2].longitude_rate_deg_per_tt_day * before_sign > 0.0
    assert values[2].bodies[2].longitude_rate_deg_per_tt_day * after_sign > 0.0
    # Ephemeris.IndependentStationNeighborhoods uses this same center bound, not a station Boolean.
    assert abs(values[1].bodies[2].longitude_rate_deg_per_tt_day) < 3e-5
  for offset, sign in ((-0.01, -1), (0.01, 1)):
    value = snapshot(civil_dt=celestial.jde_to_ut1(2460681.5899983719 + offset), scale=celestial.CivilScale.UT1)
    assert value.bodies[12].longitude_rate_deg_per_tt_day * sign > 0.0
    assert value.bodies[12].longitude_rate_deg_per_tt_day == value.bodies[13].longitude_rate_deg_per_tt_day

  failures = [
    {"civil_dt": celestial.CivilDateTime(1885, 1, 1, 0.0), "scale": celestial.CivilScale.UT1},
    {"civil_dt": celestial.CivilDateTime(2099, 12, 31, 0.999999), "scale": celestial.CivilScale.UT1},
    {"civil_dt": celestial.CivilDateTime(2035, 6, 1, 0.5), "model": celestial.DeltaTModel.ALGO4},
    {"civil_dt": celestial.CivilDateTime(2034, 12, 31, 0.99919925925925923), "model": celestial.DeltaTModel.ALGO4},
  ]
  for latitude in (-70.0, 70.0):
    failures.append({"location": celestial.GeoLocation(latitude, 0.0)})
    for system in (celestial.HouseSystem.EQUAL, celestial.HouseSystem.WHOLE_SIGN):
      check_snapshot(snapshot(location=celestial.GeoLocation(latitude, 0.0), system=system))
  for changes in failures:
    error = raises(celestial.CelestialError, lambda changes=changes: snapshot(**changes))
    assert error.operation == "chart_snapshot" and error.recorded
    check_snapshot(snapshot())
    assert _binding.last_error_text() == ""
  print("PASS installed historical/model/southern/wrap/station/polar/joint-domain chart cases")


def main() -> None:
  run_validation_guards()
  run_single_read_inputs()
  run_owned_values()
  run_error_translation()
  run_documented_replay()
  run_core_aligned_cases()


if __name__ == "__main__":
  main()
