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
  assert call("solar_lon_roots", 2024, 0.0, None, 2) == 0


def edge_new_moons_after_jde() -> None:
  assert call("new_moons_after_jde", 2460463.0, None, 0) == 0
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
  assert len(HAPPY_TESTS) == len(EDGE_TESTS) == 29
  happy = run_group("HAPPY", HAPPY_TESTS)
  edge = run_group("EDGE", EDGE_TESTS)
  if happy[0] != happy[1] or edge[0] != edge[1]:
    raise SystemExit(1)


if __name__ == "__main__":
  main()
