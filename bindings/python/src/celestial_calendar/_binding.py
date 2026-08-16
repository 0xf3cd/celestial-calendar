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

"""Private ctypes declarations for the CelestialCalendar C ABI."""

from __future__ import annotations

import atexit
import ctypes
import sys
from importlib import resources


class JulianDay(ctypes.Structure):
  _fields_ = [("valid", ctypes.c_bool), ("value", ctypes.c_double)]


class UT1Time(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint32),
    ("day", ctypes.c_uint32),
    ("fraction", ctypes.c_double),
  ]


class SunCoordinate(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("lon", ctypes.c_double),
    ("lat", ctypes.c_double),
    ("r", ctypes.c_double),
  ]


class MoonCoordinate(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("lon", ctypes.c_double),
    ("lat", ctypes.c_double),
    ("r", ctypes.c_double),
  ]


class MoonIllumination(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("illumination", ctypes.c_double),
    ("elongation_deg", ctypes.c_double),
  ]


class MoonPositionAngle(ctypes.Structure):
  _fields_ = [("valid", ctypes.c_bool), ("angle_deg", ctypes.c_double)]


class Discriminant(ctypes.Structure):
  _fields_ = [("valid", ctypes.c_bool), ("count", ctypes.c_uint32)]


class EquationOfTime(ctypes.Structure):
  _fields_ = [("valid", ctypes.c_bool), ("value", ctypes.c_double)]


class ApparentSolarTime(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint32),
    ("day", ctypes.c_uint32),
    ("fraction", ctypes.c_double),
  ]


class SiderealTime(ctypes.Structure):
  _fields_ = [("valid", ctypes.c_bool), ("value", ctypes.c_double)]


class JieqiMomentQuery(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("jq_idx", ctypes.c_uint8),
    ("y", ctypes.c_int32),
    ("m", ctypes.c_uint32),
    ("d", ctypes.c_uint32),
    ("frac", ctypes.c_double),
  ]


class SupportedLunarYearRange(ctypes.Structure):
  _fields_ = [("valid", ctypes.c_bool), ("start", ctypes.c_int32), ("end", ctypes.c_int32)]


class LunarYearInfo(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint8),
    ("day", ctypes.c_uint8),
    ("leap_month", ctypes.c_uint8),
    ("month_len", ctypes.c_uint16),
  ]


class LunarDate(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint8),
    ("is_leap", ctypes.c_bool),
    ("day", ctypes.c_uint8),
  ]


class GregorianDate(ctypes.Structure):
  _fields_ = [
    ("valid", ctypes.c_bool),
    ("year", ctypes.c_int32),
    ("month", ctypes.c_uint8),
    ("day", ctypes.c_uint8),
  ]


class DeltaT(ctypes.Structure):
  _fields_ = [("valid", ctypes.c_bool), ("value", ctypes.c_double)]


P_U32 = ctypes.POINTER(ctypes.c_uint32)
P_DOUBLE = ctypes.POINTER(ctypes.c_double)
P_CHAR = ctypes.POINTER(ctypes.c_char)

BINDING_SPECS = {
  "set_log_verbosity": ((ctypes.c_uint8,), ctypes.c_bool),
  "last_error": ((), ctypes.c_char_p),
  "ut1_to_jd": ((ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_double), JulianDay),
  "ut1_to_jde": ((ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_double), JulianDay),
  "jde_to_ut1": ((ctypes.c_double,), UT1Time),
  "sun_apparent_geocentric_coord": ((ctypes.c_double,), SunCoordinate),
  "moon_apparent_geocentric_coord": ((ctypes.c_double,), MoonCoordinate),
  "moon_illumination": ((ctypes.c_double,), MoonIllumination),
  "moon_position_angle": ((ctypes.c_double,), MoonPositionAngle),
  "moon_phase_moments": ((ctypes.c_int32, ctypes.c_uint8, P_U32, P_DOUBLE, ctypes.c_uint32), ctypes.c_uint32),
  "solar_lon_root_discriminant": ((ctypes.c_int32, ctypes.c_double), Discriminant),
  "solar_lon_roots": ((ctypes.c_int32, ctypes.c_double, P_DOUBLE, ctypes.c_uint32), ctypes.c_uint32),
  "new_moons_after_jde": ((ctypes.c_double, P_DOUBLE, ctypes.c_uint32), ctypes.c_uint32),
  "new_moons_in_year": ((ctypes.c_int32, P_U32, P_DOUBLE, ctypes.c_uint32), ctypes.c_uint32),
  "equation_of_time": ((ctypes.c_double,), EquationOfTime),
  "apparent_solar_time": (
    (ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_double, ctypes.c_double),
    ApparentSolarTime,
  ),
  "local_apparent_sidereal_time": ((ctypes.c_double, ctypes.c_double), SiderealTime),
  "query_jieqi_moment": ((ctypes.c_int32, ctypes.c_uint8), JieqiMomentQuery),
  "get_jieqi_name": ((ctypes.c_uint8, P_CHAR, ctypes.c_uint32), ctypes.c_bool),
  "get_supported_lunar_year_range": ((ctypes.c_uint8,), SupportedLunarYearRange),
  "get_lunar_year_info": ((ctypes.c_uint8, ctypes.c_int32), LunarYearInfo),
  "gregorian_to_lunar": ((ctypes.c_uint8, ctypes.c_int32, ctypes.c_uint8, ctypes.c_uint8), LunarDate),
  "lunar_to_gregorian": (
    (ctypes.c_uint8, ctypes.c_int32, ctypes.c_uint8, ctypes.c_bool, ctypes.c_uint8),
    GregorianDate,
  ),
  "delta_t_algo1": ((ctypes.c_double,), DeltaT),
  "delta_t_algo2": ((ctypes.c_double,), DeltaT),
  "delta_t_algo3": ((ctypes.c_double,), DeltaT),
  "delta_t_algo4": ((ctypes.c_double,), DeltaT),
  "delta_t_algo5": ((ctypes.c_double,), DeltaT),
  "delta_t": ((ctypes.c_double,), DeltaT),
}

STRUCT_TYPES = {
  structure.__name__: structure
  for structure in (
    JulianDay,
    UT1Time,
    SunCoordinate,
    MoonCoordinate,
    MoonIllumination,
    MoonPositionAngle,
    Discriminant,
    EquationOfTime,
    ApparentSolarTime,
    SiderealTime,
    JieqiMomentQuery,
    SupportedLunarYearRange,
    LunarYearInfo,
    LunarDate,
    GregorianDate,
    DeltaT,
  )
}

RECORDING_EXPORTS = frozenset(
  {
    "ut1_to_jd",
    "ut1_to_jde",
    "jde_to_ut1",
    "moon_illumination",
    "moon_position_angle",
    "moon_phase_moments",
    "local_apparent_sidereal_time",
  }
)


def _native_filename() -> str:
  if sys.platform == "win32":
    return "_celestial_calendar.dll"
  if sys.platform == "darwin":
    return "_celestial_calendar.dylib"
  if sys.platform.startswith("linux"):
    return "_celestial_calendar.so"
  raise ImportError(f"celestial_calendar does not support platform {sys.platform!r}")


_RESOURCE_CONTEXT = resources.as_file(resources.files(__package__) / "_native" / _native_filename())
_LIBRARY_PATH = _RESOURCE_CONTEXT.__enter__()
atexit.register(_RESOURCE_CONTEXT.__exit__, None, None, None)

LIB = ctypes.CDLL(str(_LIBRARY_PATH))
FUNCTIONS = {}
for _name, (_argtypes, _restype) in BINDING_SPECS.items():
  _function = getattr(LIB, _name)
  _function.argtypes = _argtypes
  _function.restype = _restype
  FUNCTIONS[_name] = _function


def call(name: str, *args: object) -> object:
  """Call one replaceable private binding."""
  return FUNCTIONS[name](*args)


def last_error_text() -> str:
  """Read the current thread's native error without exposing its pointer."""
  message = FUNCTIONS["last_error"]()
  return "" if message is None else message.decode("utf-8", errors="replace")
