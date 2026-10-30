# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Astronomical calculations and Chinese calendar conversion."""

import ctypes as _ctypes
import math as _math
from calendar import monthrange as _monthrange
from collections.abc import Iterable as _Iterable
from dataclasses import dataclass as _dataclass
from datetime import date as _date
from datetime import datetime as _datetime
from enum import Enum as _Enum
from enum import IntEnum as _IntEnum
from enum import StrEnum as _StrEnum
from operator import index as _index
from typing import TypeVar as _TypeVar

from . import _abi, _binding, _protocol
from . import capability as _capability, chart as _chart, events as _events, query as _query
from ._codes import constant as _constant, enum_code as _enum_code
from ._models import DeltaTModel as DeltaTModel
from ._models import ModelKey as ModelKey
from ._models import ModelKind as ModelKind
from ._models import NutationModel as NutationModel
from ._models import Target as Target
from ._models import selector_code as _selector_code
from ._protocol import CelestialError as CelestialError
from ._version import VERSION as __version__
from .capability import CapabilityOperation as CapabilityOperation
from .capability import CapabilityRecord as CapabilityRecord
from .capability import CapabilityStatus as CapabilityStatus
from .capability import CivilHouseCapability as CivilHouseCapability
from .capability import ModelIdentity as ModelIdentity
from .capability import ModelStatus as ModelStatus
from .chart import BodyState as BodyState
from .chart import ChartHouses as ChartHouses
from .chart import ChartSnapshot as ChartSnapshot
from .chart import ChartTimes as ChartTimes
from .chart import CivilScale as CivilScale
from .chart import GeoLocation as GeoLocation
from .chart import HouseSystem as HouseSystem
from .events import EventDirection as EventDirection
from .events import LongitudeDifferenceEvent as LongitudeDifferenceEvent
from .events import LongitudeEvent as LongitudeEvent
from .events import StationEvent as StationEvent
from .events import StationMotion as StationMotion
from .query import EquatorialPosition as EquatorialPosition
from .query import HouseQueryResult as HouseQueryResult
from .query import Position as Position


class MoonPhase(_StrEnum):
  """One of the four principal Moon phases."""

  NEW = "new"
  FIRST_QUARTER = "first_quarter"
  FULL = "full"
  LAST_QUARTER = "last_quarter"


class LunarAlgorithm(_StrEnum):
  """Chinese Lunar algorithms with different Gregorian date-label bases.

  ALGO1 preserves HKO labels for Lunar years 1901-2099. ALGO2 computes from VSOP87D /
  truncated ELP2000-82B for 410-2500. ALGO3 combines HKO for 1901-2099 with ALGO2 for
  the remaining years of 1600-2199. ALGO2 renders TT through the library's UTC model,
  then adds 8 hours: a UT1 proxy before 1972, the leap-second table from 1972, and
  frozen Delta AT = 37 s after its last entry. This is not an accuracy ranking.
  """

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
  """Civil date and fraction of its day; the operation identifies the time scale."""

  year: int
  month: int
  day: int
  fraction: float


@_dataclass(frozen=True)
class GregorianDate:
  """Proleptic Gregorian date label on the selected Lunar algorithm's basis."""

  year: int
  month: int
  day: int

  @staticmethod
  def from_date(value: _date) -> "GregorianDate":
    """Copy a date label, accepting date subclasses but rejecting datetime.

    No time scale or timezone conversion is performed. Always returns GregorianDate.
    """
    if not isinstance(value, _date) or isinstance(value, _datetime):
      raise TypeError("value must be a datetime.date")
    return GregorianDate(value.year, value.month, value.day)

  def to_date(self) -> _date:
    """Return this label as datetime.date, whose valid year range is [1, 9999].

    Wrong field types raise TypeError; invalid dates or years raise ValueError.
    """
    return _date(*_civil_date(self, "date"))


@_dataclass(frozen=True)
class LunarDate:
  """Chinese Lunar date in traditional month numbering, with a separate leap flag."""

  year: int
  month: int
  day: int
  is_leap: bool


@_dataclass(frozen=True)
class SunCoordinate:
  """Apparent geocentric ecliptic longitude/latitude in degrees and distance in AU."""

  longitude_deg: float
  latitude_deg: float
  distance_au: float


@_dataclass(frozen=True)
class MoonCoordinate:
  """Apparent geocentric ecliptic longitude/latitude in degrees and distance in km."""

  longitude_deg: float
  latitude_deg: float
  distance_km: float


@_dataclass(frozen=True)
class MoonIllumination:
  """Illuminated fraction [0, 1] and Moon-Sun longitude elongation [0, 360) degrees."""

  fraction: float
  elongation_deg: float


@_dataclass(frozen=True)
class JieqiMoment:
  """UT1 civil moment of a Jieqi, not a UTC or east-eight wall date."""

  jieqi: Jieqi
  moment_ut1: CivilDateTime


@_dataclass(frozen=True)
class LunarYearRange:
  """Inclusive Lunar year range supported by an algorithm."""

  start: int
  end: int


@_dataclass(frozen=True)
class LunarYearInfo:
  """Month lengths in days and first Gregorian date on the selected algorithm's basis."""

  first_day: GregorianDate
  leap_month: int | None
  month_lengths: tuple[int, ...]


_EnumT = _TypeVar("_EnumT", bound=_IntEnum | _StrEnum)
_MOON_PHASE = {MoonPhase.NEW: 0, MoonPhase.FIRST_QUARTER: 1, MoonPhase.FULL: 2, MoonPhase.LAST_QUARTER: 3}
_LUNAR_ALGORITHM = {LunarAlgorithm.ALGO1: 1, LunarAlgorithm.ALGO2: 2, LunarAlgorithm.ALGO3: 3}
_MAX_NEW_MOON_COUNT = 4096
_TARGET_OPERATIONS = tuple(
  CapabilityOperation[name.removeprefix("CELESTIAL_OPERATION_")]
  for name in _abi.PROTOCOLS["celestial_capabilities"]["operations"]
)


def _enum(value: object, enum_type: type[_EnumT], name: str) -> _EnumT:
  if not isinstance(value, enum_type):
    raise TypeError(f"{name} must be a {enum_type.__name__} member")
  if not any(value is member for member in enum_type):
    raise ValueError(f"{name} must be a declared {enum_type.__name__} member")
  return value


def _integer(value: object, name: str, minimum: int, maximum: int) -> int:
  if not isinstance(value, int) or isinstance(value, (bool, _Enum)):
    raise TypeError(f"{name} must be an integer")
  value = _index(value)
  if not minimum <= value <= maximum:
    raise ValueError(f"{name} must fit [{minimum}, {maximum}]")
  return value


def _year(value: object, name: str = "year") -> int:
  return _integer(value, name, -(2**31), 2**31 - 1)


def _finite(value: object, name: str) -> float:
  if isinstance(value, (bool, _Enum)) or not isinstance(value, (int, float)):
    raise TypeError(f"{name} must be a real number")
  try:
    result = float(value)
  except OverflowError as error:
    raise ValueError(f"{name} must fit a finite native double") from error
  if not _math.isfinite(result):
    raise ValueError(f"{name} must be finite")
  return result


def _civil_date(value: object, name: str) -> tuple[int, int, int]:
  if not isinstance(value, GregorianDate):
    raise TypeError(f"{name} must be a GregorianDate")
  return (
    _year(value.year, f"{name}.year"),
    _integer(value.month, f"{name}.month", 0, 255),
    _integer(value.day, f"{name}.day", 0, 255),
  )


def _civil_datetime(value: object, name: str) -> tuple[int, int, int, float]:
  if not isinstance(value, CivilDateTime):
    raise TypeError(f"{name} must be a CivilDateTime")
  return (
    _year(value.year, f"{name}.year"),
    _integer(value.month, f"{name}.month", 0, 2**32 - 1),
    _integer(value.day, f"{name}.day", 0, 2**32 - 1),
    _finite(value.fraction, f"{name}.fraction"),
  )


def _location(value: object) -> tuple[float, float]:
  if not isinstance(value, GeoLocation):
    raise TypeError("location must be a GeoLocation")
  return _finite(value.latitude_deg, "location.latitude_deg"), _finite(value.longitude_deg, "location.longitude_deg")


def _result(export: str, operation: str, *args: object) -> object:
  return _protocol.valid(_binding.call(export, *args), operation)


def _number(value: float, operation: str) -> float:
  return _protocol.decode(operation, _protocol.finite, value, "value")


def _gregorian_result(year: int, month: int, day: int) -> GregorianDate:
  if not -32767 <= year <= 32767:
    raise ValueError("Gregorian year is not representable")
  if not 1 <= month <= 12:
    raise ValueError("invalid Gregorian month")
  month_length = _monthrange(year, month)[1]
  if not 1 <= day <= month_length:
    raise ValueError("invalid Gregorian day")
  return GregorianDate(year, month, day)


def _civil_result(result: object) -> CivilDateTime:
  date = _gregorian_result(result.year, result.month, result.day)
  fraction = _protocol.finite(result.fraction, "fraction")
  if not 0.0 <= fraction < 1.0:
    raise ValueError("fraction is outside [0, 1)")
  return CivilDateTime(date.year, date.month, date.day, fraction)


def _coordinate_values(result: object) -> tuple[float, float, float]:
  longitude = _protocol.longitude(result.lon, "longitude_deg")
  latitude = _protocol.finite(result.lat, "latitude_deg")
  distance = _protocol.finite(result.r, "distance")
  if not -90.0 <= latitude <= 90.0 or distance <= 0.0:
    raise ValueError("invalid computed coordinate")
  return longitude, latitude, distance


def ut1_to_jd(ut1: CivilDateTime) -> float:
  """Convert a UT1 civil moment to Julian Day."""
  return _number(_result("celestial_ut1_to_jd", "ut1_to_jd", *_civil_datetime(ut1, "ut1")).value, "ut1_to_jd")


def ut1_to_jde(ut1: CivilDateTime) -> float:
  """Convert a UT1 civil moment to TT-based Julian Ephemeris Day using fixed Algo5."""
  return _number(_result("celestial_ut1_to_jde", "ut1_to_jde", *_civil_datetime(ut1, "ut1")).value, "ut1_to_jde")


def jde_to_ut1(jde: float) -> CivilDateTime:
  """Convert TT-based Julian Ephemeris Day to a UT1 civil moment using fixed Algo5."""
  result = _result("celestial_jde_to_ut1", "jde_to_ut1", _finite(jde, "jde"))
  return _protocol.decode("jde_to_ut1", _civil_result, result)


def utc_to_jde(utc: CivilDateTime) -> float:
  """Convert UTC civil fields to JDE(TT), using UT1 with fixed Algo5 before 1972.

  Input years are in [1, 32767]; the shifted TT instant must stay representable.
  Modern dates use the fixed leap-second table, retaining its final offset afterward.
  Leap seconds and the 1972 model seam are not generally invertible.
  """
  return _number(_result("celestial_utc_to_jde", "utc_to_jde", *_civil_datetime(utc, "utc")).value, "utc_to_jde")


def jde_to_utc(jde: float) -> CivilDateTime:
  """Convert JDE(TT) in [1867522.5, 13689325.5) to UTC civil fields.

  Before 1972 the result uses the UT1 substitute with fixed Algo5; year 400 is possible.
  After the table's last entry, its final offset is retained. Leap seconds, leap-step midnight
  round-off and the 1972 model seam do not promise an exact inverse.
  """
  result = _result("celestial_jde_to_utc", "jde_to_utc", _finite(jde, "jde"))
  return _protocol.decode("jde_to_utc", _civil_result, result)


def sun_apparent_geocentric_coordinate(jde: float) -> SunCoordinate:
  """Return the Sun's apparent ecliptic position at JDE(TT), in degrees and AU."""
  operation = "sun_apparent_geocentric_coordinate"
  result = _result("celestial_sun_apparent_geocentric_coord", operation, _finite(jde, "jde"))
  return SunCoordinate(*_protocol.decode(operation, _coordinate_values, result))


def moon_apparent_geocentric_coordinate(jde: float) -> MoonCoordinate:
  """Return the Moon's apparent ecliptic position at JDE(TT), in degrees and km."""
  operation = "moon_apparent_geocentric_coordinate"
  result = _result("celestial_moon_apparent_geocentric_coord", operation, _finite(jde, "jde"))
  return MoonCoordinate(*_protocol.decode(operation, _coordinate_values, result))


def moon_illumination(jde: float) -> MoonIllumination:
  """Return the Moon's illuminated fraction and elongation in degrees at JDE(TT)."""
  result = _result("celestial_moon_illumination", "moon_illumination", _finite(jde, "jde"))

  def illumination() -> MoonIllumination:
    fraction = _protocol.finite(result.illumination, "illumination")
    if not 0.0 <= fraction <= 1.0:
      raise ValueError("illumination is outside [0, 1]")
    return MoonIllumination(fraction, _protocol.longitude(result.elongation_deg, "elongation_deg"))

  return _protocol.decode("moon_illumination", illumination)


def moon_bright_limb_position_angle(jde: float) -> float:
  """Return bright-limb position angle in degrees east of the disk's north at JDE(TT)."""
  operation = "moon_bright_limb_position_angle"
  result = _result("celestial_moon_position_angle", operation, _finite(jde, "jde"))
  return _protocol.decode(operation, _protocol.longitude, result.angle_deg, "angle_deg")


def moon_phase_moments(year: int, phase: MoonPhase) -> tuple[float, ...]:
  """Return principal-phase JDE(TT) moments in a Gregorian year [1, 32766]."""
  records = _protocol.count_fill(
    "celestial_moon_phase_moments",
    (_year(year), _MOON_PHASE[_enum(phase, MoonPhase, "phase")]),
    _ctypes.c_double,
    "moon_phase_moments",
  )
  return tuple(_number(value, "moon_phase_moments") for value in records)


def sun_longitude_crossings(year: int, longitude_deg: float) -> tuple[float, ...]:
  """Return JDE(TT) crossings of a longitude [0, 360) degrees in year [1, 32766].

  An empty tuple means no crossing. Invalid values raise ValueError; native
  computation failures and corrupt payloads raise CelestialError.
  """
  operation = "sun_longitude_crossings"
  args = (_year(year), _finite(longitude_deg, "longitude_deg"))
  discriminant = _result("celestial_solar_lon_root_discriminant", operation, *args)
  if discriminant.count not in (0, 1, 2):
    raise _protocol.malformed(operation, "invalid root count")
  if discriminant.count == 0:
    return ()
  slots = (_ctypes.c_double * discriminant.count)()
  written = _binding.call("celestial_solar_lon_roots", *args, slots, len(slots))
  _protocol.check_error(operation)
  if written != len(slots):
    raise _protocol.malformed(operation, "incomplete root fill")
  return tuple(_number(value, operation) for value in slots)


def new_moons_after(jde: float, count: int) -> tuple[float, ...]:
  """Return count new-moon JDE(TT)s after jde; the binding resource cap is 4096.

  count must be an integer in [0, 4096]; zero returns an empty tuple without a call.
  """
  start = _finite(jde, "jde")
  checked = _integer(count, "count", 0, _MAX_NEW_MOON_COUNT)
  if checked == 0:
    return ()
  slots = (_ctypes.c_double * checked)()
  written = _binding.call("celestial_new_moons_after_jde", start, slots, checked)
  _protocol.check_error("new_moons_after")
  if written != checked:
    raise _protocol.malformed("new_moons_after", "incomplete fill")
  return tuple(_number(value, "new_moons_after") for value in slots)


def new_moons_in_year(year: int) -> tuple[float, ...]:
  """Return all new-moon JDE(TT)s in a Gregorian year [1, 32766]."""
  records = _protocol.count_fill("celestial_new_moons_in_year", (_year(year),), _ctypes.c_double, "new_moons_in_year")
  return tuple(_number(value, "new_moons_in_year") for value in records)


def equation_of_time(jde: float) -> float:
  """Return apparent minus mean solar time in degrees of hour angle at JDE(TT)."""
  result = _result("celestial_equation_of_time", "equation_of_time", _finite(jde, "jde"))
  return _number(result.value, "equation_of_time")


def apparent_solar_time(utc: CivilDateTime, longitude_deg: float) -> CivilDateTime:
  """Convert civil UTC to local apparent solar time; longitude is east-positive degrees."""
  return _protocol.decode(
    "apparent_solar_time",
    _civil_result,
    _result(
      "celestial_apparent_solar_time",
      "apparent_solar_time",
      *_civil_datetime(utc, "utc"),
      _finite(longitude_deg, "longitude_deg"),
    ),
  )


def local_apparent_sidereal_time(jd_ut1: float, longitude_deg: float) -> float:
  """Return LAST [0, 360) degrees at JD(UT1) and an east-positive longitude in degrees.

  The native year window is [401, 32766]; longitude is in [-180, 180]. This fixed
  operation uses Algo5 and IAU1980, independently of the house nutation default.
  """
  operation = "local_apparent_sidereal_time"
  result = _result(
    "celestial_local_apparent_sidereal_time",
    operation,
    _finite(jd_ut1, "jd_ut1"),
    _finite(longitude_deg, "longitude_deg"),
  )
  return _protocol.decode(operation, _protocol.longitude, result.value, "value")


def jieqi_moment(year: int, jieqi: Jieqi) -> JieqiMoment:
  """Return a Jieqi's UT1 civil moment in year [401, 32766], not a UTC+8 wall date.

  Establish a time-scale conversion before displaying UTC or an east-eight date.
  The returned UT1 year may differ from the requested year.
  """
  result = _result("celestial_query_jieqi_moment", "jieqi_moment", _year(year), _enum(jieqi, Jieqi, "jieqi").value)
  return _protocol.decode("jieqi_moment", _jieqi_result, result)


def _jieqi_result(result: object) -> JieqiMoment:
  date = _gregorian_result(result.y, result.m, result.d)
  fraction = _protocol.finite(result.frac, "fraction")
  if not 0.0 <= fraction < 1.0:
    raise ValueError("fraction is outside [0, 1)")
  return JieqiMoment(Jieqi(result.jq_idx), CivilDateTime(date.year, date.month, date.day, fraction))


def jieqi_name(jieqi: Jieqi) -> str:
  """Return the Chinese UTF-8 name of a declared Jieqi."""
  return _protocol.sized_string("celestial_get_jieqi_name", (_enum(jieqi, Jieqi, "jieqi").value,), "jieqi_name")


def supported_lunar_year_range(algorithm: LunarAlgorithm) -> LunarYearRange:
  """Query inclusive Lunar-year bounds, not Gregorian-year bounds, from native."""
  code = _LUNAR_ALGORITHM[_enum(algorithm, LunarAlgorithm, "algorithm")]
  result = _result("celestial_get_supported_lunar_year_range", "supported_lunar_year_range", code)
  if result.start > result.end:
    raise _protocol.malformed("supported_lunar_year_range", "reversed year bounds")
  return LunarYearRange(result.start, result.end)


def lunar_year_info(algorithm: LunarAlgorithm, year: int) -> LunarYearInfo:
  """Return month lengths and first Gregorian date on the selected algorithm's basis.

  The native library validates the algorithm's Lunar-year admission range.
  """
  code = _LUNAR_ALGORITHM[_enum(algorithm, LunarAlgorithm, "algorithm")]
  result = _result("celestial_get_lunar_year_info", "lunar_year_info", code, _year(year))
  first_day = _protocol.decode("lunar_year_info", _gregorian_result, result.year, result.month, result.day)
  if not 0 <= result.leap_month <= 12:
    raise _protocol.malformed("lunar_year_info", "invalid leap month")
  count = 12 if result.leap_month == 0 else 13
  return LunarYearInfo(
    first_day,
    None if result.leap_month == 0 else result.leap_month,
    tuple(30 if result.month_len & (1 << index) else 29 for index in range(count)),
  )


def gregorian_to_lunar(algorithm: LunarAlgorithm, date: GregorianDate | _date) -> LunarDate:
  """Convert a Gregorian date label on the selected algorithm's basis to a Lunar date.

  date subclasses are accepted; datetime is rejected without dropping time or timezone.
  This does not convert an instant. Native rejects invalid dates and uncovered labels.
  """
  code = _LUNAR_ALGORITHM[_enum(algorithm, LunarAlgorithm, "algorithm")]
  if isinstance(date, _date):
    date = GregorianDate.from_date(date)
  result = _result("celestial_gregorian_to_lunar", "gregorian_to_lunar", code, *_civil_date(date, "date"))
  if not 1 <= result.month <= 12 or not 1 <= result.day <= 30:
    raise _protocol.malformed("gregorian_to_lunar", "invalid lunar month or day")
  return LunarDate(result.year, result.month, result.day, result.is_leap)


def lunar_to_gregorian(algorithm: LunarAlgorithm, date: LunarDate) -> GregorianDate:
  """Convert a traditional Lunar month plus leap flag to a Gregorian date label."""
  code = _LUNAR_ALGORITHM[_enum(algorithm, LunarAlgorithm, "algorithm")]
  if not isinstance(date, LunarDate):
    raise TypeError("date must be a LunarDate")
  year = _year(date.year, "date.year")
  month = _integer(date.month, "date.month", 0, 255)
  day = _integer(date.day, "date.day", 0, 255)
  if not isinstance(date.is_leap, bool):
    raise TypeError("date.is_leap must be a bool")
  result = _result("celestial_lunar_to_gregorian", "lunar_to_gregorian", code, year, month, date.is_leap, day)
  return _protocol.decode("lunar_to_gregorian", _gregorian_result, result.year, result.month, result.day)


def delta_t(year: float, model: DeltaTModel | ModelKey = DeltaTModel.DEFAULT) -> float:
  """Return TT - UT1 in seconds at a decimal Gregorian year.

  DEFAULT is resolved by native. Model guards apply at the evaluation instant:
  Algo1 requires year >= -4000, Algo3 year < 3000, and Algo4 year < 2035.
  The scalar convenience validates the resolved key; model_identity_text() supplies
  source/branch descriptions. These domains are not statistical accuracy guarantees.
  """
  code = _selector_code(model, ModelKind.DELTA_T, "model")
  result = _result("celestial_delta_t", "delta_t", _finite(year, "year"), code)
  return _protocol.decode("delta_t", _delta_t_value, result, code)


def _delta_t_value(result: object, code: int) -> float:
  key = ModelKey(ModelKind.DELTA_T, result.model_identity_code)
  if code and key.identity != code:
    raise ValueError("resolved model differs from explicit selection")
  return _protocol.finite(result.value, "value")


def _selected_key(key: ModelKey, code: int, operation: str) -> None:
  if code and key.identity != code:
    raise _protocol.malformed(operation, "resolved model differs from explicit selection")


def chart_snapshot(
  civil_dt: CivilDateTime,
  scale: CivilScale,
  location: GeoLocation,
  system: HouseSystem,
  model: DeltaTModel | ModelKey = DeltaTModel.DEFAULT,
) -> ChartSnapshot:
  """Return an immutable chart with resolved Delta T and house-nutation keys.

  Civil years are [1885, 2100); strict UTC starts at 1972-01-01. The native library validates civil,
  geographic, joint-target/rate and derived-TT model limits. Algo4 additionally requires
  derived JDE(TT) < 2464328.5. Future UTC uses the held leap table; derived UT1 is modelled,
  not observed DUT1. Location affects houses only; timezone conversion is the caller's job.
  """
  civil = _civil_datetime(civil_dt, "civil_dt")
  scale_code = _enum_code("CIVIL_SCALE", _enum(scale, CivilScale, "scale"))
  latitude, longitude = _location(location)
  system_code = _enum_code("HOUSE_SYSTEM", _enum(system, HouseSystem, "system"))
  model_code = _selector_code(model, ModelKind.DELTA_T, "model")
  result = _result(
    "celestial_chart_snapshot", "chart_snapshot", *civil, scale_code, latitude, longitude, system_code, model_code
  )
  decoded = _protocol.decode("chart_snapshot", _chart.decode_snapshot, result)
  _selected_key(decoded.delta_t_model, model_code, "chart_snapshot")
  return decoded


def _event_records(export: str, args: tuple[object, ...], record_type, decoder, start: float, end: float):
  operation = export.removeprefix("celestial_")
  records = _protocol.count_fill(export, args, record_type, operation)
  decoded = _protocol.decode(operation, decoder, records)
  if any(not start <= event.jde_tt < end for event in decoded):
    raise _protocol.malformed(operation, "event outside requested interval")
  if any(first.jde_tt >= second.jde_tt for first, second in zip(decoded, decoded[1:], strict=False)):
    raise _protocol.malformed(operation, "events not in strict time order")
  return decoded


def longitude_events(
  target: Target, longitude_deg: float, start_jde_tt: float, end_jde_tt: float
) -> tuple[LongitudeEvent, ...]:
  """Return longitude crossings or exact contacts in [start_jde_tt, end_jde_tt).

  Angles are apparent geocentric degrees; whole-turn shifts denote the same level.
  Times are JDE(TT), rates are degrees per TT day. The native library validates the target's rate domain
  and nonempty span limit of 3660 TT days. Empty success is legal; no partial list is returned.
  """
  code = _enum_code("TARGET", _enum(target, Target, "target"))
  start, end = _finite(start_jde_tt, "start_jde_tt"), _finite(end_jde_tt, "end_jde_tt")
  return _event_records(
    "celestial_longitude_events",
    (code, _finite(longitude_deg, "longitude_deg"), start, end),
    _abi.CelestialLongitudeEventRecord,
    _events.decode_longitude_events,
    start,
    end,
  )


def longitude_stations(target: Target, start_jde_tt: float, end_jde_tt: float) -> tuple[StationEvent, ...]:
  """Return stationary instants of the published seven-point rate, in JDE(TT).

  The interval/domain contract is longitude_events()'s. Certified nonstationary targets
  return an empty tuple. The reported rate in degrees per TT day need not be bitwise zero.
  """
  code = _enum_code("TARGET", _enum(target, Target, "target"))
  start, end = _finite(start_jde_tt, "start_jde_tt"), _finite(end_jde_tt, "end_jde_tt")
  return _event_records(
    "celestial_longitude_stations",
    (code, start, end),
    _abi.CelestialStationEventRecord,
    _events.decode_station_events,
    start,
    end,
  )


def longitude_difference_events(
  first: Target, second: Target, difference_deg: float, start_jde_tt: float, end_jde_tt: float
) -> tuple[LongitudeDifferenceEvent, ...]:
  """Return directed (first - second) longitude events, canonicalized to (-180, 180] degrees.

  Reversing targets reverses the angle and direction. Self-pairs and continuously satisfied
  relations are invalid; constant mismatch is legal empty success. Both rate domains apply.
  Times and interval limits follow longitude_events(); failed searches return no partial list.
  """
  a = _enum_code("TARGET", _enum(first, Target, "first"))
  b = _enum_code("TARGET", _enum(second, Target, "second"))
  start, end = _finite(start_jde_tt, "start_jde_tt"), _finite(end_jde_tt, "end_jde_tt")
  return _event_records(
    "celestial_longitude_difference_events",
    (a, b, _finite(difference_deg, "difference_deg"), start, end),
    _abi.CelestialLongitudeDifferenceEventRecord,
    _events.decode_difference_events,
    start,
    end,
  )


def capabilities(operation: CapabilityOperation, subject: Target | None = None) -> tuple[CapabilityRecord, ...]:
  """Return native TT capability records for POSITION, LONGITUDE_RATE, EVENTS or EQUATORIAL.

  subject selects a Target; None selects every target. HOUSES, CHART and DELTA_T
  raise ValueError. Use house_capability(delta_t_model=...) for civil-house policy
  and capability_text() for operation-wide descriptions.
  """
  checked = _enum(operation, CapabilityOperation, "operation")
  if checked not in _TARGET_OPERATIONS:
    if checked is CapabilityOperation.HOUSES:
      raise ValueError("use house_capability(delta_t_model=...) for civil-house policy")
    raise ValueError("use capability_text() for CHART or DELTA_T descriptions")
  targets = tuple(Target) if subject is None else (_enum(subject, Target, "subject"),)
  subject_code = _constant("TARGET_ALL") if subject is None else _enum_code("TARGET", targets[0])
  records = _protocol.count_fill(
    "celestial_capabilities",
    (_enum_code("OPERATION", checked), subject_code),
    _abi.CelestialCapabilityRecord,
    "capabilities",
    expected_count=len(targets),
  )
  return _protocol.decode("capabilities", _capability.decode_capability_records, checked, records, targets)


def house_capability(*, delta_t_model: DeltaTModel | ModelKey = DeltaTModel.DEFAULT) -> CivilHouseCapability:
  """Return the civil-house admission policy and resolved Delta T key, without computing houses."""
  code = _selector_code(delta_t_model, ModelKind.DELTA_T, "delta_t_model")
  result = _result("celestial_house_capability", "house_capability", code)
  decoded = _protocol.decode("house_capability", _capability.decode_house_capability, result)
  _selected_key(decoded.delta_t_model, code, "house_capability")
  return decoded


def model_identities(kind: ModelKind | None = None) -> tuple[ModelIdentity, ...]:
  """Return registered keys and applicability; DEFAULT is not a registered identity."""
  code = _constant("MODEL_KIND_ALL") if kind is None else _enum_code("MODEL_KIND", _enum(kind, ModelKind, "kind"))
  records = _protocol.count_fill(
    "celestial_model_identities", (code,), _abi.CelestialModelIdentityRecord, "model_identities"
  )
  return _protocol.decode("model_identities", _capability.decode_model_identities, records, kind)


def model_identity_text(key: ModelKey) -> str:
  """Describe the exact registered (kind, identity) key, independently of registry order."""
  if not isinstance(key, ModelKey):
    raise TypeError("key must be a ModelKey")
  ModelKey(key.kind, key.identity)
  return _protocol.sized_string(
    "celestial_model_identity_text", (_enum_code("MODEL_KIND", key.kind), key.identity), "model_identity_text"
  )


def capability_text(operation: CapabilityOperation, subject: Target | None = None) -> str:
  """Describe an operation's sources, corrections, defaults and domains.

  Target operations require a concrete Target subject. HOUSES, CHART and DELTA_T
  require subject=None for operation-wide text; a Target there raises ValueError.
  Use model_identity_text() for a model key, not a registry ordinal.
  """
  checked = _enum(operation, CapabilityOperation, "operation")
  if checked in _TARGET_OPERATIONS:
    code = _enum_code("TARGET", _enum(subject, Target, "subject"))
  else:
    if subject is not None:
      _enum(subject, Target, "subject")
      raise ValueError("subject must be None for HOUSES, CHART and DELTA_T text")
    code = _constant("TARGET_ALL")
  return _protocol.sized_string(
    "celestial_capability_text", (_enum_code("OPERATION", checked), code), "capability_text"
  )


def _selection(targets: _Iterable[Target]) -> tuple[tuple[Target, ...], object]:
  checked = tuple(_enum(target, Target, f"targets[{index}]") for index, target in enumerate(targets))
  if not checked:
    raise ValueError("targets must not be empty")
  if len(set(checked)) != len(checked):
    raise ValueError("targets must not contain duplicates")
  selection = (_ctypes.c_uint32 * len(checked))(*(_enum_code("TARGET", target) for target in checked))
  return checked, selection


def positions(targets: _Iterable[Target], jde_tt: float) -> tuple[Position, ...]:
  """Return selected apparent ecliptic positions of date, in caller order at JDE(TT).

  Longitude/latitude are degrees and physical distance is AU; nodes have no distance.
  Empty, duplicate and unknown selections are rejected. Only selected providers run:
  no rates, civil conversion, houses or snapshot joint window. A failed target fails all.
  """
  checked, selection = _selection(targets)
  records = _protocol.count_fill(
    "celestial_positions",
    (selection, len(checked), _finite(jde_tt, "jde_tt")),
    _abi.CelestialPositionRecord,
    "positions",
    expected_count=len(checked),
  )
  return _protocol.decode("positions", _query.decode_positions, records, checked)


def equatorial_positions(targets: _Iterable[Target], jde_tt: float) -> tuple[EquatorialPosition, ...]:
  """Return selected apparent geocentric RA/Dec of date at JDE(TT), in caller order.

  RA is [0, 360) degrees, declination [-90, 90] degrees, and physical distance is AU.
  All fourteen targets are supported; mathematical nodes have no distance. Selection
  and provider-window rules follow positions(). No rate, civil or house operation runs.
  Fixed IAU1980 longitude nutation and true obliquity define the projection. Finite-input
  evaluation may fail and does not establish accuracy or physical plausibility remotely.
  """
  checked, selection = _selection(targets)
  records = _protocol.count_fill(
    "celestial_equatorial_positions",
    (selection, len(checked), _finite(jde_tt, "jde_tt")),
    _abi.CelestialEquatorialPositionRecord,
    "equatorial_positions",
    expected_count=len(checked),
  )
  return _protocol.decode("equatorial_positions", _query.decode_equatorial_positions, records, checked)


def house_query(
  civil_dt: CivilDateTime,
  scale: CivilScale,
  location: GeoLocation,
  system: HouseSystem,
  *,
  delta_t_model: DeltaTModel | ModelKey = DeltaTModel.DEFAULT,
  nutation_model: NutationModel | ModelKey = NutationModel.DEFAULT,
) -> HouseQueryResult:
  """Return independent house geometry and both resolved model keys at a civil instant.

  Civil policy follows house_capability(): [1885, 2100), strict UTC from 1972-01-01,
  and the selected model's derived-TT cutoff (Algo4: JDE(TT) < 2464328.5). No chart
  target/rate window applies. Latitude is north-positive (-90, 90) degrees; longitude
  east-positive [-180, 180] degrees. The native library checks these domains and computes the geometry.
  Placidus rejects polar geometry; every system rejects an unresolved Ascendant.
  DEFAULT is resolved by native. Reusing result keys preserves model selection, while
  exact coefficients also depend on the library/source version. Future UTC-derived UT1
  is modelled from the held leap table, not observed DUT1. There is no partial result.
  """
  civil = _civil_datetime(civil_dt, "civil_dt")
  scale_code = _enum_code("CIVIL_SCALE", _enum(scale, CivilScale, "scale"))
  latitude, longitude = _location(location)
  checked_system = _enum(system, HouseSystem, "system")
  delta_code = _selector_code(delta_t_model, ModelKind.DELTA_T, "delta_t_model")
  nutation_code = _selector_code(nutation_model, ModelKind.NUTATION, "nutation_model")
  result = _result(
    "celestial_house_query",
    "house_query",
    *civil,
    scale_code,
    delta_code,
    nutation_code,
    latitude,
    longitude,
    _enum_code("HOUSE_SYSTEM", checked_system),
  )
  decoded = _protocol.decode("house_query", _query.decode_house_query, result, checked_system)
  _selected_key(decoded.delta_t_model, delta_code, "house_query")
  _selected_key(decoded.nutation_model, nutation_code, "house_query")
  return decoded


__all__ = [
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
  "house_capability",
  "house_query",
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
  "model_identities",
  "model_identity_text",
  "moon_apparent_geocentric_coordinate",
  "moon_bright_limb_position_angle",
  "moon_illumination",
  "moon_phase_moments",
  "new_moons_after",
  "new_moons_in_year",
  "positions",
  "sun_apparent_geocentric_coordinate",
  "sun_longitude_crossings",
  "supported_lunar_year_range",
  "ut1_to_jd",
  "ut1_to_jde",
  "utc_to_jde",
]
