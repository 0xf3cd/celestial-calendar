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

"""Astronomical calculations and Chinese calendar conversion."""

import ctypes as _ctypes
import math as _math
from dataclasses import dataclass as _dataclass
from enum import IntEnum as _IntEnum
from enum import StrEnum as _StrEnum
from typing import TypeVar as _TypeVar

from . import _binding
from ._version import VERSION as __version__


class LogVerbosity(_StrEnum):
  """Native logging verbosity."""

  NONE = "none"
  INFO = "info"
  DEBUG = "debug"


class MoonPhase(_StrEnum):
  """One of the four principal Moon phases."""

  NEW = "new"
  FIRST_QUARTER = "first_quarter"
  FULL = "full"
  LAST_QUARTER = "last_quarter"


class DeltaTModel(_StrEnum):
  """A selectable ΔT model."""

  DEFAULT = "default"
  ALGO1 = "algo1"
  ALGO2 = "algo2"
  ALGO3 = "algo3"
  ALGO4 = "algo4"
  ALGO5 = "algo5"


class LunarAlgorithm(_StrEnum):
  """A Chinese Lunar calendar conversion algorithm."""

  ALGO1 = "algo1"
  ALGO2 = "algo2"
  ALGO3 = "algo3"


class Jieqi(_IntEnum):
  """The 24 Chinese Jieqi (节气), indexed from Lichun (立春)."""

  LICHUN = 0
  YUSHUI = 1
  JINGZHE = 2
  CHUNFEN = 3
  QINGMING = 4
  GUYU = 5
  LIXIA = 6
  XIAOMAN = 7
  MANGZHONG = 8
  XIAZHI = 9
  XIAOSHU = 10
  DASHU = 11
  LIQIU = 12
  CHUSHU = 13
  BAILU = 14
  QIUFEN = 15
  HANLU = 16
  SHUANGJIANG = 17
  LIDONG = 18
  XIAOXUE = 19
  DAXUE = 20
  DONGZHI = 21
  XIAOHAN = 22
  DAHAN = 23


@_dataclass(frozen=True)
class CivilDateTime:
  """A civil date and fraction of its day; the function names identify its time scale."""

  year: int
  month: int
  day: int
  fraction: float


@_dataclass(frozen=True)
class GregorianDate:
  """A proleptic Gregorian calendar date."""

  year: int
  month: int
  day: int


@_dataclass(frozen=True)
class LunarDate:
  """A Chinese Lunar date using traditional month numbering."""

  year: int
  month: int
  day: int
  is_leap: bool


@_dataclass(frozen=True)
class SunCoordinate:
  """Apparent geocentric ecliptic coordinates of the Sun."""

  longitude_deg: float
  latitude_deg: float
  radius_au: float


@_dataclass(frozen=True)
class MoonCoordinate:
  """Apparent geocentric ecliptic coordinates of the Moon."""

  longitude_deg: float
  latitude_deg: float
  distance_km: float


@_dataclass(frozen=True)
class MoonIllumination:
  """The illuminated fraction and Moon-Sun elongation."""

  fraction: float
  elongation_deg: float


@_dataclass(frozen=True)
class JieqiMoment:
  """The UT1 civil moment of a Jieqi."""

  jieqi: Jieqi
  moment_ut1: CivilDateTime


@_dataclass(frozen=True)
class LunarYearRange:
  """Inclusive Lunar year range supported by an algorithm."""

  start: int
  end: int


@_dataclass(frozen=True)
class LunarYearInfo:
  """The Gregorian first day and month structure of a Lunar year."""

  first_day: GregorianDate
  leap_month: int | None
  month_lengths: tuple[int, ...]


class CelestialError(RuntimeError):
  """A failure reported by the native CelestialCalendar boundary."""

  def __init__(self, operation: str, message: str, *, recorded: bool) -> None:
    super().__init__(message)
    self.operation = operation
    self.recorded = recorded


_EnumT = _TypeVar("_EnumT", bound=_IntEnum | _StrEnum)
_MAX_CIVIL_YEAR = 32_767
_MAX_CALENDAR_YEAR = 32_766
_MIN_JIEQI_YEAR = 401
_MAX_NEW_MOON_COUNT = 4_096
_JIEQI_NAME_BYTES = 16
_LOG_VERBOSITY = {LogVerbosity.NONE: 0, LogVerbosity.INFO: 1, LogVerbosity.DEBUG: 2}
_MOON_PHASE = {MoonPhase.NEW: 0, MoonPhase.FIRST_QUARTER: 1, MoonPhase.FULL: 2, MoonPhase.LAST_QUARTER: 3}
_LUNAR_ALGORITHM = {LunarAlgorithm.ALGO1: 1, LunarAlgorithm.ALGO2: 2, LunarAlgorithm.ALGO3: 3}
_LUNAR_YEAR_RANGE = {
  LunarAlgorithm.ALGO1: (1901, 2099),
  LunarAlgorithm.ALGO2: (410, 2500),
  LunarAlgorithm.ALGO3: (1600, 2199),
}
_DELTA_T_EXPORT = {
  DeltaTModel.DEFAULT: "delta_t",
  DeltaTModel.ALGO1: "delta_t_algo1",
  DeltaTModel.ALGO2: "delta_t_algo2",
  DeltaTModel.ALGO3: "delta_t_algo3",
  DeltaTModel.ALGO4: "delta_t_algo4",
  DeltaTModel.ALGO5: "delta_t_algo5",
}


def _enum(value: object, enum_type: type[_EnumT], name: str) -> _EnumT:
  if not isinstance(value, enum_type):
    raise TypeError(f"{name} must be a {enum_type.__name__} member")
  return value


def _integer(value: object, name: str, minimum: int, maximum: int) -> int:
  if not isinstance(value, int) or isinstance(value, (bool, _IntEnum)):
    raise TypeError(f"{name} must be an integer")
  if value < minimum or value > maximum:
    raise ValueError(f"{name} must be in [{minimum}, {maximum}]")
  return value


def _finite(value: object, name: str) -> float:
  if isinstance(value, bool) or not isinstance(value, (int, float)):
    raise TypeError(f"{name} must be a real number")
  result = float(value)
  if not _math.isfinite(result):
    raise ValueError(f"{name} must be finite")
  return result


def _ranged_float(value: object, name: str, minimum: float, maximum: float, *, include_maximum: bool = True) -> float:
  result = _finite(value, name)
  outside = result < minimum or (result > maximum if include_maximum else result >= maximum)
  if outside:
    closing = "]" if include_maximum else ")"
    raise ValueError(f"{name} must be in [{minimum}, {maximum}{closing}")
  return result


def _gregorian_month_length(year: int, month: int) -> int:
  if month == 2:
    return 29 if year % 4 == 0 and (year % 100 != 0 or year % 400 == 0) else 28
  return 30 if month in (4, 6, 9, 11) else 31


def _civil_date(value: object, name: str, *, maximum_year: int = _MAX_CIVIL_YEAR) -> tuple[int, int, int]:
  if not isinstance(value, GregorianDate):
    raise TypeError(f"{name} must be a GregorianDate")
  year = _integer(value.year, f"{name}.year", 1, maximum_year)
  month = _integer(value.month, f"{name}.month", 1, 12)
  day = _integer(value.day, f"{name}.day", 1, _gregorian_month_length(year, month))
  return year, month, day


def _civil_datetime(value: object, name: str) -> tuple[int, int, int, float]:
  if not isinstance(value, CivilDateTime):
    raise TypeError(f"{name} must be a CivilDateTime")
  year = _integer(value.year, f"{name}.year", 1, _MAX_CIVIL_YEAR)
  month = _integer(value.month, f"{name}.month", 1, 12)
  day = _integer(value.day, f"{name}.day", 1, _gregorian_month_length(year, month))
  fraction = _ranged_float(value.fraction, f"{name}.fraction", 0.0, 1.0, include_maximum=False)
  return year, month, day, fraction


def _lunar_date(value: object, algorithm: LunarAlgorithm, name: str) -> tuple[int, int, bool, int]:
  if not isinstance(value, LunarDate):
    raise TypeError(f"{name} must be a LunarDate")
  start, end = _LUNAR_YEAR_RANGE[algorithm]
  year = _integer(value.year, f"{name}.year", start, end)
  month = _integer(value.month, f"{name}.month", 1, 12)
  day = _integer(value.day, f"{name}.day", 1, 30)
  if not isinstance(value.is_leap, bool):
    raise TypeError(f"{name}.is_leap must be a bool")
  return year, month, value.is_leap, day


def _failure(operation: str, *, recording: bool) -> CelestialError:
  detail = _binding.last_error_text() if recording else ""
  message = detail or f"{operation} failed"
  return CelestialError(operation, message, recorded=bool(detail))


def _valid(result: object, operation: str, *, recording: bool = False) -> object:
  if not result.valid:
    raise _failure(operation, recording=recording)
  return result


def _civil_result(result: object) -> CivilDateTime:
  return CivilDateTime(result.year, result.month, result.day, result.fraction)


def set_log_verbosity(level: LogVerbosity) -> None:
  """Set process-wide native log verbosity."""
  checked = _enum(level, LogVerbosity, "level")
  if not _binding.call("set_log_verbosity", _LOG_VERBOSITY[checked]):
    raise _failure("set_log_verbosity", recording=False)


def ut1_to_jd(ut1: CivilDateTime) -> float:
  """Convert a UT1 civil moment to Julian Day."""
  value = _civil_datetime(ut1, "ut1")
  result = _valid(_binding.call("ut1_to_jd", *value), "ut1_to_jd", recording=True)
  return result.value


def ut1_to_jde(ut1: CivilDateTime) -> float:
  """Convert a UT1 civil moment to TT-based Julian Ephemeris Day."""
  value = _civil_datetime(ut1, "ut1")
  result = _valid(_binding.call("ut1_to_jde", *value), "ut1_to_jde", recording=True)
  return result.value


def jde_to_ut1(jde: float) -> CivilDateTime:
  """Convert TT-based Julian Ephemeris Day to a UT1 civil moment."""
  result = _valid(_binding.call("jde_to_ut1", _finite(jde, "jde")), "jde_to_ut1", recording=True)
  return _civil_result(result)


def sun_apparent_geocentric_coordinate(jde: float) -> SunCoordinate:
  """Return the Sun's apparent geocentric ecliptic coordinate at JDE."""
  result = _valid(
    _binding.call("sun_apparent_geocentric_coord", _finite(jde, "jde")),
    "sun_apparent_geocentric_coordinate",
  )
  return SunCoordinate(result.lon, result.lat, result.r)


def moon_apparent_geocentric_coordinate(jde: float) -> MoonCoordinate:
  """Return the Moon's apparent geocentric ecliptic coordinate at JDE."""
  result = _valid(
    _binding.call("moon_apparent_geocentric_coord", _finite(jde, "jde")),
    "moon_apparent_geocentric_coordinate",
  )
  return MoonCoordinate(result.lon, result.lat, result.r)


def moon_illumination(jde: float) -> MoonIllumination:
  """Return the Moon's illuminated fraction and elongation at JDE."""
  result = _valid(
    _binding.call("moon_illumination", _finite(jde, "jde")),
    "moon_illumination",
    recording=True,
  )
  return MoonIllumination(result.illumination, result.elongation_deg)


def moon_bright_limb_position_angle(jde: float) -> float:
  """Return the Moon's bright-limb position angle in degrees."""
  result = _valid(
    _binding.call("moon_position_angle", _finite(jde, "jde")),
    "moon_bright_limb_position_angle",
    recording=True,
  )
  return result.angle_deg


def moon_phase_moments(year: int, phase: MoonPhase) -> tuple[float, ...]:
  """Return the JDE moments of a principal Moon phase in a Gregorian year."""
  checked_year = _integer(year, "year", 1, _MAX_CALENDAR_YEAR)
  checked_phase = _enum(phase, MoonPhase, "phase")
  total = _ctypes.c_uint32()
  written = _binding.call(
    "moon_phase_moments",
    checked_year,
    _MOON_PHASE[checked_phase],
    _ctypes.byref(total),
    None,
    0,
  )
  if written != 0 or total.value == 0:
    raise _failure("moon_phase_moments", recording=True)
  expected = total.value
  slots = (_ctypes.c_double * expected)()
  written = _binding.call(
    "moon_phase_moments",
    checked_year,
    _MOON_PHASE[checked_phase],
    _ctypes.byref(total),
    slots,
    expected,
  )
  if written != expected:
    raise _failure("moon_phase_moments", recording=True)
  return tuple(slots[:written])


def solar_longitude_roots(year: int, longitude_deg: float) -> tuple[float, ...]:
  """Return JDEs when the Sun reaches an apparent geocentric longitude."""
  checked_year = _integer(year, "year", 1, _MAX_CALENDAR_YEAR)
  longitude = _ranged_float(longitude_deg, "longitude_deg", 0.0, 360.0, include_maximum=False)
  discriminant = _valid(
    _binding.call("solar_lon_root_discriminant", checked_year, longitude),
    "solar_longitude_roots",
  )
  if discriminant.count == 0:
    return ()
  slots = (_ctypes.c_double * discriminant.count)()
  written = _binding.call("solar_lon_roots", checked_year, longitude, slots, discriminant.count)
  if written != discriminant.count:
    raise _failure("solar_longitude_roots", recording=False)
  return tuple(slots[:written])


def new_moons_after(jde: float, count: int) -> tuple[float, ...]:
  """Return the requested number of new-moon JDEs after a starting JDE."""
  start = _finite(jde, "jde")
  checked_count = _integer(count, "count", 0, _MAX_NEW_MOON_COUNT)
  if checked_count == 0:
    return ()
  slots = (_ctypes.c_double * checked_count)()
  written = _binding.call("new_moons_after_jde", start, slots, checked_count)
  if written != checked_count:
    raise _failure("new_moons_after", recording=False)
  return tuple(slots[:written])


def new_moons_in_year(year: int) -> tuple[float, ...]:
  """Return all new-moon JDEs in a Gregorian year."""
  checked_year = _integer(year, "year", 1, _MAX_CALENDAR_YEAR)
  total = _ctypes.c_uint32()
  written = _binding.call("new_moons_in_year", checked_year, _ctypes.byref(total), None, 0)
  if written != 0 or total.value == 0:
    raise _failure("new_moons_in_year", recording=False)
  expected = total.value
  slots = (_ctypes.c_double * expected)()
  written = _binding.call("new_moons_in_year", checked_year, _ctypes.byref(total), slots, expected)
  if written != expected:
    raise _failure("new_moons_in_year", recording=False)
  return tuple(slots[:written])


def equation_of_time(jde: float) -> float:
  """Return apparent minus mean solar time in degrees of hour angle."""
  result = _valid(_binding.call("equation_of_time", _finite(jde, "jde")), "equation_of_time")
  return result.value


def apparent_solar_time(utc: CivilDateTime, longitude_deg: float) -> CivilDateTime:
  """Convert civil UTC to local apparent solar time at an east-positive longitude in degrees."""
  value = _civil_datetime(utc, "utc")
  longitude = _ranged_float(longitude_deg, "longitude_deg", -180.0, 180.0)
  result = _valid(_binding.call("apparent_solar_time", *value, longitude), "apparent_solar_time")
  return _civil_result(result)


def local_apparent_sidereal_time(jd_ut1: float, longitude_deg: float) -> float:
  """Return local apparent sidereal time in degrees at an east-positive longitude."""
  jd = _finite(jd_ut1, "jd_ut1")
  longitude = _ranged_float(longitude_deg, "longitude_deg", -180.0, 180.0)
  result = _valid(
    _binding.call("local_apparent_sidereal_time", jd, longitude),
    "local_apparent_sidereal_time",
    recording=True,
  )
  return result.value


def jieqi_moment(year: int, jieqi: Jieqi) -> JieqiMoment:
  """Return the UT1 civil moment of a Jieqi in a Gregorian year."""
  checked_year = _integer(year, "year", _MIN_JIEQI_YEAR, _MAX_CALENDAR_YEAR)
  checked_jieqi = _enum(jieqi, Jieqi, "jieqi")
  result = _valid(_binding.call("query_jieqi_moment", checked_year, checked_jieqi.value), "jieqi_moment")
  return JieqiMoment(Jieqi(result.jq_idx), CivilDateTime(result.y, result.m, result.d, result.frac))


def jieqi_name(jieqi: Jieqi) -> str:
  """Return the Chinese name of a Jieqi."""
  checked_jieqi = _enum(jieqi, Jieqi, "jieqi")
  buffer = _ctypes.create_string_buffer(_JIEQI_NAME_BYTES)
  if not _binding.call("get_jieqi_name", checked_jieqi.value, buffer, len(buffer)):
    raise _failure("jieqi_name", recording=False)
  return buffer.value.decode("utf-8")


def supported_lunar_year_range(algorithm: LunarAlgorithm) -> LunarYearRange:
  """Return the inclusive Lunar year range supported by an algorithm."""
  checked = _enum(algorithm, LunarAlgorithm, "algorithm")
  result = _valid(
    _binding.call("get_supported_lunar_year_range", _LUNAR_ALGORITHM[checked]),
    "supported_lunar_year_range",
  )
  return LunarYearRange(result.start, result.end)


def lunar_year_info(algorithm: LunarAlgorithm, year: int) -> LunarYearInfo:
  """Return the first day and month lengths of a Lunar year."""
  checked = _enum(algorithm, LunarAlgorithm, "algorithm")
  start, end = _LUNAR_YEAR_RANGE[checked]
  checked_year = _integer(year, "year", start, end)
  result = _valid(_binding.call("get_lunar_year_info", _LUNAR_ALGORITHM[checked], checked_year), "lunar_year_info")
  month_count = 12 if result.leap_month == 0 else 13
  month_lengths = tuple(30 if result.month_len & (1 << index) else 29 for index in range(month_count))
  leap_month = None if result.leap_month == 0 else result.leap_month
  return LunarYearInfo(GregorianDate(result.year, result.month, result.day), leap_month, month_lengths)


def gregorian_to_lunar(algorithm: LunarAlgorithm, date: GregorianDate) -> LunarDate:
  """Convert a Gregorian date to a Chinese Lunar date."""
  checked = _enum(algorithm, LunarAlgorithm, "algorithm")
  value = _civil_date(date, "date")
  result = _valid(_binding.call("gregorian_to_lunar", _LUNAR_ALGORITHM[checked], *value), "gregorian_to_lunar")
  return LunarDate(result.year, result.month, result.day, result.is_leap)


def lunar_to_gregorian(algorithm: LunarAlgorithm, date: LunarDate) -> GregorianDate:
  """Convert a Chinese Lunar date to a Gregorian date."""
  checked = _enum(algorithm, LunarAlgorithm, "algorithm")
  year, month, is_leap, day = _lunar_date(date, checked, "date")
  result = _valid(
    _binding.call("lunar_to_gregorian", _LUNAR_ALGORITHM[checked], year, month, is_leap, day),
    "lunar_to_gregorian",
  )
  return GregorianDate(result.year, result.month, result.day)


def delta_t(year: float, model: DeltaTModel = DeltaTModel.DEFAULT) -> float:
  """Return ΔT in seconds for a decimal Gregorian year."""
  decimal_year = _finite(year, "year")
  checked_model = _enum(model, DeltaTModel, "model")
  if checked_model is DeltaTModel.ALGO1 and decimal_year < -4000.0:
    raise ValueError("year must be at least -4000 for DeltaTModel.ALGO1")
  if checked_model is DeltaTModel.ALGO3 and decimal_year >= 3000.0:
    raise ValueError("year must be less than 3000 for DeltaTModel.ALGO3")
  if checked_model is DeltaTModel.ALGO4 and decimal_year >= 2035.0:
    raise ValueError("year must be less than 2035 for DeltaTModel.ALGO4")
  result = _valid(_binding.call(_DELTA_T_EXPORT[checked_model], decimal_year), "delta_t")
  return result.value


__all__ = [
  "CelestialError",
  "CivilDateTime",
  "DeltaTModel",
  "GregorianDate",
  "Jieqi",
  "JieqiMoment",
  "LogVerbosity",
  "LunarAlgorithm",
  "LunarDate",
  "LunarYearInfo",
  "LunarYearRange",
  "MoonCoordinate",
  "MoonIllumination",
  "MoonPhase",
  "SunCoordinate",
  "__version__",
  "apparent_solar_time",
  "delta_t",
  "equation_of_time",
  "gregorian_to_lunar",
  "jde_to_ut1",
  "jieqi_moment",
  "jieqi_name",
  "local_apparent_sidereal_time",
  "lunar_to_gregorian",
  "lunar_year_info",
  "moon_apparent_geocentric_coordinate",
  "moon_bright_limb_position_angle",
  "moon_illumination",
  "moon_phase_moments",
  "new_moons_after",
  "new_moons_in_year",
  "set_log_verbosity",
  "solar_longitude_roots",
  "sun_apparent_geocentric_coordinate",
  "supported_lunar_year_range",
  "ut1_to_jd",
  "ut1_to_jde",
]
