# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""One happy and one edge/protocol check for every C export in an installed wheel."""

from __future__ import annotations

import ctypes
import math
import traceback

from celestial_calendar import _binding


NAN = float("nan")


def call(name: str, *args: object) -> object:
  return _binding.call(name, *args)


def happy_set_log_verbosity() -> None:
  assert call("set_log_verbosity", 0)


def happy_last_error() -> None:
  assert call("ut1_to_jd", 2024, 6, 1, 0.5).valid
  assert _binding.last_error_text() == ""


def happy_ut1_to_jd() -> None:
  result = call("ut1_to_jd", 2024, 6, 1, 0.5)
  assert result.valid and abs(result.value - 2460463.0) < 1e-6


def happy_ut1_to_jde() -> None:
  result = call("ut1_to_jde", 2024, 6, 1, 0.5)
  assert result.valid and result.value > 2460463.0


def happy_jde_to_ut1() -> None:
  result = call("jde_to_ut1", 2460463.0)
  assert result.valid and (result.year, result.month, result.day) == (2024, 6, 1)


def happy_sun_apparent_geocentric_coord() -> None:
  result = call("sun_apparent_geocentric_coord", 2460463.0)
  assert result.valid and 0.0 <= result.lon < 360.0 and result.r > 0.0


def happy_moon_apparent_geocentric_coord() -> None:
  result = call("moon_apparent_geocentric_coord", 2460463.0)
  assert result.valid and 0.0 <= result.lon < 360.0 and result.r > 0.0


def happy_moon_illumination() -> None:
  result = call("moon_illumination", 2448724.5)
  assert result.valid and abs(result.illumination - 0.6786) < 5e-5


def happy_moon_position_angle() -> None:
  result = call("moon_position_angle", 2448724.5)
  assert result.valid and abs(result.angle_deg - 285.0) < 0.05


def happy_moon_phase_moments() -> None:
  total = ctypes.c_uint32()
  assert call("moon_phase_moments", 2024, 0, ctypes.byref(total), None, 0) == 0
  assert total.value in (12, 13)
  slots = (ctypes.c_double * total.value)()
  assert call("moon_phase_moments", 2024, 0, ctypes.byref(total), slots, total.value) == total.value


def happy_solar_lon_root_discriminant() -> None:
  result = call("solar_lon_root_discriminant", 2024, 0.0)
  assert result.valid and result.count == 1


def happy_solar_lon_roots() -> None:
  slots = (ctypes.c_double * 1)()
  assert call("solar_lon_roots", 2024, 0.0, slots, 1) == 1 and math.isfinite(slots[0])


def happy_new_moons_after_jde() -> None:
  slots = (ctypes.c_double * 3)()
  assert call("new_moons_after_jde", 2460463.0, slots, 3) == 3 and slots[0] < slots[1] < slots[2]


def happy_new_moons_in_year() -> None:
  total = ctypes.c_uint32()
  assert call("new_moons_in_year", 2024, ctypes.byref(total), None, 0) == 0
  assert total.value in (12, 13)


def happy_equation_of_time() -> None:
  result = call("equation_of_time", 2460463.0)
  assert result.valid and abs(result.value) < 5.0


def happy_apparent_solar_time() -> None:
  result = call("apparent_solar_time", 2024, 6, 1, 0.5, 116.4)
  assert result.valid and (result.year, result.month, result.day) == (2024, 6, 1)


def happy_local_apparent_sidereal_time() -> None:
  result = call("local_apparent_sidereal_time", 2460463.0, 120.0)
  assert result.valid and 0.0 <= result.value < 360.0


def happy_query_jieqi_moment() -> None:
  result = call("query_jieqi_moment", 2024, 0)
  assert result.valid and (result.jq_idx, result.y, result.m) == (0, 2024, 2)


def happy_get_jieqi_name() -> None:
  buffer = ctypes.create_string_buffer(16)
  assert call("get_jieqi_name", 0, buffer, len(buffer))
  assert buffer.value == "立春".encode()


def happy_get_supported_lunar_year_range() -> None:
  result = call("get_supported_lunar_year_range", 3)
  assert result.valid and (result.start, result.end) == (1600, 2199)


def happy_get_lunar_year_info() -> None:
  result = call("get_lunar_year_info", 2, 2024)
  assert result.valid and (result.year, result.month, result.day) == (2024, 2, 10)


def happy_gregorian_to_lunar() -> None:
  result = call("gregorian_to_lunar", 1, 2023, 3, 22)
  assert result.valid and (result.year, result.month, result.is_leap, result.day) == (2023, 2, True, 1)


def happy_lunar_to_gregorian() -> None:
  result = call("lunar_to_gregorian", 1, 2023, 2, True, 1)
  assert result.valid and (result.year, result.month, result.day) == (2023, 3, 22)


def _happy_delta_t(name: str) -> None:
  result = call(name, 2024.5)
  assert result.valid and math.isfinite(result.value)


def happy_delta_t_algo1() -> None:
  _happy_delta_t("delta_t_algo1")


def happy_delta_t_algo2() -> None:
  _happy_delta_t("delta_t_algo2")


def happy_delta_t_algo3() -> None:
  _happy_delta_t("delta_t_algo3")


def happy_delta_t_algo4() -> None:
  _happy_delta_t("delta_t_algo4")


def happy_delta_t_algo5() -> None:
  _happy_delta_t("delta_t_algo5")


def happy_delta_t() -> None:
  _happy_delta_t("delta_t")


def happy_chart_snapshot_v1() -> None:
  # Direct FFI calls exercise the large by-value return without public Python validation.
  arguments = (2024, 6, 1, 0.5, 1, -33.9, 151.2, 0, 0)
  result = _binding.FUNCTIONS["chart_snapshot_v1"](*arguments)
  assert result.valid and _binding.last_error_text() == ""
  assert result.jd_ut1 == 2460463.0 and result.jde_tt > result.jd_ut1
  assert len(result.bodies) == 14 and len(result.houses.cusps_deg) == 12
  assert [body.target_code for body in result.bodies] == list(range(14))
  assert [body.present_fields for body in result.bodies] == [3] * 10 + [1] * 4
  for body in result.bodies:
    assert math.isfinite(body.longitude_deg) and 0.0 <= body.longitude_deg < 360.0
    assert math.isfinite(body.latitude_deg) and -90.0 <= body.latitude_deg <= 90.0
    assert math.isfinite(body.longitude_rate_deg_per_tt_day)
    if body.present_fields & 2:
      assert math.isfinite(body.distance_au) and body.distance_au > 0.0
    else:
      assert body.latitude_deg == 0.0

  # Separate legacy exports anchor times, ordering and the Moon's KM-to-AU conversion.
  sun = call("sun_apparent_geocentric_coord", result.jde_tt)
  moon = call("moon_apparent_geocentric_coord", result.jde_tt)
  assert sun.valid and moon.valid
  assert abs(result.bodies[0].longitude_deg - sun.lon) < 1e-10
  assert abs(result.bodies[0].distance_au - sun.r) < 1e-12
  assert abs(result.bodies[1].longitude_deg - moon.lon) < 1e-10
  assert abs(result.bodies[1].distance_au - moon.r / 149597870.7) < 1e-12
  for ascending, descending in ((10, 11), (12, 13)):
    assert abs((result.bodies[descending].longitude_deg - result.bodies[ascending].longitude_deg) % 360 - 180) < 1e-10

  axes = result.houses
  for angle in (axes.ascendant_deg, axes.midheaven_deg, axes.descendant_deg, axes.imum_coeli_deg, *axes.cusps_deg):
    assert math.isfinite(angle) and 0.0 <= angle < 360.0
  assert abs((axes.descendant_deg - axes.ascendant_deg) % 360 - 180) < 1e-10
  assert abs((axes.imum_coeli_deg - axes.midheaven_deg) % 360 - 180) < 1e-10
  for index, cusp in enumerate(axes.cusps_deg):
    assert abs(math.remainder(cusp - axes.ascendant_deg - index * 30, 360.0)) < 1e-10

  explicit = _binding.FUNCTIONS["chart_snapshot_v1"](*arguments[:-1], 5)
  assert explicit.valid and explicit.jd_ut1 == result.jd_ut1 and explicit.jde_tt == result.jde_tt
  assert [tuple(getattr(body, name) for name, _ in body._fields_) for body in explicit.bodies] == [
    tuple(getattr(body, name) for name, _ in body._fields_) for body in result.bodies
  ]
  assert tuple(explicit.houses.cusps_deg) == tuple(axes.cusps_deg)


def edge_set_log_verbosity() -> None:
  assert call("set_log_verbosity", 0)
  assert not call("set_log_verbosity", 3)


def edge_last_error() -> None:
  assert not call("ut1_to_jd", 2024, 6, 1, NAN).valid
  recorded = _binding.last_error_text()
  assert recorded
  assert not call("sun_apparent_geocentric_coord", NAN).valid
  assert _binding.last_error_text() and _binding.last_error_text() != recorded
  assert call("ut1_to_jd", 2024, 6, 1, 0.5).valid
  assert _binding.last_error_text() == ""


def edge_ut1_to_jd() -> None:
  assert not call("ut1_to_jd", 2024, 6, 1, NAN).valid and _binding.last_error_text()


def edge_ut1_to_jde() -> None:
  assert not call("ut1_to_jde", 2024, 13, 1, 0.5).valid and _binding.last_error_text()


def edge_jde_to_ut1() -> None:
  assert not call("jde_to_ut1", NAN).valid and _binding.last_error_text()


def edge_sun_apparent_geocentric_coord() -> None:
  assert not call("sun_apparent_geocentric_coord", NAN).valid


def edge_moon_apparent_geocentric_coord() -> None:
  assert not call("moon_apparent_geocentric_coord", NAN).valid


def edge_moon_illumination() -> None:
  assert not call("moon_illumination", NAN).valid and _binding.last_error_text()


def edge_moon_position_angle() -> None:
  assert not call("moon_position_angle", NAN).valid and _binding.last_error_text()


def edge_moon_phase_moments() -> None:
  total = ctypes.c_uint32()
  assert call("moon_phase_moments", 2024, 0, ctypes.byref(total), None, 0) == 0 and total.value in (12, 13)
  one = (ctypes.c_double * 1)()
  assert call("moon_phase_moments", 2024, 0, ctypes.byref(total), one, 1) == 1
  assert call("moon_phase_moments", 2024, 0, None, None, 0) == 0 and _binding.last_error_text()


def edge_solar_lon_root_discriminant() -> None:
  assert not call("solar_lon_root_discriminant", 2024, NAN).valid


def edge_solar_lon_roots() -> None:
  assert call("solar_lon_roots", 2024, 0.0, None, 0) == 0
  assert _binding.last_error_text() == ""
  assert call("solar_lon_roots", 2024, 0.0, None, 2) == 0


def edge_new_moons_after_jde() -> None:
  assert call("new_moons_after_jde", 2460463.0, None, 0) == 0
  assert _binding.last_error_text() == ""
  assert call("new_moons_after_jde", 2460463.0, None, 3) == 0


def edge_new_moons_in_year() -> None:
  total = ctypes.c_uint32()
  assert call("new_moons_in_year", 2024, ctypes.byref(total), None, 0) == 0 and total.value in (12, 13)
  sentinel = ctypes.c_uint32(0xDEADBEEF)
  assert call("new_moons_in_year", 0, ctypes.byref(sentinel), None, 0) == 0 and sentinel.value == 0


def edge_equation_of_time() -> None:
  assert not call("equation_of_time", NAN).valid


def edge_apparent_solar_time() -> None:
  assert not call("apparent_solar_time", 2024, 6, 1, 0.5, 200.0).valid


def edge_local_apparent_sidereal_time() -> None:
  assert not call("local_apparent_sidereal_time", 1000000.0, 0.0).valid and _binding.last_error_text()


def edge_query_jieqi_moment() -> None:
  assert not call("query_jieqi_moment", 2024, 24).valid


def edge_get_jieqi_name() -> None:
  small = ctypes.create_string_buffer(2)
  assert not call("get_jieqi_name", 0, small, len(small))
  assert not call("get_jieqi_name", 0, None, 16)


def edge_get_supported_lunar_year_range() -> None:
  assert not call("get_supported_lunar_year_range", 0).valid


def edge_get_lunar_year_info() -> None:
  assert not call("get_lunar_year_info", 9, 2024).valid


def edge_gregorian_to_lunar() -> None:
  assert not call("gregorian_to_lunar", 1, 2023, 13, 1).valid


def edge_lunar_to_gregorian() -> None:
  assert not call("lunar_to_gregorian", 1, 2024, 2, True, 1).valid


def _edge_delta_t(name: str) -> None:
  assert not call(name, NAN).valid


def edge_delta_t_algo1() -> None:
  _edge_delta_t("delta_t_algo1")


def edge_delta_t_algo2() -> None:
  _edge_delta_t("delta_t_algo2")


def edge_delta_t_algo3() -> None:
  _edge_delta_t("delta_t_algo3")


def edge_delta_t_algo4() -> None:
  _edge_delta_t("delta_t_algo4")


def edge_delta_t_algo5() -> None:
  _edge_delta_t("delta_t_algo5")


def edge_delta_t() -> None:
  _edge_delta_t("delta_t")


def edge_chart_snapshot_v1() -> None:
  function = _binding.FUNCTIONS["chart_snapshot_v1"]
  baseline = (2024, 6, 1, 0.5, 1, 35.0, 120.0, 0, 0)
  assert not call("sun_apparent_geocentric_coord", NAN).valid
  old_error = _binding.last_error_text()
  assert function(*baseline).valid and _binding.last_error_text() == ""

  invalid = (
    (0, 1884),
    (0, 2100),
    (0, -2147483648),
    (0, 2147483647),
    (0, 2024 + 65536),
    (1, 0),
    (1, 13),
    (1, 257),
    (1, 65537),
    (1, 0xFFFFFFFF),
    (2, 0),
    (2, 31),
    (2, 257),
    (2, 65537),
    (2, 0xFFFFFFFF),
    (3, -0.1),
    (3, 1.0),
    (3, NAN),
    (3, float("inf")),
    (4, 2),
    (4, 256),
    (4, 0xFFFFFFFF),
    (5, -90.0),
    (5, 90.0),
    (5, NAN),
    (5, float("inf")),
    (6, -180.1),
    (6, 180.1),
    (6, NAN),
    (6, float("inf")),
    (7, 3),
    (7, 256),
    (7, 0xFFFFFFFF),
    (8, 6),
    (8, 256),
    (8, 0xFFFFFFFF),
  )
  argument_names = (
    "year",
    "month",
    "day",
    "fraction",
    "civil_scale",
    "latitude",
    "longitude",
    "house_system",
    "delta_t_model",
  )
  for index, value in invalid:
    arguments = list(baseline)
    arguments[index] = value
    assert not function(*arguments).valid, (index, value)
    message = _binding.last_error_text()
    assert argument_names[index] in message.lower() and message != old_error, (index, value, message)
    assert function(*baseline).valid and _binding.last_error_text() == ""

  for arguments in (
    (2023, 2, 29, 0.5, 1, 35.0, 120.0, 0, 0),
    (1971, 12, 31, 0.5, 0, 35.0, 120.0, 0, 0),
    (1885, 1, 1, 0.0, 1, 35.0, 120.0, 0, 0),
    (2035, 1, 1, 0.0, 0, 35.0, 120.0, 0, 4),
    (2024, 6, 1, 0.5, 1, 80.0, 120.0, 2, 0),
  ):
    assert not function(*arguments).valid, arguments
    message = _binding.last_error_text()
    assert message and message != old_error, (arguments, message)
    assert function(*baseline).valid and _binding.last_error_text() == ""

  for arguments in (
    (1900, 6, 1, 0.5, 1, -35.0, -180.0, 1, 0),
    (1972, 1, 1, 0.0, 0, 35.0, 180.0, 2, 0),
    (2024, 2, 29, 0.0, 1, 35.0, 120.0, 0, 0),
    (2024, 6, 1, 0.5, 1, math.nextafter(-90.0, 0.0), 120.0, 0, 0),
    (2024, 6, 1, 0.5, 1, math.nextafter(90.0, 0.0), 120.0, 0, 0),
    *((2024, 6, 1, 0.5, 1, 35.0, 120.0, 0, model) for model in range(6)),
  ):
    assert function(*arguments).valid, arguments
    assert _binding.last_error_text() == ""

  # Leave a final recording failure for the inventory runner's boundary-policy check.
  assert not function(2024, 6, 1, NAN, 1, 35.0, 120.0, 0, 0).valid
  assert "fraction" in _binding.last_error_text()


EXPORT_NAMES = tuple(_binding.BINDING_SPECS)
HAPPY_TESTS = {name: globals()[f"happy_{name}"] for name in EXPORT_NAMES}
EDGE_TESTS = {name: globals()[f"edge_{name}"] for name in EXPORT_NAMES}


def run_group(label: str, tests: dict[str, object]) -> tuple[int, int]:
  """Run a named check for every export without hiding later failures."""
  passed = 0
  for name, test in tests.items():
    try:
      if label == "EDGE" and name != "last_error":
        assert call("ut1_to_jd", 2000, 1, 1, 0.5).valid
        assert _binding.last_error_text() == ""
      test()
      if label == "EDGE" and name != "last_error":
        assert _binding.last_error_text(), f"{name} did not record its failure"
    except Exception:
      print(f"FAIL {label} {name}")
      traceback.print_exc()
    else:
      print(f"PASS {label} {name}")
      passed += 1
  print(f"{label} {passed}/{len(tests)}")
  return passed, len(tests)


def main() -> None:
  """Require both fixed-denominator groups to pass in full."""
  assert len(HAPPY_TESTS) == len(EDGE_TESTS) == 30
  happy = run_group("HAPPY", HAPPY_TESTS)
  edge = run_group("EDGE", EDGE_TESTS)
  if happy[0] != happy[1] or edge[0] != edge[1]:
    raise SystemExit(1)


if __name__ == "__main__":
  main()
