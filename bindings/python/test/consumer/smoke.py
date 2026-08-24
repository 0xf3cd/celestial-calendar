# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

"""Installed-wheel public API, validation, and error-policy checks."""

from __future__ import annotations

import math
from contextlib import contextmanager
from dataclasses import FrozenInstanceError
from importlib import resources
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

  def __call__(self, *args: object) -> object:
    self.calls += 1
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

  celestial.set_log_verbosity(celestial.LogVerbosity.NONE)
  checks.append("set_log_verbosity")
  ut1 = celestial.CivilDateTime(2000, 1, 1, 0.5)
  assert celestial.ut1_to_jd(ut1) == 2451545.0
  checks.append("ut1_to_jd")
  assert math.isfinite(celestial.ut1_to_jde(celestial.CivilDateTime(2024, 6, 1, 0.5)))
  checks.append("ut1_to_jde")
  assert celestial.jde_to_ut1(2451545.0).year == 2000
  checks.append("jde_to_ut1")
  assert celestial.sun_apparent_geocentric_coordinate(2451545.0).radius_au > 0.0
  checks.append("sun_apparent_geocentric_coordinate")
  assert celestial.moon_apparent_geocentric_coordinate(2451545.0).distance_km > 0.0
  checks.append("moon_apparent_geocentric_coordinate")
  assert abs(celestial.moon_illumination(2448724.5).fraction - 0.6786) < 5e-5
  checks.append("moon_illumination")
  assert math.isfinite(celestial.moon_bright_limb_position_angle(2448724.5))
  checks.append("moon_bright_limb_position_angle")
  assert all(len(celestial.moon_phase_moments(2024, phase)) >= 12 for phase in celestial.MoonPhase)
  checks.append("moon_phase_moments")
  assert len(celestial.solar_longitude_roots(2024, 0.0)) == 1
  assert len(celestial.solar_longitude_roots(2024, 280.1)) == 2
  checks.append("solar_longitude_roots")
  assert len(celestial.new_moons_after(2451545.0, 3)) == 3
  checks.append("new_moons_after")
  assert len(celestial.new_moons_in_year(2024)) >= 12
  checks.append("new_moons_in_year")
  assert math.isfinite(celestial.equation_of_time(2451545.0))
  checks.append("equation_of_time")
  assert celestial.apparent_solar_time(celestial.CivilDateTime(2024, 6, 1, 0.5), 116.4).day == 1
  checks.append("apparent_solar_time")
  assert 0.0 <= celestial.local_apparent_sidereal_time(2451545.0, 0.0) < 360.0
  checks.append("local_apparent_sidereal_time")
  lichun = celestial.jieqi_moment(401, celestial.Jieqi.LICHUN).moment_ut1
  assert (lichun.year, lichun.month, lichun.day) == (401, 2, 3)
  checks.append("jieqi_moment")
  assert celestial.jieqi_name(celestial.Jieqi.LICHUN) == "立春"
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
  checks.append("delta_t")

  assert len(checks) == len(set(checks)) == 22
  print("PASS public functions 22/22")


def run_validation_guards() -> None:
  """Prove hostile input is rejected before its native callable can run."""
  cases = [
    ("set_log_verbosity", lambda: celestial.set_log_verbosity("none"), TypeError),
    ("ut1_to_jd", lambda: celestial.ut1_to_jd(celestial.CivilDateTime(2024, 1, 1, True)), TypeError),
    ("ut1_to_jde", lambda: celestial.ut1_to_jde(celestial.CivilDateTime(2023, 2, 29, 0.0)), ValueError),
    ("jde_to_ut1", lambda: celestial.jde_to_ut1(float("nan")), ValueError),
    ("sun_apparent_geocentric_coord", lambda: celestial.sun_apparent_geocentric_coordinate(float("inf")), ValueError),
    ("moon_apparent_geocentric_coord", lambda: celestial.moon_apparent_geocentric_coordinate(False), TypeError),
    ("moon_illumination", lambda: celestial.moon_illumination("2451545"), TypeError),
    ("moon_position_angle", lambda: celestial.moon_bright_limb_position_angle(float("-inf")), ValueError),
    ("moon_phase_moments", lambda: celestial.moon_phase_moments(2024, 0), TypeError),
    ("solar_lon_root_discriminant", lambda: celestial.solar_longitude_roots(2024, 360.0), ValueError),
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
    ("delta_t_algo1", lambda: celestial.delta_t(-4001, celestial.DeltaTModel.ALGO1), ValueError),
    ("delta_t_algo3", lambda: celestial.delta_t(3000, celestial.DeltaTModel.ALGO3), ValueError),
    ("delta_t_algo4", lambda: celestial.delta_t(2035, celestial.DeltaTModel.ALGO4), ValueError),
  ]
  for binding_name, action, error_type in cases:
    trap = Trap(fail=True)
    with replaced_binding(binding_name, trap):
      raises(error_type, action)
    assert trap.calls == 0, binding_name
  print(f"PASS hostile inputs pre-native {len(cases)}/{len(cases)}")


def run_native_failures() -> None:
  """Prove native reasons cross the real library through public wrappers."""
  sidereal = raises(celestial.CelestialError, lambda: celestial.local_apparent_sidereal_time(1e6, 0.0))
  assert sidereal.operation == "local_apparent_sidereal_time" and sidereal.recorded
  assert "julian day number" in str(sidereal) and "below JD 1867522.5" in str(sidereal)

  lunar = raises(
    celestial.CelestialError,
    lambda: celestial.gregorian_to_lunar(celestial.LunarAlgorithm.ALGO1, celestial.GregorianDate(2100, 2, 9)),
  )
  assert lunar.operation == "gregorian_to_lunar" and lunar.recorded
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
  assert len(boundaries) == len(set(boundaries)) == 16
  print("PASS inclusive public boundaries 16/16")


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
  with replaced_binding("last_error", error_reader), replaced_binding(
    "moon_phase_moments", recording_fill_failure
  ):
    error = raises(
      celestial.CelestialError,
      lambda: celestial.moon_phase_moments(2024, celestial.MoonPhase.NEW),
    )
  assert error.recorded and recording_fill_failure.calls == 2 and error_reader.calls == 1
  assert str(error) == "fill failed"

  error_reader = Trap(b"new moon fill failed")
  new_moon_fill_failure = CountFillFailure(13)
  with replaced_binding("last_error", error_reader), replaced_binding(
    "new_moons_in_year", new_moon_fill_failure
  ):
    error = raises(celestial.CelestialError, lambda: celestial.new_moons_in_year(2024))
  assert error.recorded and new_moon_fill_failure.calls == 2 and error_reader.calls == 1
  assert str(error) == "new moon fill failed"

  error_reader = Trap(b"recorded detail")
  recording_failure = Trap(SimpleNamespace(valid=False))
  with replaced_binding("last_error", error_reader), replaced_binding(
    "local_apparent_sidereal_time", recording_failure
  ):
    error = raises(celestial.CelestialError, lambda: celestial.local_apparent_sidereal_time(2451545.0, 0.0))
  assert error.operation == "local_apparent_sidereal_time" and error.recorded
  assert recording_failure.calls == error_reader.calls == 1

  error_reader = Trap(b"sun coordinate failed")
  sun_failure = Trap(SimpleNamespace(valid=False))
  with replaced_binding("last_error", error_reader), replaced_binding(
    "sun_apparent_geocentric_coord", sun_failure
  ):
    error = raises(celestial.CelestialError, lambda: celestial.sun_apparent_geocentric_coordinate(2451545.0))
  assert error.operation == "sun_apparent_geocentric_coordinate" and error.recorded
  assert sun_failure.calls == error_reader.calls == 1

  assert celestial.solar_longitude_roots(1, 281.3) == ()
  assert celestial.new_moons_after(2451545.0, 0) == ()
  assert celestial.lunar_year_info(celestial.LunarAlgorithm.ALGO3, 2024).leap_month is None
  assert celestial.jieqi_name(celestial.Jieqi.LICHUN) == "立春"
  print("PASS count boundary 4096/4097; last_error reads 4/4")


def run_value_contract() -> None:
  """Pin immutable values and the intentionally flat public surface."""
  date = celestial.GregorianDate(2024, 1, 1)
  raises(FrozenInstanceError, lambda: setattr(date, "day", 2))
  assert len(celestial.__all__) == len(set(celestial.__all__))
  public_names = {name for name in celestial.__dict__ if not name.startswith("_")} | {"__version__"}
  assert set(celestial.__all__) == public_names
  assert not hasattr(celestial, "last_error")
  assert not hasattr(celestial, "solar_lon_root_discriminant")
  assert not hasattr(celestial, "delta_t_algo1")
  print("PASS frozen values and public surface")


def main() -> None:
  """Run the installed-wheel consumer suite."""
  run_happy_paths()
  run_validation_guards()
  run_native_failures()
  run_acceptance_boundaries()
  run_protocol_seams()
  run_value_contract()


if __name__ == "__main__":
  main()
