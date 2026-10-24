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
import re
import traceback

from celestial_calendar import _abi, _binding


NAN = float("nan")


def call(name: str, *args: object) -> object:
  return _binding.call(name if name.startswith("celestial_") else f"celestial_{name}", *args)


def happy_celestial_last_error() -> None:
  assert call("ut1_to_jd", 2024, 6, 1, 0.5).valid
  assert _binding.last_error_text() == ""


def happy_celestial_ut1_to_jd() -> None:
  result = call("ut1_to_jd", 2024, 6, 1, 0.5)
  assert result.valid and abs(result.value - 2460463.0) < 1e-6


def happy_celestial_ut1_to_jde() -> None:
  result = call("ut1_to_jde", 2024, 6, 1, 0.5)
  assert result.valid and result.value > 2460463.0


def happy_celestial_jde_to_ut1() -> None:
  result = call("jde_to_ut1", 2460463.0)
  assert result.valid and (result.year, result.month, result.day) == (2024, 6, 1)


def happy_celestial_sun_apparent_geocentric_coord() -> None:
  result = call("sun_apparent_geocentric_coord", 2460463.0)
  assert result.valid and 0.0 <= result.lon < 360.0 and result.r > 0.0


def happy_celestial_moon_apparent_geocentric_coord() -> None:
  result = call("moon_apparent_geocentric_coord", 2460463.0)
  assert result.valid and 0.0 <= result.lon < 360.0 and result.r > 0.0


def happy_celestial_moon_illumination() -> None:
  result = call("moon_illumination", 2448724.5)
  assert result.valid and abs(result.illumination - 0.6786) < 5e-5


def happy_celestial_moon_position_angle() -> None:
  result = call("moon_position_angle", 2448724.5)
  assert result.valid and abs(result.angle_deg - 285.0) < 0.05


def happy_celestial_moon_phase_moments() -> None:
  total = ctypes.c_uint32()
  assert call("moon_phase_moments", 2024, 0, ctypes.byref(total), None, 0) == 0
  assert total.value in (12, 13)
  slots = (ctypes.c_double * total.value)()
  assert call("moon_phase_moments", 2024, 0, ctypes.byref(total), slots, total.value) == total.value


def happy_celestial_solar_lon_root_discriminant() -> None:
  result = call("solar_lon_root_discriminant", 2024, 0.0)
  assert result.valid and result.count == 1


def happy_celestial_solar_lon_roots() -> None:
  slots = (ctypes.c_double * 1)()
  assert call("solar_lon_roots", 2024, 0.0, slots, 1) == 1 and math.isfinite(slots[0])


def happy_celestial_new_moons_after_jde() -> None:
  slots = (ctypes.c_double * 3)()
  assert call("new_moons_after_jde", 2460463.0, slots, 3) == 3 and slots[0] < slots[1] < slots[2]


def happy_celestial_new_moons_in_year() -> None:
  total = ctypes.c_uint32()
  assert call("new_moons_in_year", 2024, ctypes.byref(total), None, 0) == 0
  assert total.value in (12, 13)


def happy_celestial_equation_of_time() -> None:
  result = call("equation_of_time", 2460463.0)
  assert result.valid and abs(result.value) < 5.0


def happy_celestial_apparent_solar_time() -> None:
  result = call("apparent_solar_time", 2024, 6, 1, 0.5, 116.4)
  assert result.valid and (result.year, result.month, result.day) == (2024, 6, 1)


def happy_celestial_local_apparent_sidereal_time() -> None:
  result = call("local_apparent_sidereal_time", 2460463.0, 120.0)
  assert result.valid and 0.0 <= result.value < 360.0


def happy_celestial_query_jieqi_moment() -> None:
  result = call("query_jieqi_moment", 2024, 0)
  assert result.valid and (result.jq_idx, result.y, result.m) == (0, 2024, 2)


def happy_celestial_get_jieqi_name() -> None:
  required = call("get_jieqi_name", 0, None, 0)
  assert required == 7
  buffer = ctypes.create_string_buffer(required)
  assert call("get_jieqi_name", 0, buffer, len(buffer)) == required
  assert buffer.value == "立春".encode()


def happy_celestial_get_supported_lunar_year_range() -> None:
  result = call("get_supported_lunar_year_range", 3)
  assert result.valid and (result.start, result.end) == (1600, 2199)


def happy_celestial_get_lunar_year_info() -> None:
  result = call("get_lunar_year_info", 2, 2024)
  assert result.valid and (result.year, result.month, result.day) == (2024, 2, 10)


def happy_celestial_gregorian_to_lunar() -> None:
  result = call("gregorian_to_lunar", 1, 2023, 3, 22)
  assert result.valid and (result.year, result.month, result.is_leap, result.day) == (2023, 2, True, 1)


def happy_celestial_lunar_to_gregorian() -> None:
  result = call("lunar_to_gregorian", 1, 2023, 2, True, 1)
  assert result.valid and (result.year, result.month, result.day) == (2023, 3, 22)


def happy_celestial_delta_t() -> None:
  for code in range(6):
    result = call("delta_t", 2024.5, code)
    assert result.valid and math.isfinite(result.value)
    assert result.model_identity_code == (code or 5)


def happy_celestial_chart_snapshot() -> None:
  # Direct FFI calls exercise the large by-value return without public Python validation.
  arguments = (2024, 6, 1, 0.5, 1, -33.9, 151.2, 0, 0)
  result = call("chart_snapshot", *arguments)
  assert result.valid and _binding.last_error_text() == ""
  assert result.jd_ut1 == 2460463.0 and result.jde_tt > result.jd_ut1
  assert (result.delta_t_identity_code, result.nutation_identity_code) == (5, 2)
  assert len(result.bodies) == 14 and len(result.houses.cusps_deg) == 12
  assert [body.target_code for body in result.bodies] == list(range(14))
  assert [body.present_fields for body in result.bodies] == [3] * 10 + [1] * 4
  assert [body.source_code for body in result.bodies] == [1, 2, 3, 3, 3, 3, 3, 3, 3, 4, 5, 5, 6, 6]
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

  explicit = call("chart_snapshot", *arguments[:-1], 5)
  assert explicit.valid and explicit.jd_ut1 == result.jd_ut1 and explicit.jde_tt == result.jde_tt
  assert [tuple(getattr(body, name) for name, _ in body._fields_) for body in explicit.bodies] == [
    tuple(getattr(body, name) for name, _ in body._fields_) for body in result.bodies
  ]
  assert tuple(explicit.houses.cusps_deg) == tuple(axes.cusps_deg)


def edge_celestial_last_error() -> None:
  assert not call("ut1_to_jd", 2024, 6, 1, NAN).valid
  recorded = _binding.last_error_text()
  assert recorded
  assert not call("sun_apparent_geocentric_coord", NAN).valid
  assert _binding.last_error_text() and _binding.last_error_text() != recorded
  assert call("ut1_to_jd", 2024, 6, 1, 0.5).valid
  assert _binding.last_error_text() == ""


def edge_celestial_ut1_to_jd() -> None:
  assert not call("ut1_to_jd", 2024, 6, 1, NAN).valid and _binding.last_error_text()


def edge_celestial_ut1_to_jde() -> None:
  assert not call("ut1_to_jde", 2024, 13, 1, 0.5).valid and _binding.last_error_text()


def edge_celestial_jde_to_ut1() -> None:
  assert not call("jde_to_ut1", NAN).valid and _binding.last_error_text()


def edge_celestial_sun_apparent_geocentric_coord() -> None:
  assert not call("sun_apparent_geocentric_coord", NAN).valid


def edge_celestial_moon_apparent_geocentric_coord() -> None:
  assert not call("moon_apparent_geocentric_coord", NAN).valid


def edge_celestial_moon_illumination() -> None:
  assert not call("moon_illumination", NAN).valid and _binding.last_error_text()


def edge_celestial_moon_position_angle() -> None:
  assert not call("moon_position_angle", NAN).valid and _binding.last_error_text()


def edge_celestial_moon_phase_moments() -> None:
  total = ctypes.c_uint32()
  assert call("moon_phase_moments", 2024, 0, ctypes.byref(total), None, 0) == 0 and total.value in (12, 13)
  one = (ctypes.c_double * 1)()
  assert call("moon_phase_moments", 2024, 0, ctypes.byref(total), one, 1) == 1
  assert call("moon_phase_moments", 2024, 0, None, None, 0) == 0 and _binding.last_error_text()


def edge_celestial_solar_lon_root_discriminant() -> None:
  assert not call("solar_lon_root_discriminant", 2024, NAN).valid


def edge_celestial_solar_lon_roots() -> None:
  assert call("solar_lon_roots", 2024, 0.0, None, 0) == 0
  assert _binding.last_error_text() == ""
  assert call("solar_lon_roots", 2024, 0.0, None, 2) == 0


def edge_celestial_new_moons_after_jde() -> None:
  assert call("new_moons_after_jde", 2460463.0, None, 0) == 0
  assert _binding.last_error_text() == ""
  assert call("new_moons_after_jde", 2460463.0, None, 3) == 0


def edge_celestial_new_moons_in_year() -> None:
  total = ctypes.c_uint32()
  assert call("new_moons_in_year", 2024, ctypes.byref(total), None, 0) == 0 and total.value in (12, 13)
  sentinel = ctypes.c_uint32(0xDEADBEEF)
  assert call("new_moons_in_year", 0, ctypes.byref(sentinel), None, 0) == 0 and sentinel.value == 0


def edge_celestial_equation_of_time() -> None:
  assert not call("equation_of_time", NAN).valid


def edge_celestial_apparent_solar_time() -> None:
  assert not call("apparent_solar_time", 2024, 6, 1, 0.5, 200.0).valid


def edge_celestial_local_apparent_sidereal_time() -> None:
  assert not call("local_apparent_sidereal_time", 1000000.0, 0.0).valid and _binding.last_error_text()


def edge_celestial_query_jieqi_moment() -> None:
  assert not call("query_jieqi_moment", 2024, 24).valid


def edge_celestial_get_jieqi_name() -> None:
  small = ctypes.create_string_buffer(2)
  assert call("get_jieqi_name", 0, small, len(small)) == 7 and small.raw[-1:] == b"\0"
  assert call("get_jieqi_name", 0, None, 16) == 0


def edge_celestial_get_supported_lunar_year_range() -> None:
  assert not call("get_supported_lunar_year_range", 0).valid


def edge_celestial_get_lunar_year_info() -> None:
  assert not call("get_lunar_year_info", 9, 2024).valid


def edge_celestial_gregorian_to_lunar() -> None:
  assert not call("gregorian_to_lunar", 1, 2023, 13, 1).valid


def edge_celestial_lunar_to_gregorian() -> None:
  assert not call("lunar_to_gregorian", 1, 2024, 2, True, 1).valid


def edge_celestial_delta_t() -> None:
  for code in range(6):
    assert not call("delta_t", NAN, code).valid
    assert call("last_error_kind") == 1
  for year, model in ((-4001.0, 1), (3000.0, 3), (2035.0, 4), (2024.5, 256), (2024.5, 0xFFFFFFFF)):
    assert not call("delta_t", year, model).valid
    assert call("last_error_kind") == 1


def edge_celestial_chart_snapshot() -> None:
  function = _binding.FUNCTIONS["celestial_chart_snapshot"]
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
    assert call("last_error_kind") == 1 and message and message != old_error, (index, value, message)
    if index == 8:
      assert "code" in message.lower() and str(value) in message, (index, value, message)
    else:
      assert argument_names[index] in message.lower(), (index, value, message)
    assert function(*baseline).valid and call("last_error_kind") == 0 and _binding.last_error_text() == ""

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


def happy_celestial_longitude_events() -> None:
  total = ctypes.c_uint32()
  record = _abi.CelestialLongitudeEventRecord
  assert call("longitude_events", 0, 0.0, 2460735.5, 2460785.5, ctypes.byref(total), None, 0) == 0
  assert total.value == 1
  slots = (record * total.value)()
  assert call("longitude_events", 0, 0.0, 2460735.5, 2460785.5, ctypes.byref(total), slots, total.value) == 1
  assert slots[0].direction_code == 0 and 0.0 <= slots[0].longitude_deg < 360.0


def happy_celestial_longitude_stations() -> None:
  total = ctypes.c_uint32()
  record = _abi.CelestialStationEventRecord
  assert call("longitude_stations", 2, 2460735.5, 2460785.5, ctypes.byref(total), None, 0) == 0
  assert total.value == 2
  slots = (record * total.value)()
  assert call("longitude_stations", 2, 2460735.5, 2460785.5, ctypes.byref(total), slots, total.value) == 2
  motions = {
    (slots[0].motion_before_code, slots[0].motion_after_code),
    (slots[1].motion_before_code, slots[1].motion_after_code),
  }
  assert motions == {(0, 1), (1, 0)}


def happy_celestial_longitude_difference_events() -> None:
  total = ctypes.c_uint32()
  record = _abi.CelestialLongitudeDifferenceEventRecord
  assert call("longitude_difference_events", 1, 0, -60.0, 2460676.5, 2460707.5, ctypes.byref(total), None, 0) == 0
  assert total.value == 1
  slots = (record * total.value)()
  assert (
    call("longitude_difference_events", 1, 0, -60.0, 2460676.5, 2460707.5, ctypes.byref(total), slots, total.value) == 1
  )
  assert abs(slots[0].signed_difference_deg + 60.0) < 1e-6


def edge_celestial_longitude_events() -> None:
  total = ctypes.c_uint32()
  # An empty result is legal and clears the recorded error.
  assert call("longitude_events", 3, 137.5, 2460735.5, 2460785.5, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and _binding.last_error_text() == ""
  # Truncation keeps the full total observable.
  assert call("longitude_events", 2, 0.0, 2460735.5, 2460785.5, ctypes.byref(total), None, 0) == 0
  assert total.value == 3
  one = (_abi.CelestialLongitudeEventRecord * 1)()
  written = call("longitude_events", 2, 0.0, 2460735.5, 2460785.5, ctypes.byref(total), one, 1)
  assert written == 1 and total.value == 3
  # A rejected query resets the out-parameter and records why.
  sentinel = ctypes.c_uint32(0xDEADBEEF)
  assert call("longitude_events", 2, NAN, 2460735.5, 2460785.5, ctypes.byref(sentinel), None, 0) == 0
  assert sentinel.value == 0 and "target_longitude_deg" in _binding.last_error_text()


def edge_celestial_longitude_stations() -> None:
  total = ctypes.c_uint32()
  # The Sun's certified rate bounds exclude a station: a legal empty result.
  assert call("longitude_stations", 0, 2460735.5, 2460785.5, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and _binding.last_error_text() == ""
  sentinel = ctypes.c_uint32(0xDEADBEEF)
  assert call("longitude_stations", 0, 2460785.5, 2460735.5, ctypes.byref(sentinel), None, 0) == 0
  assert sentinel.value == 0 and "nonempty" in _binding.last_error_text()


def edge_celestial_longitude_difference_events() -> None:
  total = ctypes.c_uint32()
  # The antipodal node relation holds throughout at the opposition level.
  sentinel = ctypes.c_uint32(0xDEADBEEF)
  assert call("longitude_difference_events", 10, 11, 180.0, 2460676.5, 2460707.5, ctypes.byref(sentinel), None, 0) == 0
  assert sentinel.value == 0 and "throughout" in _binding.last_error_text()
  # A constant mismatch is a legal empty result and clears the recorded error.
  assert call("longitude_difference_events", 10, 11, 0.0, 2460676.5, 2460707.5, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and _binding.last_error_text() == ""
  # Self-pairs are rejected before any provider work.
  assert call("longitude_difference_events", 0, 0, 1.0, 2460676.5, 2460707.5, ctypes.byref(sentinel), None, 0) == 0
  assert sentinel.value == 0 and "distinct" in _binding.last_error_text()


def happy_celestial_capabilities() -> None:
  total = ctypes.c_uint32()
  record = _abi.CelestialCapabilityRecord
  assert call("celestial_capabilities", 2, 0xFFFFFFFF, ctypes.byref(total), None, 0) == 0
  assert total.value == 14
  slots = (record * total.value)()
  assert call("celestial_capabilities", 2, 0xFFFFFFFF, ctypes.byref(total), slots, total.value) == 14
  sun = next(slot for slot in slots if slot.subject_code == 0)
  assert sun.operation_code == 2 and sun.maximum_span_tt_days == 3660
  pluto = next(slot for slot in slots if slot.subject_code == 9)
  # The center domain is the root window shrunk by the stencil reach, so the status is an
  # implementation window even though Pluto's position root window is source-published.
  assert pluto.status_code == 1 and pluto.domain_start_jde_tt == 2409543.875


def edge_celestial_capabilities() -> None:
  total = ctypes.c_uint32()
  # Unknown operations and subjects are rejected before any record is built.
  assert call("celestial_capabilities", 9, 0xFFFFFFFF, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and "operation" in _binding.last_error_text().lower()
  assert call("celestial_capabilities", 0, 14, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and call("last_error_kind") == 1
  assert "code 14" in _binding.last_error_text()
  # HOUSES has a civil-policy result, not a fabricated TT interval.
  assert call("celestial_capabilities", 3, 7, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and call("last_error_kind") == 1
  # The group runner expects a recorded failure at exit.
  assert call("celestial_capabilities", 5, 0xFFFFFFFF, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and call("last_error_kind") == 1


def happy_celestial_model_identities() -> None:
  total = ctypes.c_uint32()
  record = _abi.CelestialModelIdentityRecord
  assert call("celestial_model_identities", 0xFFFFFFFF, ctypes.byref(total), None, 0) == 0
  assert total.value == 13
  slots = (record * total.value)()
  assert call("celestial_model_identities", 0xFFFFFFFF, ctypes.byref(total), slots, total.value) == 13
  assert slots[4].kind_code == 0 and slots[4].identity_code == 5
  assert slots[3].kind_code == 0 and slots[3].status_code == 1
  assert all(slot.kind_code == 1 for slot in slots[5:7])


def edge_celestial_model_identities() -> None:
  total = ctypes.c_uint32()
  # Unknown kinds are rejected; a kind filter keeps the registry counts.
  assert call("celestial_model_identities", 9, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and call("last_error_kind") == 1
  assert "model kind 9" in _binding.last_error_text()
  assert call("celestial_model_identities", 0, ctypes.byref(total), None, 0) == 0
  assert total.value == 5
  # The group runner expects a recorded failure at exit.
  assert call("celestial_model_identities", 7, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and call("last_error_kind") == 1
  assert "model kind 7" in _binding.last_error_text()


def happy_celestial_capability_text() -> None:
  required = call("celestial_capability_text", 0, 9, None, 0)
  assert required > 0 and _binding.last_error_text() == ""
  buffer = (ctypes.c_char * required)()
  assert call("celestial_capability_text", 0, 9, buffer, required) == required
  assert buffer.raw[-1:] == b"\0" and len(buffer.value) + 1 == required
  text = buffer.value.decode("utf-8")
  assert "Pluto" in text and "Meeus Chapter 37" in text
  assert "PLUTO95 supplies independent references, not the runtime evaluator" in text


def edge_celestial_capability_text() -> None:
  # EQUATORIAL has target-domain records; identities have a separate text entry point.
  total = ctypes.c_uint32()
  assert call("celestial_capability_text", 4, 4, None, 0) > 0
  assert call("celestial_capabilities", 4, 0xFFFFFFFF, ctypes.byref(total), None, 0) == 0
  assert total.value == 14 and call("last_error_kind") == 0
  # A null buffer with positive capacity is an argument error, not a crash.
  assert call("celestial_capability_text", 0, 0, None, 8) == 0
  assert "buffer" in _binding.last_error_text()
  # Out-of-range subjects are rejected before narrowing, never truncated into another subject.
  assert call("celestial_capability_text", 0, 256, None, 0) == 0
  assert call("last_error_kind") == 1 and "code 256" in _binding.last_error_text()
  assert call("celestial_capability_text", 4, 256, None, 0) == 0
  assert call("last_error_kind") == 1
  assert call("celestial_capability_text", 0, 14, None, 0) == 0
  assert call("last_error_kind") == 1 and "code 14" in _binding.last_error_text()


EXPORT_LABELS = (
  "last_error",
  "last_error_kind",
  "ut1_to_jd",
  "ut1_to_jde",
  "jde_to_ut1",
  "sun_apparent_geocentric_coord",
  "moon_apparent_geocentric_coord",
  "moon_illumination",
  "moon_position_angle",
  "moon_phase_moments",
  "solar_lon_root_discriminant",
  "solar_lon_roots",
  "new_moons_after_jde",
  "new_moons_in_year",
  "equation_of_time",
  "apparent_solar_time",
  "local_apparent_sidereal_time",
  "query_jieqi_moment",
  "get_jieqi_name",
  "get_supported_lunar_year_range",
  "get_lunar_year_info",
  "gregorian_to_lunar",
  "lunar_to_gregorian",
  "delta_t",
  "chart_snapshot",
  "longitude_events",
  "longitude_stations",
  "longitude_difference_events",
  "capabilities",
  "model_identities",
  "capability_text",
  "model_identity_text",
  "positions",
  "equatorial_positions",
  "house_query",
  "house_capability",
)


def happy_celestial_positions() -> None:
  # Direct FFI calls exercise the caller-ordered count+fill selection without public validation.
  record = _abi.CelestialPositionRecord
  selection = (ctypes.c_uint32 * 3)(9, 0, 10)
  total = ctypes.c_uint32()
  assert call("positions", selection, 3, 2451545.0, ctypes.byref(total), None, 0) == 0
  assert total.value == 3
  slots = (record * total.value)()
  assert call("positions", selection, 3, 2451545.0, ctypes.byref(total), slots, total.value) == 3
  assert [slot.target_code for slot in slots] == [9, 0, 10]
  assert (slots[0].present_fields & 2) and not (slots[2].present_fields & 2)
  assert slots[2].source_code == 5  # CELESTIAL_POSITION_SOURCE_NODE_MEAN
  assert not (slots[0].present_fields & 2) or math.isfinite(slots[0].distance_au)
  for slot in slots:
    assert math.isfinite(slot.longitude_deg) and 0.0 <= slot.longitude_deg < 360.0
    assert math.isfinite(slot.latitude_deg)

  # A subset succeeds at Pluto's provider window start, below the snapshot joint window.
  solo = (ctypes.c_uint32 * 1)(9)
  assert call("positions", solo, 1, 2409543.5, ctypes.byref(total), slots, 1) == 1
  assert total.value == 1


def edge_celestial_positions() -> None:
  record = _abi.CelestialPositionRecord
  selection = (ctypes.c_uint32 * 1)(0)
  total = ctypes.c_uint32()
  slots = (record * 1)()
  # Empty selections, duplicates, unknown codes and non-finite times are rejected up front.
  assert call("positions", selection, 0, 2451545.0, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and "target_count" in _binding.last_error_text()
  duplicated = (ctypes.c_uint32 * 2)(0, 0)
  assert call("positions", duplicated, 2, 2451545.0, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and "duplicates" in _binding.last_error_text()
  unknown = (ctypes.c_uint32 * 1)(14)
  assert call("positions", unknown, 1, 2451545.0, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and call("last_error_kind") == 1
  assert "code 14" in _binding.last_error_text()
  assert call("positions", selection, 1, NAN, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and "jde_tt" in _binding.last_error_text()
  # A failing target fails the whole query with no partial list.
  pluto = (ctypes.c_uint32 * 1)(9)
  assert call("positions", pluto, 1, 2409543.4, ctypes.byref(total), slots, 1) == 0
  assert total.value == 0 and _binding.last_error_text() != ""
  # The group runner expects a recorded failure at exit.
  assert call("positions", None, 1, 2451545.0, ctypes.byref(total), None, 0) == 0
  assert total.value == 0 and "target_codes" in _binding.last_error_text()


def happy_celestial_house_query() -> None:
  # Direct FFI calls exercise the by-value result with resolved identities.
  result = call("house_query", 2024, 6, 1, 0.5, 1, 0, 0, 51.5, 0.0, 2)
  assert result.valid and _binding.last_error_text() == ""
  assert result.house_system_code == 2 and result.delta_t_identity_code == 5
  assert result.nutation_identity_code == 2  # IAU 1980
  assert result.jde_tt > result.jd_ut1
  assert len(result.houses.cusps_deg) == 12
  for angle in (
    result.houses.ascendant_deg,
    result.houses.midheaven_deg,
    result.houses.descendant_deg,
    result.houses.imum_coeli_deg,
    *result.houses.cusps_deg,
  ):
    assert math.isfinite(angle) and 0.0 <= angle < 360.0

  # The snapshot joint window does not apply: a pre-window civil instant resolves.
  early = call("house_query", 1885, 1, 1, 0.0, 1, 5, 1, 51.5, 0.0, 0)
  assert early.valid and early.jde_tt < 2409543.875 and early.nutation_identity_code == 1  # MEEUS


def edge_celestial_house_query() -> None:
  function = _binding.FUNCTIONS["celestial_house_query"]
  baseline = (2024, 6, 1, 0.5, 1, 0, 0, 51.5, 0.0, 2)
  assert function(*baseline).valid and _binding.last_error_text() == ""
  # Unknown model codes are rejected before any house geometry is attempted.
  for position in (5, 6):
    arguments = list(baseline)
    arguments[position] = 9
    assert not function(*arguments).valid
    assert _binding.last_error_text() != ""
  # Polar Placidus fails by-value; a position query at the same instant still succeeds.
  assert not function(2024, 6, 1, 0.5, 1, 0, 0, 89.9, 0.0, 2).valid
  assert function(*baseline).valid
  # The group runner expects a recorded failure at exit.
  assert not function(1884, 6, 1, 0.5, 1, 0, 0, 51.5, 0.0, 2).valid
  assert "year" in _binding.last_error_text()


def happy_celestial_last_error_kind() -> None:
  happy_celestial_last_error()
  assert call("last_error_kind") == 0


def edge_celestial_last_error_kind() -> None:
  assert not call("ut1_to_jd", 2000, 1, 1, NAN).valid
  for _ in range(2):
    assert call("last_error_kind") == 1
    message = _binding.last_error_text()
    assert message and call("last_error_kind") == 1 and _binding.last_error_text() == message


def happy_celestial_model_identity_text() -> None:
  names = (
    (("algo1",), ("algo2",), ("algo3",), ("algo4",), ("algo5",)),
    (("meeus",), ("iau1980",)),
    (
      ("sun", "vsop87d"),
      ("moon", "elp2000"),
      ("planet", "vsop87d"),
      ("pluto95",),
      ("mean", "node"),
      ("true", "meeus", "five"),
    ),
  )
  for kind, identities in ((0, range(1, 6)), (1, range(1, 3)), (2, range(1, 7))):
    for identity in identities:
      required = call("model_identity_text", kind, identity, None, 0)
      assert required > 1 and call("last_error_kind") == 0
      buffer = (ctypes.c_char * required)()
      assert call("model_identity_text", kind, identity, buffer, required) == required
      assert buffer.raw[-1:] == b"\0" and len(buffer.value) + 1 == required
      text = re.sub(r"[^a-z0-9]", "", buffer.value.decode("utf-8").lower())
      assert all(name in text for name in names[kind][identity - 1]), (kind, identity, text)


def edge_celestial_model_identity_text() -> None:
  for kind, identity in ((0, 0), (0, 6), (1, 3), (2, 7), (3, 1), (0xFFFFFFFF, 1), (256, 1)):
    assert call("model_identity_text", kind, identity, None, 0) == 0
    assert call("last_error_kind") == 1
  one = (ctypes.c_char * 1)()
  assert call("model_identity_text", 0, 5, one, 1) > 1 and one.raw == b"\0"
  assert call("model_identity_text", 0, 5, None, 1) == 0 and call("last_error_kind") == 1


def happy_celestial_equatorial_positions() -> None:
  selection = (ctypes.c_uint32 * 14)(*reversed(range(14)))
  total = ctypes.c_uint32()
  assert call("equatorial_positions", selection, 14, 2451545.0, ctypes.byref(total), None, 0) == 0
  assert total.value == 14 and call("last_error_kind") == 0
  slots = (_abi.CelestialEquatorialPositionRecord * 14)()
  assert call("equatorial_positions", selection, 14, 2451545.0, ctypes.byref(total), slots, 14) == 14
  assert total.value == 14 and [slot.target_code for slot in slots] == list(reversed(range(14)))
  for slot in slots:
    assert 0.0 <= slot.right_ascension_deg < 360.0 and -90.0 <= slot.declination_deg <= 90.0
    assert 1 <= slot.source_code <= 6
    assert slot.present_fields == (0 if slot.target_code >= 10 else 2)
    if slot.target_code < 10:
      assert math.isfinite(slot.distance_au) and slot.distance_au > 0.0


def edge_celestial_equatorial_positions() -> None:
  total = ctypes.c_uint32(123)
  sun = (ctypes.c_uint32 * 1)(0)
  for selection, count, jde in (
    (sun, 0, 2451545.0),
    (None, 1, 2451545.0),
    ((ctypes.c_uint32 * 2)(0, 0), 2, 2451545.0),
    ((ctypes.c_uint32 * 1)(256), 1, 2451545.0),
    (sun, 1, NAN),
  ):
    assert call("equatorial_positions", selection, count, jde, ctypes.byref(total), None, 0) == 0
    assert total.value == 0 and call("last_error_kind") == 1


def happy_celestial_house_capability() -> None:
  for model in range(6):
    result = call("house_capability", model)
    assert result.valid and result.status_code == 1
    assert (result.civil_start_year, result.civil_end_year_exclusive) == (1885, 2100)
    assert result.civil_scale_mask == 3
    assert (result.utc_start_year, result.utc_start_month, result.utc_start_day) == (1972, 1, 1)
    assert result.delta_t_identity_code == (model or 5)
    assert result.has_derived_tt_cutoff is (model == 4)
    if model == 4:
      assert result.derived_tt_end_jde_tt == 2464328.5


def edge_celestial_house_capability() -> None:
  for model in (6, 256, 0xFFFFFFFF):
    assert not call("house_capability", model).valid and call("last_error_kind") == 1


def protocol_test(prefix: str, label: str):
  return globals()[f"{prefix}_celestial_{label}"]


HAPPY_TESTS = {f"celestial_{label}": protocol_test("happy", label) for label in EXPORT_LABELS}
EDGE_TESTS = {f"celestial_{label}": protocol_test("edge", label) for label in EXPORT_LABELS}


def run_group(label: str, tests: dict[str, object]) -> tuple[int, int]:
  """Run a named check for every export without hiding later failures."""
  passed = 0
  for name, test in tests.items():
    try:
      if label == "EDGE" and name not in {"celestial_last_error", "celestial_last_error_kind"}:
        assert call("ut1_to_jd", 2000, 1, 1, 0.5).valid
        assert _binding.last_error_text() == ""
      test()
      if label == "EDGE" and name not in {"celestial_last_error", "celestial_last_error_kind"}:
        assert call("last_error_kind") != 0, f"{name} did not record its failure"
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
  assert len(HAPPY_TESTS) == len(EDGE_TESTS) == 36
  assert set(HAPPY_TESTS) == set(EDGE_TESTS) == set(_binding.BINDING_SPECS)
  happy = run_group("HAPPY", HAPPY_TESTS)
  edge = run_group("EDGE", EDGE_TESTS)
  if happy[0] != happy[1] or edge[0] != edge[1]:
    raise SystemExit(1)


if __name__ == "__main__":
  main()
