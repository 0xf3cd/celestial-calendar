# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Installed-wheel public API, validation, and error-policy checks."""

from __future__ import annotations

import datetime
import importlib
import math
import os
import subprocess
import sys
from contextlib import ExitStack, contextmanager
from dataclasses import FrozenInstanceError
from importlib import resources
from pathlib import Path
from types import SimpleNamespace

import celestial_calendar as celestial
from celestial_calendar import _binding


def raises(error_type: type[BaseException], action: object) -> BaseException:
  """Return the expected exception or fail."""
  try:
    action()
  except error_type as error:
    return error
  raise AssertionError(f"expected {error_type.__name__}")


@contextmanager
def replaced_binding(name: str, replacement: object):
  """Temporarily replace one private callable so validation tests can count native calls."""
  if not name.startswith("celestial_"):
    name = f"celestial_{name}"
  original = _binding.FUNCTIONS[name]
  _binding.FUNCTIONS[name] = replacement
  try:
    yield
  finally:
    _binding.FUNCTIONS[name] = original


class Trap:
  """A callable that records calls and fails if configured as a validation trap."""

  def __init__(self, result: object = None, *, fail: bool = False) -> None:
    self.result = result
    self.fail = fail
    self.calls = 0
    self.args = ()

  def __call__(self, *args: object) -> object:
    self.calls += 1
    self.args = args
    if self.fail:
      raise AssertionError("native call crossed a rejected-input guard")
    return self.result


class CountFillFailure:
  """Succeed at count query, then reproduce the documented fill failure protocol."""

  def __init__(self, count: int) -> None:
    self.count = count
    self.calls = 0

  def __call__(self, *args: object) -> int:
    self.calls += 1
    count = args[-3]._obj
    if args[-2] is None:
      count.value = self.count
      return 0
    count.value = 0
    return 0


def run_happy_paths() -> None:
  """Exercise every public function against the native wheel."""
  assert resources.files(celestial).joinpath("py.typed").is_file()
  checks = []

  assert not hasattr(celestial, "set_log_verbosity") and not hasattr(celestial, "LogVerbosity")
  ut1 = celestial.CivilDateTime(2000, 1, 1, 0.5)
  assert celestial.ut1_to_jd(ut1) == 2451545.0
  checks.append("ut1_to_jd")
  assert math.isfinite(celestial.ut1_to_jde(celestial.CivilDateTime(2024, 6, 1, 0.5)))
  checks.append("ut1_to_jde")
  assert celestial.jde_to_ut1(2451545.0).year == 2000
  checks.append("jde_to_ut1")
  assert math.isfinite(celestial.utc_to_jde(celestial.CivilDateTime(2026, 10, 28, 0.5)))
  checks.append("utc_to_jde")
  assert celestial.jde_to_utc(1867522.5).year == 400
  checks.append("jde_to_utc")
  assert celestial.sun_apparent_geocentric_coordinate(2451545.0).distance_au > 0.0
  checks.append("sun_apparent_geocentric_coordinate")
  assert celestial.moon_apparent_geocentric_coordinate(2451545.0).distance_km > 0.0
  checks.append("moon_apparent_geocentric_coordinate")
  assert abs(celestial.moon_illumination(2448724.5).fraction - 0.6786) < 5e-5
  checks.append("moon_illumination")
  assert math.isfinite(celestial.moon_bright_limb_position_angle(2448724.5))
  checks.append("moon_bright_limb_position_angle")
  assert all(len(celestial.moon_phase_moments(2024, phase)) >= 12 for phase in celestial.MoonPhase)
  checks.append("moon_phase_moments")
  assert len(celestial.sun_longitude_crossings(2024, 0.0)) == 1
  assert len(celestial.sun_longitude_crossings(2024, 280.1)) == 2
  checks.append("sun_longitude_crossings")
  assert len(celestial.new_moons_after(2451545.0, 3)) == 3
  checks.append("new_moons_after")
  assert len(celestial.new_moons_in_year(2024)) >= 12
  checks.append("new_moons_in_year")
  assert math.isfinite(celestial.equation_of_time(2451545.0))
  checks.append("equation_of_time")
  assert celestial.apparent_solar_time(celestial.CivilDateTime(2024, 6, 1, 0.5), 116.4).day == 1
  previous_year = celestial.apparent_solar_time(celestial.CivilDateTime(1, 1, 1, 0.0), -180.0)
  assert (previous_year.year, previous_year.month, previous_year.day) == (0, 12, 31)
  checks.append("apparent_solar_time")
  assert 0.0 <= celestial.local_apparent_sidereal_time(2451545.0, 0.0) < 360.0
  checks.append("local_apparent_sidereal_time")
  lichun = celestial.jieqi_moment(401, celestial.Jieqi.LICHUN).moment_ut1
  assert (lichun.year, lichun.month, lichun.day) == (401, 2, 3)
  checks.append("jieqi_moment")
  # src/calendar/jieqi.hpp: Jieqi English aliases and JIEQI_NAME, indexed from Lichun.
  jieqi_names = (
    ("LICHUN", "立春"),
    ("YUSHUI", "雨水"),
    ("JINGZHE", "惊蛰"),
    ("CHUNFEN", "春分"),
    ("QINGMING", "清明"),
    ("GUYU", "谷雨"),
    ("LIXIA", "立夏"),
    ("XIAOMAN", "小满"),
    ("MANGZHONG", "芒种"),
    ("XIAZHI", "夏至"),
    ("XIAOSHU", "小暑"),
    ("DASHU", "大暑"),
    ("LIQIU", "立秋"),
    ("CHUSHU", "处暑"),
    ("BAILU", "白露"),
    ("QIUFEN", "秋分"),
    ("HANLU", "寒露"),
    ("SHUANGJIANG", "霜降"),
    ("LIDONG", "立冬"),
    ("XIAOXUE", "小雪"),
    ("DAXUE", "大雪"),
    ("DONGZHI", "冬至"),
    ("XIAOHAN", "小寒"),
    ("DAHAN", "大寒"),
  )
  assert len(celestial.Jieqi) == len(jieqi_names) == 24
  for index, (symbol, expected) in enumerate(jieqi_names):
    jieqi = celestial.Jieqi[symbol]
    assert jieqi.name == symbol and jieqi.value == index
    assert celestial.jieqi_name(jieqi) == expected
  checks.append("jieqi_name")
  assert celestial.supported_lunar_year_range(celestial.LunarAlgorithm.ALGO3) == celestial.LunarYearRange(1600, 2199)
  checks.append("supported_lunar_year_range")
  year_info = celestial.lunar_year_info(celestial.LunarAlgorithm.ALGO1, 2023)
  assert year_info.leap_month == 2 and len(year_info.month_lengths) == 13
  algo2_year_info = celestial.lunar_year_info(celestial.LunarAlgorithm.ALGO2, 2024)
  assert algo2_year_info.first_day == celestial.GregorianDate(2024, 2, 10)
  checks.append("lunar_year_info")
  for algorithm in (celestial.LunarAlgorithm.ALGO2, celestial.LunarAlgorithm.ALGO3):
    lunar = celestial.gregorian_to_lunar(algorithm, celestial.GregorianDate(2024, 2, 10))
    assert lunar == celestial.LunarDate(2024, 1, 1, False)
    assert celestial.lunar_to_gregorian(algorithm, lunar) == celestial.GregorianDate(2024, 2, 10)
  checks.append("gregorian_to_lunar")
  checks.append("lunar_to_gregorian")
  assert all(math.isfinite(celestial.delta_t(2024.5, model)) for model in celestial.DeltaTModel)
  # src/test/astro/delta_t_test.cpp::DefaultDispatch uses these three algo5 branches.
  for year in (1950.0, 2020.0, 2040.0):
    assert (
      celestial.delta_t(year)
      == celestial.delta_t(year, celestial.DeltaTModel.DEFAULT)
      == celestial.delta_t(year, celestial.DeltaTModel.ALGO5)
    )
    dispatch = []

    def selected(year, code, dispatch=dispatch):
      dispatch.append((year, code))
      return SimpleNamespace(valid=True, value=12.5 if code == 0 else 67.5, model_identity_code=5)

    with replaced_binding("delta_t", selected):
      assert celestial.delta_t(year) == 12.5
      assert dispatch == [(year, 0)]
      assert celestial.delta_t(year, celestial.DeltaTModel.DEFAULT) == 12.5
      assert dispatch == [(year, 0), (year, 0)]
      assert celestial.delta_t(year, celestial.DeltaTModel.ALGO5) == 67.5
      assert dispatch == [(year, 0), (year, 0), (year, 5)]
  checks.append("delta_t")

  assert len(checks) == len(set(checks)) == 23
  print(f"PASS public functions {len(checks)}/{len(checks)}")


def run_validation_guards() -> None:
  """Check binding types before dispatch and domain errors through native."""
  cases = [
    ("ut1_to_jd", lambda: celestial.ut1_to_jd(celestial.CivilDateTime(2024, 1, 1, True)), TypeError),
    ("ut1_to_jde", lambda: celestial.ut1_to_jde(celestial.CivilDateTime(2023, 2, 29, 0.0)), ValueError),
    ("jde_to_ut1", lambda: celestial.jde_to_ut1(float("nan")), ValueError),
    ("utc_to_jde", lambda: celestial.utc_to_jde(celestial.CivilDateTime(2024, 1, 1, True)), TypeError),
    ("jde_to_utc", lambda: celestial.jde_to_utc(float("nan")), ValueError),
    ("sun_apparent_geocentric_coord", lambda: celestial.sun_apparent_geocentric_coordinate(float("inf")), ValueError),
    ("moon_apparent_geocentric_coord", lambda: celestial.moon_apparent_geocentric_coordinate(False), TypeError),
    ("moon_illumination", lambda: celestial.moon_illumination("2451545"), TypeError),
    ("moon_position_angle", lambda: celestial.moon_bright_limb_position_angle(float("-inf")), ValueError),
    ("moon_phase_moments", lambda: celestial.moon_phase_moments(2024, 0), TypeError),
    ("solar_lon_root_discriminant", lambda: celestial.sun_longitude_crossings(2024, 360.0), ValueError),
    ("new_moons_after_jde", lambda: celestial.new_moons_after(2451545.0, celestial.Jieqi.DAHAN), TypeError),
    ("new_moons_after_jde", lambda: celestial.new_moons_after(2451545.0, 1.5), TypeError),
    ("new_moons_after_jde", lambda: celestial.new_moons_after(2451545.0, -1), ValueError),
    ("new_moons_in_year", lambda: celestial.new_moons_in_year(0), ValueError),
    ("equation_of_time", lambda: celestial.equation_of_time(True), TypeError),
    (
      "apparent_solar_time",
      lambda: celestial.apparent_solar_time(celestial.CivilDateTime(2024, 1, 1, 0.0), 180.1),
      ValueError,
    ),
    ("local_apparent_sidereal_time", lambda: celestial.local_apparent_sidereal_time(2451545.0, -181), ValueError),
    (
      "ut1_to_jd",
      lambda: celestial.ut1_to_jd(celestial.CivilDateTime(2024, 1, 1, 1.0)),
      ValueError,
    ),
    ("query_jieqi_moment", lambda: celestial.jieqi_moment(2024, 0), TypeError),
    ("query_jieqi_moment", lambda: celestial.jieqi_moment(400, celestial.Jieqi.LICHUN), ValueError),
    ("get_jieqi_name", lambda: celestial.jieqi_name(0), TypeError),
    ("get_supported_lunar_year_range", lambda: celestial.supported_lunar_year_range("algo1"), TypeError),
    ("get_lunar_year_info", lambda: celestial.lunar_year_info(celestial.LunarAlgorithm.ALGO1, 1900), ValueError),
    (
      "gregorian_to_lunar",
      lambda: celestial.gregorian_to_lunar(celestial.LunarAlgorithm.ALGO1, {"year": 2024}),
      TypeError,
    ),
    (
      "lunar_to_gregorian",
      lambda: celestial.lunar_to_gregorian(celestial.LunarAlgorithm.ALGO1, celestial.LunarDate(2023, 2, 1, 1)),
      TypeError,
    ),
    ("delta_t", lambda: celestial.delta_t(-4001, celestial.DeltaTModel.ALGO1), ValueError),
    ("delta_t", lambda: celestial.delta_t(3000, celestial.DeltaTModel.ALGO3), ValueError),
    ("delta_t", lambda: celestial.delta_t(2035, celestial.DeltaTModel.ALGO4), ValueError),
  ]
  for binding_name, action, error_type in cases:
    original = _binding.FUNCTIONS[f"celestial_{binding_name}"]
    seen = []

    def counted(*args, seen=seen, original=original):
      seen.append(args)
      return original(*args)

    with replaced_binding(binding_name, counted):
      raises(error_type, action)
    if error_type is TypeError:
      assert not seen, binding_name
    elif seen:
      assert len(seen) == 1 and _binding.last_error_kind() == 1, binding_name
  print(f"PASS binding/native invalid inputs {len(cases)}/{len(cases)}")


def run_undeclared_enum_members() -> None:
  """Reject an object of an enum class that is not one of its declared members before any native call."""

  def forged(enum_type: type, value: object, name: str) -> object:
    candidate = (int if issubclass(enum_type, int) else str).__new__(enum_type, value)
    candidate._value_ = value
    candidate._name_ = name
    return candidate

  date = celestial.GregorianDate(2026, 1, 1)
  lunar = celestial.LunarDate(2026, 1, 1, False)
  calls = (
    (celestial.MoonPhase, lambda value: celestial.moon_phase_moments(2026, value)),
    (celestial.Jieqi, lambda value: celestial.jieqi_moment(2026, value)),
    (celestial.Jieqi, celestial.jieqi_name),
    (celestial.LunarAlgorithm, celestial.supported_lunar_year_range),
    (celestial.LunarAlgorithm, lambda value: celestial.lunar_year_info(value, 2026)),
    (celestial.LunarAlgorithm, lambda value: celestial.gregorian_to_lunar(value, date)),
    (celestial.LunarAlgorithm, lambda value: celestial.lunar_to_gregorian(value, lunar)),
    (celestial.DeltaTModel, lambda value: celestial.delta_t(2000.0, value)),
  )
  traps = {name: Trap(fail=True) for name in _binding.FUNCTIONS}
  checked = 0
  with ExitStack() as stack:
    for binding_name, trap in traps.items():
      stack.enter_context(replaced_binding(binding_name, trap))
    for enum_type, call in calls:
      declared = next(iter(enum_type))
      unknown = 99 if issubclass(enum_type, int) else "unknown"
      for value, name in ((unknown, "UNKNOWN"), (declared.value, "LOOKALIKE")):
        candidate = forged(enum_type, value, name)
        assert isinstance(candidate, enum_type) and candidate is not declared
        raises(ValueError, lambda call=call, candidate=candidate: call(candidate))
        checked += 1
  assert checked == 2 * len(calls) and all(trap.calls == 0 for trap in traps.values())
  print(f"PASS undeclared enum members pre-native {checked}/{checked}")


def run_date_bridge() -> None:
  """Keep date-only conversion separate from time scales and the wider project date range."""

  class DateSubclass(datetime.date):
    pass

  class DateTimeSubclass(datetime.datetime):
    pass

  class GregorianDateSubclass(celestial.GregorianDate):
    pass

  assert isinstance(celestial.GregorianDate.__dict__["from_date"], staticmethod)
  for value in (
    datetime.date(1, 1, 1),
    datetime.date(9999, 12, 31),
    datetime.date(2000, 2, 29),
    DateSubclass(2024, 2, 10),
  ):
    expected = celestial.GregorianDate(value.year, value.month, value.day)
    for factory in (
      celestial.GregorianDate.from_date,
      GregorianDateSubclass.from_date,
      GregorianDateSubclass(10000, 1, 1).from_date,
    ):
      converted = factory(value)
      assert type(converted) is celestial.GregorianDate and converted == expected
      restored = converted.to_date()
      assert type(restored) is datetime.date and restored == value
  assert GregorianDateSubclass(2024, 2, 29).to_date() == datetime.date(2024, 2, 29)

  rejected_dates = (
    datetime.datetime(2024, 2, 10),
    datetime.datetime(2024, 2, 10, 23, 30, tzinfo=datetime.timezone(datetime.timedelta(hours=-8))),
    DateTimeSubclass(2024, 2, 10),
    DateTimeSubclass(2024, 2, 10, tzinfo=datetime.timezone.utc),
    celestial.CivilDateTime(2024, 2, 10, 0.0),
    {"year": 2024, "month": 2, "day": 10},
    "2024-02-10",
    None,
  )
  for value in (*rejected_dates, celestial.GregorianDate(2024, 2, 10)):
    raises(TypeError, lambda value=value: celestial.GregorianDate.from_date(value))

  invalid_dates = [
    (celestial.GregorianDate(0, 1, 1), ValueError),
    (celestial.GregorianDate(-1, 1, 1), ValueError),
    (celestial.GregorianDate(32768, 1, 1), ValueError),
    (celestial.GregorianDate(2024, 0, 1), ValueError),
    (celestial.GregorianDate(2024, 13, 1), ValueError),
    (celestial.GregorianDate(2024, 1, 0), ValueError),
    (celestial.GregorianDate(2024, 1, 32), ValueError),
    (celestial.GregorianDate(2024, 4, 31), ValueError),
    (celestial.GregorianDate(2023, 2, 29), ValueError),
    (celestial.GregorianDate(1900, 2, 29), ValueError),
  ]
  for field in ("year", "month", "day"):
    for value in (True, 1.0, "1", None, celestial.Jieqi.YUSHUI):
      fields = {"year": 2024, "month": 1, "day": 1, field: value}
      invalid_dates.append((celestial.GregorianDate(**fields), TypeError))
  for value, error_type in invalid_dates:
    raises(error_type, value.to_date)
  for year in (10000, 32767):
    value = celestial.GregorianDate(year, 1, 1)
    assert value.year == year
    raises(ValueError, value.to_date)

  for algorithm, native_algorithm in zip(celestial.LunarAlgorithm, (1, 2, 3), strict=True):
    rejected = Trap(fail=True)
    with replaced_binding("gregorian_to_lunar", rejected):
      for value in rejected_dates:
        raises(TypeError, lambda value=value, algorithm=algorithm: celestial.gregorian_to_lunar(algorithm, value))
      for value, error_type in invalid_dates:
        if error_type is TypeError:
          raises(error_type, lambda value=value, algorithm=algorithm: celestial.gregorian_to_lunar(algorithm, value))
    assert rejected.calls == 0

    for value, error_type in invalid_dates:
      if error_type is ValueError:
        raises(ValueError, lambda value=value, algorithm=algorithm: celestial.gregorian_to_lunar(algorithm, value))

    accepted = Trap(SimpleNamespace(valid=True, year=2024, month=1, day=1, is_leap=False))
    with (
      replaced_binding("gregorian_to_lunar", accepted),
      replaced_binding("last_error_kind", Trap(0)),
      replaced_binding("last_error", Trap(fail=True)),
    ):
      for year in (1, 9999, 10000, 32767):
        assert celestial.gregorian_to_lunar(algorithm, celestial.GregorianDate(year, 1, 1)) == celestial.LunarDate(
          2024, 1, 1, False
        )
        assert accepted.args == (native_algorithm, year, 1, 1)
        if year <= datetime.MAXYEAR:
          assert celestial.gregorian_to_lunar(algorithm, datetime.date(year, 1, 1)) == celestial.LunarDate(
            2024, 1, 1, False
          )
          assert accepted.args == (native_algorithm, year, 1, 1)
    assert accepted.calls == 6

    for value in (datetime.date(2024, 2, 10), DateSubclass(2024, 2, 10)):
      lunar = celestial.gregorian_to_lunar(algorithm, value)
      assert lunar == celestial.LunarDate(2024, 1, 1, False)
      gregorian = celestial.lunar_to_gregorian(algorithm, lunar)
      assert type(gregorian) is celestial.GregorianDate
      assert gregorian.to_date() == value
      assert celestial.gregorian_to_lunar(algorithm, gregorian.to_date()) == lunar
    assert celestial.lunar_year_info(algorithm, 2024).first_day.to_date() == datetime.date(2024, 2, 10)
  print("PASS date-only factory, guards, wide years and Lunar round trips")


def run_lunar_range_queries() -> None:
  """Query changing bounds without caching, while calculations leave admission to native."""
  query = Trap(fail=True)
  with replaced_binding("get_supported_lunar_year_range", query):
    original = sys.modules.pop("celestial_calendar")
    try:
      imported = importlib.import_module("celestial_calendar")
      assert imported is not original and imported._binding is _binding
    finally:
      sys.modules["celestial_calendar"] = original
  assert query.calls == 0

  for algorithm, native_algorithm in zip(celestial.LunarAlgorithm, (1, 2, 3), strict=True):
    query = Trap()
    celestial.ut1_to_jd(celestial.CivilDateTime(2000, 1, 1, 0.5))
    with replaced_binding("get_supported_lunar_year_range", query):
      for count, (start, end) in enumerate(((2024, 2024), (10000, 10001)), start=1):
        query.result = SimpleNamespace(valid=True, start=start, end=end)
        assert celestial.supported_lunar_year_range(algorithm) == celestial.LunarYearRange(start, end)
        assert query.calls == count and query.args == (native_algorithm,)

    cases = (
      (
        "lunar_year_info",
        "get_lunar_year_info",
        lambda year, algorithm=algorithm: celestial.lunar_year_info(algorithm, year),
        SimpleNamespace(valid=True, year=2024, month=2, day=10, leap_month=0, month_len=0),
      ),
      (
        "lunar_to_gregorian",
        "lunar_to_gregorian",
        lambda year, algorithm=algorithm: celestial.lunar_to_gregorian(
          algorithm, celestial.LunarDate(year, 1, 1, False)
        ),
        SimpleNamespace(valid=True, year=2024, month=2, day=10),
      ),
    )
    for operation, binding_name, action, result in cases:
      query = Trap(fail=True)
      calculation = Trap(result)
      with replaced_binding("get_supported_lunar_year_range", query), replaced_binding(binding_name, calculation):
        for count, year in enumerate((2024, 2024, 2025, 2023, 10000, 10001, 0), start=1):
          action(year)
          assert query.calls == 0 and calculation.calls == count
          assert calculation.args[:2] == (native_algorithm, year)
        calculation.fail = True
        for year in (True, 2024.0):
          raises(TypeError, lambda year=year, action=action: action(year))
          assert query.calls == 0 and calculation.calls == 7
      for detail in (b"calculation failed", b""):
        calculation = Trap(SimpleNamespace(valid=False))
        with (
          replaced_binding(binding_name, calculation),
          replaced_binding("last_error", Trap(detail)),
          replaced_binding("last_error_kind", Trap(2)),
        ):
          error = raises(celestial.CelestialError, lambda action=action: action(2024))
        assert error.operation == operation and error.recorded
        assert calculation.calls == 1
        assert str(error) == (detail.decode() if detail else f"{operation} failed (native error kind 2)")

    for detail in (b"range query failed", b""):
      query = Trap(SimpleNamespace(valid=False))
      error_reader = Trap(detail)
      with (
        replaced_binding("get_supported_lunar_year_range", query),
        replaced_binding("last_error", error_reader),
        replaced_binding("last_error_kind", Trap(2)),
      ):
        error = raises(
          celestial.CelestialError,
          lambda algorithm=algorithm: celestial.supported_lunar_year_range(algorithm),
        )
      assert error.operation == "supported_lunar_year_range" and error.recorded
      assert str(error) == (detail.decode() if detail else "supported_lunar_year_range failed (native error kind 2)")
      assert query.calls == error_reader.calls == 1 and query.args == (native_algorithm,)

    query = Trap(fail=True)
    calculation = Trap(fail=True)
    with replaced_binding("get_supported_lunar_year_range", query), replaced_binding("lunar_to_gregorian", calculation):
      for value in (None, {"year": 2024}, celestial.GregorianDate(2024, 1, 1)):
        raises(TypeError, lambda value=value, algorithm=algorithm: celestial.lunar_to_gregorian(algorithm, value))
    assert query.calls == calculation.calls == 0

    query = Trap(SimpleNamespace(valid=True, start=2024, end=2024))
    invalid_dates = (
      (celestial.LunarDate(2024, True, 1, False), TypeError),
      (celestial.LunarDate(2024, 0, 1, False), ValueError),
      (celestial.LunarDate(2024, 1, 31, False), ValueError),
      (celestial.LunarDate(2024, 1, 1, 1), TypeError),
    )
    with replaced_binding("get_supported_lunar_year_range", query):
      for value, error_type in invalid_dates:
        raises(error_type, lambda value=value, algorithm=algorithm: celestial.lunar_to_gregorian(algorithm, value))
      assert query.calls == 0
  print("PASS fresh native range queries, query-free import and native-owned Lunar admission")


def run_native_failures() -> None:
  """Prove native reasons cross the real library through public wrappers."""
  sidereal = raises(ValueError, lambda: celestial.local_apparent_sidereal_time(1e6, 0.0))
  assert "julian day number" in str(sidereal) and "below JD 1867522.5" in str(sidereal)

  lunar = raises(
    ValueError,
    lambda: celestial.gregorian_to_lunar(celestial.LunarAlgorithm.ALGO1, celestial.GregorianDate(2100, 2, 9)),
  )
  assert "cannot be represented" in str(lunar)
  print("PASS real native failure reasons 2/2")


def run_acceptance_boundaries() -> None:
  # Keep boundary categories aligned with
  # bindings/javascript/test/node/public_api_test.mjs::acceptedBoundaries; each package runner remains independent.
  boundaries = []
  assert math.isfinite(celestial.ut1_to_jd(celestial.CivilDateTime(1, 1, 1, 0.0)))
  boundaries.append("civil year lower")
  assert math.isfinite(celestial.ut1_to_jd(celestial.CivilDateTime(32767, 1, 1, 0.0)))
  boundaries.append("civil year upper")
  assert len(celestial.moon_phase_moments(32766, celestial.MoonPhase.NEW)) >= 12
  boundaries.append("phase year upper")
  assert len(celestial.new_moons_in_year(32766)) >= 12
  boundaries.append("new moons year upper")
  assert celestial.jieqi_moment(401, celestial.Jieqi.LICHUN).moment_ut1.year == 401
  boundaries.append("Jieqi year lower")
  assert celestial.jieqi_moment(32766, celestial.Jieqi.LICHUN).moment_ut1.year == 32766
  boundaries.append("Jieqi year upper")
  assert celestial.ut1_to_jd(celestial.CivilDateTime(2000, 1, 1, 0.0)) == 2451544.5
  boundaries.append("civil fraction lower")
  for longitude in (-180.0, 180.0):
    assert math.isfinite(celestial.local_apparent_sidereal_time(2451545.0, longitude)), longitude
    boundaries.append(f"longitude {longitude}")
    solar = celestial.apparent_solar_time(celestial.CivilDateTime(2024, 6, 1, 0.5), longitude)
    assert 0.0 <= solar.fraction < 1.0, longitude
    boundaries.append(f"solar longitude {longitude}")
  assert math.isfinite(celestial.delta_t(-4000, celestial.DeltaTModel.ALGO1))
  boundaries.append("delta T algo1 lower")

  lunar_ranges = {
    celestial.LunarAlgorithm.ALGO1: celestial.LunarYearRange(1901, 2099),
    celestial.LunarAlgorithm.ALGO2: celestial.LunarYearRange(410, 2500),
    celestial.LunarAlgorithm.ALGO3: celestial.LunarYearRange(1600, 2199),
  }
  for algorithm, expected in lunar_ranges.items():
    assert celestial.supported_lunar_year_range(algorithm) == expected
    for year in (expected.start, expected.end):
      assert celestial.lunar_year_info(algorithm, year).first_day.year == year, (algorithm, year)
      boundaries.append(f"{algorithm.value} year {year}")
  assert len(boundaries) == len(set(boundaries)) == 18
  print(f"PASS inclusive public boundaries {len(boundaries)}/{len(boundaries)}")


def run_protocol_seams() -> None:
  """Pin the count cap and last-error reads."""
  accepted = Trap(4096)
  with replaced_binding("new_moons_after_jde", accepted):
    assert len(celestial.new_moons_after(2451545.0, 4096)) == 4096
  assert accepted.calls == 1

  rejected = Trap(fail=True)
  with replaced_binding("new_moons_after_jde", rejected):
    raises(ValueError, lambda: celestial.new_moons_after(2451545.0, 4097))
  assert rejected.calls == 0

  error_reader = Trap(b"fill failed")
  recording_fill_failure = CountFillFailure(13)
  with (
    replaced_binding("last_error", error_reader),
    replaced_binding("moon_phase_moments", recording_fill_failure),
    replaced_binding("last_error_kind", lambda: 2 if recording_fill_failure.calls == 2 else 0),
  ):
    error = raises(
      celestial.CelestialError,
      lambda: celestial.moon_phase_moments(2024, celestial.MoonPhase.NEW),
    )
  assert error.recorded and recording_fill_failure.calls == 2 and error_reader.calls == 1
  assert str(error) == "fill failed"

  error_reader = Trap(b"new moon fill failed")
  new_moon_fill_failure = CountFillFailure(13)
  with (
    replaced_binding("last_error", error_reader),
    replaced_binding("new_moons_in_year", new_moon_fill_failure),
    replaced_binding("last_error_kind", lambda: 2 if new_moon_fill_failure.calls == 2 else 0),
  ):
    error = raises(celestial.CelestialError, lambda: celestial.new_moons_in_year(2024))
  assert error.recorded and new_moon_fill_failure.calls == 2 and error_reader.calls == 1
  assert str(error) == "new moon fill failed"

  error_reader = Trap(b"recorded detail")
  recording_failure = Trap(SimpleNamespace(valid=False))
  with (
    replaced_binding("last_error", error_reader),
    replaced_binding("last_error_kind", Trap(2)),
    replaced_binding("local_apparent_sidereal_time", recording_failure),
  ):
    error = raises(celestial.CelestialError, lambda: celestial.local_apparent_sidereal_time(2451545.0, 0.0))
  assert error.operation == "local_apparent_sidereal_time" and error.recorded
  assert recording_failure.calls == error_reader.calls == 1

  error_reader = Trap(b"sun coordinate failed")
  sun_failure = Trap(SimpleNamespace(valid=False))
  with (
    replaced_binding("last_error", error_reader),
    replaced_binding("sun_apparent_geocentric_coord", sun_failure),
    replaced_binding("last_error_kind", Trap(2)),
  ):
    error = raises(celestial.CelestialError, lambda: celestial.sun_apparent_geocentric_coordinate(2451545.0))
  assert error.operation == "sun_apparent_geocentric_coordinate" and error.recorded
  assert sun_failure.calls == error_reader.calls == 1

  assert celestial.sun_longitude_crossings(1, 281.3) == ()
  assert celestial.new_moons_after(2451545.0, 0) == ()
  assert celestial.lunar_year_info(celestial.LunarAlgorithm.ALGO3, 2024).leap_month is None
  assert celestial.jieqi_name(celestial.Jieqi.LICHUN) == "立春"
  print("PASS count boundary 4096/4097; last_error reads 4/4")


def run_sun_crossing_failures() -> None:
  """Both native entry points preserve the public operation on failure."""
  discriminant = Trap(fail=True)
  roots = Trap(fail=True)
  with replaced_binding("solar_lon_root_discriminant", discriminant), replaced_binding("solar_lon_roots", roots):
    for year, longitude, error_type in (
      (2024, float("nan"), ValueError),
      (True, 0.0, TypeError),
      (2024, "0.0", TypeError),
    ):
      raises(error_type, lambda year=year, longitude=longitude: celestial.sun_longitude_crossings(year, longitude))
  assert discriminant.calls == roots.calls == 0
  for year, longitude in ((0, 0.0), (32767, 0.0), (2024, -1.0), (2024, 360.0)):
    raises(ValueError, lambda year=year, longitude=longitude: celestial.sun_longitude_crossings(year, longitude))

  for detail in (b"sun crossing failed", b""):
    for valid in (False, True):
      discriminant = Trap(SimpleNamespace(valid=valid, count=1))
      roots = Trap(0, fail=not valid)
      error_reader = Trap(detail)
      with (
        replaced_binding("solar_lon_root_discriminant", discriminant),
        replaced_binding("solar_lon_roots", roots),
        replaced_binding("last_error", error_reader),
        replaced_binding("last_error_kind", lambda valid=valid, roots=roots: 2 if not valid or roots.calls else 0),
      ):
        error = raises(celestial.CelestialError, lambda: celestial.sun_longitude_crossings(2024, 0.0))
      assert error.operation == "sun_longitude_crossings" and error.recorded
      assert str(error) == (detail.decode() if detail else "sun_longitude_crossings failed (native error kind 2)")
      assert discriminant.calls == error_reader.calls == 1 and roots.calls == int(valid)
  print("PASS renamed sun crossing failure operations")


def run_value_contract() -> None:
  """Pin immutable values and the intentionally flat public surface."""
  date = celestial.GregorianDate(2024, 1, 1)
  raises(FrozenInstanceError, lambda: setattr(date, "day", 2))
  assert len(celestial.__all__) == len(set(celestial.__all__))
  assert celestial.chart is importlib.import_module("celestial_calendar.chart")
  public_names = {
    name
    for name in celestial.__dict__
    if not name.startswith("_") and name not in {"chart", "events", "capability", "query"}
  } | {"__version__"}
  assert set(celestial.__all__) == public_names
  assert set(celestial.__all__) == {
    "BodyState",
    "CapabilityOperation",
    "CapabilityRecord",
    "CapabilityStatus",
    "CelestialError",
    "ChartHouses",
    "ChartSnapshot",
    "ChartTimes",
    "CivilDateTime",
    "CivilHouseCapability",
    "CivilScale",
    "DeltaTModel",
    "EquatorialPosition",
    "EventDirection",
    "GeoLocation",
    "GregorianDate",
    "HouseQueryResult",
    "HouseSystem",
    "Jieqi",
    "JieqiMoment",
    "LunarAlgorithm",
    "LunarDate",
    "LunarYearInfo",
    "LunarYearRange",
    "LongitudeDifferenceEvent",
    "LongitudeEvent",
    "ModelIdentity",
    "ModelKey",
    "ModelKind",
    "ModelStatus",
    "MoonCoordinate",
    "MoonIllumination",
    "MoonPhase",
    "NutationModel",
    "Position",
    "StationEvent",
    "StationMotion",
    "SunCoordinate",
    "Target",
    "__version__",
    "apparent_solar_time",
    "capabilities",
    "capability_text",
    "chart_snapshot",
    "delta_t",
    "equation_of_time",
    "equatorial_positions",
    "gregorian_to_lunar",
    "house_query",
    "house_capability",
    "jde_to_ut1",
    "jde_to_utc",
    "jieqi_moment",
    "jieqi_name",
    "local_apparent_sidereal_time",
    "longitude_difference_events",
    "longitude_events",
    "longitude_stations",
    "lunar_to_gregorian",
    "lunar_year_info",
    "moon_apparent_geocentric_coordinate",
    "moon_bright_limb_position_angle",
    "moon_illumination",
    "moon_phase_moments",
    "model_identities",
    "model_identity_text",
    "new_moons_after",
    "new_moons_in_year",
    "positions",
    "sun_apparent_geocentric_coordinate",
    "sun_longitude_crossings",
    "supported_lunar_year_range",
    "ut1_to_jd",
    "ut1_to_jde",
    "utc_to_jde",
  }
  assert not hasattr(celestial, "solar_longitude_roots")
  assert not hasattr(celestial, "last_error")
  assert not hasattr(celestial, "solar_lon_root_discriminant")
  assert not hasattr(celestial, "delta_t_algo1")
  for name in ("CivilDateTime", "DeltaTModel", "CelestialError"):
    value_type = getattr(celestial, name)
    assert value_type.__module__.startswith("celestial_calendar")
    assert getattr(importlib.import_module(value_type.__module__), name) is value_type
  # Chart's tighter raw domain must not narrow the existing civil helpers.
  assert math.isfinite(celestial.ut1_to_jd(celestial.CivilDateTime(1884, 1, 1, 0.0)))
  raises(ValueError, lambda: celestial._finite(10**1000, "value"))
  raises(TypeError, lambda: celestial._finite(celestial.Jieqi.YUSHUI, "value"))
  print("PASS frozen values and public surface")


def run_utc_conversions() -> None:
  """Pin the inherited conversion policy without assuming a global exact inverse."""
  for year in (1900, 1972, 2026, 2100, 32767):
    value = celestial.CivilDateTime(year, 6, 1, 0.56789)
    jde = celestial.utc_to_jde(value)
    result = celestial.jde_to_utc(jde)
    tolerance = 2e-4 if year == 32767 else 1e-4
    assert abs((celestial.ut1_to_jd(result) - celestial.ut1_to_jd(value)) * 86400.0) < tolerance
    raw = _binding.call("celestial_utc_to_jde", year, 6, 1, value.fraction)
    assert raw.valid and raw.value == jde
    raw_result = _binding.call("celestial_jde_to_utc", jde)
    assert raw_result.valid and (result.year, result.month, result.day, result.fraction) == (
      raw_result.year,
      raw_result.month,
      raw_result.day,
      raw_result.fraction,
    )

  for year in (401, 1900):
    historical = celestial.CivilDateTime(year, 6, 1, 0.56789)
    assert celestial.utc_to_jde(historical) == celestial.ut1_to_jde(historical)
    historical_jde = celestial.utc_to_jde(historical)
    assert celestial.jde_to_utc(historical_jde) == celestial.jde_to_ut1(historical_jde)
  for year in (2017, 2026, 2100):
    value = celestial.CivilDateTime(year, 1, 1, 0.5)
    assert abs((celestial.utc_to_jde(value) - celestial.ut1_to_jd(value)) * 86400.0 - 69.184) < 1e-4

  seam_before = celestial.CivilDateTime(1971, 12, 31, 1 - 0.001 / 86400)
  seam_after = celestial.CivilDateTime(1972, 1, 1, 0.0)
  assert abs((celestial.utc_to_jde(seam_after) - celestial.utc_to_jde(seam_before)) * 86400 + 0.067) < 1e-4
  for value in (
    celestial.CivilDateTime(1971, 12, 31, 1 - 0.1 / 86400),
    celestial.CivilDateTime(1972, 1, 1, 0.1 / 86400),
  ):
    result = celestial.jde_to_utc(celestial.utc_to_jde(value))
    assert abs((celestial.ut1_to_jd(result) - celestial.ut1_to_jd(value)) * 86400) < 1e-4

  for year, month in ((1972, 7), (1973, 1), (2017, 1)):
    midnight = datetime.date(year, month, 1)
    previous = midnight - datetime.timedelta(days=1)
    for day, fraction in ((previous, 1 - 0.001 / 86400), (midnight, 0.001 / 86400)):
      value = celestial.CivilDateTime(day.year, day.month, day.day, fraction)
      result = celestial.jde_to_utc(celestial.utc_to_jde(value))
      assert abs((celestial.ut1_to_jd(result) - celestial.ut1_to_jd(value)) * 86400) < 1e-4
    value = celestial.CivilDateTime(year, month, 1, 0.0)
    jde = celestial.utc_to_jde(value)
    result = celestial.jde_to_utc(jde)
    raw = _binding.call("celestial_jde_to_utc", jde)
    assert (result.year, result.month, result.day, result.fraction) == (raw.year, raw.month, raw.day, raw.fraction)
    gap = (celestial.ut1_to_jd(result) - celestial.ut1_to_jd(value)) * 86400
    assert abs(gap) < 1e-4 or abs(gap - 1) < 1e-4

  before = celestial.utc_to_jde(celestial.CivilDateTime(2016, 12, 31, 86399 / 86400))
  after = celestial.utc_to_jde(celestial.CivilDateTime(2017, 1, 1, 0.0))
  assert abs((after - before) * 86400 - 2) < 1e-4
  inserted = before + 1.5 / 86400
  collapsed = celestial.jde_to_utc(inserted)
  assert (collapsed.year, collapsed.month, collapsed.day) == (2017, 1, 1)
  assert abs(collapsed.fraction * 86400 - 0.5) < 1e-4
  assert abs((celestial.utc_to_jde(collapsed) - inserted) * 86400 - 1) < 1e-4

  first = celestial.utc_to_jde(celestial.CivilDateTime(1, 1, 1, 0.0))
  assert first < 1867522.5
  raises(ValueError, lambda: celestial.jde_to_utc(first))
  assert celestial.jde_to_utc(1867522.5).year == 400
  assert celestial.jde_to_utc(math.nextafter(13689325.5, -math.inf)).year == 32767
  for jde in (math.nextafter(1867522.5, -math.inf), 13689325.5, math.nan, math.inf, -math.inf):
    raises(ValueError, lambda jde=jde: celestial.jde_to_utc(jde))
  for year, month, day, fraction in (
    (0, 1, 1, 0.0),
    (-4000, 1, 1, 0.0),
    (32768, 1, 1, 0.0),
    (2024, 2, 30, 0.0),
    (2024, 257, 1, 0.0),
    (2024, 1, 257, 0.0),
    *((2024, 1, 1, fraction) for fraction in (-1e-300, 1.0, math.nan, math.inf, -math.inf)),
    (32767, 12, 31, 0.9999999),
  ):
    raises(
      ValueError, lambda y=year, m=month, d=day, f=fraction: celestial.utc_to_jde(celestial.CivilDateTime(y, m, d, f))
    )
  for value in (True, "2451545", historical):
    raises(TypeError, lambda value=value: celestial.jde_to_utc(value))
  raises(TypeError, lambda: celestial.utc_to_jde(datetime.datetime(2026, 1, 1, tzinfo=datetime.timezone.utc)))
  assert celestial.utc_to_jde(celestial.CivilDateTime(2024, 1, 1, -0.0)) == celestial.utc_to_jde(
    celestial.CivilDateTime(2024, 1, 1, 0.0)
  )
  assert math.isfinite(celestial.utc_to_jde(celestial.CivilDateTime(32767, 12, 31, 0.0)))
  print("PASS UTC conversions: raw parity, history, seam, leap edges, inserted second, fixed future offset and domains")
  example = Path(__file__).resolve().parents[4] / "docs/examples/utc_jieqi/example.py"
  executed = subprocess.run(
    [sys.executable, str(example)],
    capture_output=True,
    encoding="utf-8",
    check=True,
    env={**os.environ, "PYTHONIOENCODING": "utf-8"},
  )
  assert executed.stdout.splitlines() == ["立冬", "UTC: 2026-11-07 09:52", "UTC+8: 2026-11-07 17:52"]
  print("PASS published UTC-input Jieqi example, including UTC+8 output")


def run_event_wrappers() -> None:
  """Exercise the event wrappers' success, legal-empty and exception channels."""
  crossings = celestial.longitude_events(celestial.Target.SUN, 0.0, 2460735.5, 2460785.5)
  assert len(crossings) == 1 and crossings[0].direction is celestial.EventDirection.INCREASING
  assert celestial.longitude_stations(celestial.Target.SUN, 2460735.5, 2460785.5) == ()
  differences = celestial.longitude_difference_events(
    celestial.Target.MOON, celestial.Target.SUN, 60.0, 2460676.5, 2460707.5
  )
  assert len(differences) == 1
  raises(TypeError, lambda: celestial.longitude_events(0, 0.0, 1.0, 2.0))
  raises(ValueError, lambda: celestial.longitude_events(celestial.Target.SUN, float("nan"), 1.0, 2.0))
  raises(ValueError, lambda: celestial.longitude_events(celestial.Target.SUN, 0.0, 2.0, 1.0))
  raises(
    ValueError,
    lambda: celestial.longitude_difference_events(
      celestial.Target.SUN, celestial.Target.SUN, 1.0, 2460676.5, 2460707.5
    ),
  )
  error = raises(
    ValueError,
    lambda: celestial.longitude_events(celestial.Target.SUN, 0.0, 1.0e7, 1.0e7 + 10.0),
  )
  assert str(error) and _binding.last_error_kind() == 1
  print("PASS event wrappers: success, legal empty, TypeError/ValueError/CelestialError channels")


def main() -> None:
  """Run the installed-wheel consumer suite."""
  run_happy_paths()
  run_validation_guards()
  run_undeclared_enum_members()
  run_date_bridge()
  run_lunar_range_queries()
  run_native_failures()
  run_acceptance_boundaries()
  run_protocol_seams()
  run_sun_crossing_failures()
  run_value_contract()
  run_event_wrappers()
  run_utc_conversions()


if __name__ == "__main__":
  main()
