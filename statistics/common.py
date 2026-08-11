# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
# 
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
# 
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import sys
from enum import Enum
from pathlib import Path
from datetime import date, datetime, timedelta
from dataclasses import dataclass

import ctypes
from ctypes import (
  c_int32, c_uint32,  c_uint16, c_uint8, c_double, c_bool, c_char, c_char_p,
  POINTER, Structure
)

from typing import Optional, List


def dynamic_lib_ext() -> str:
  """Return the library extension for the current platform."""
  if sys.platform == "win32":
    return ".dll"
  elif sys.platform == "darwin":
    return ".dylib"
  elif sys.platform == "linux":
    return ".so"
  raise OSError(f"Unsupported platform: {sys.platform}")


def search_lib_path(folder: Path) -> Optional[Path]:
  """Search for the shared library in the given folder."""
  expected_ext = dynamic_lib_ext()

  if not folder.is_dir():
    return None

  # Prefer the unversioned name (the latest build's link): versioned outputs accumulate
  # in the build dir and directory order is arbitrary.
  for path in folder.iterdir():
    if path.is_file() and path.name == f"libcelestial_calendar{expected_ext}":
      return path

  for path in folder.iterdir():
    if not path.is_file():
      continue
    if expected_ext not in path.name:
      continue
    if "celestial_calendar" not in path.name:
      continue
    return path


# Define constants for paths.
PROJ_PATH        = Path(__file__).parent.parent
USNO_DATA_PATH   = Path(__file__).parent / "usno_data.txt"  # notebook-only; no test consumer (#168)
BINDINGS_PATH    = PROJ_PATH / "build" / "shared_lib"
LIB_PATH         = search_lib_path(BINDINGS_PATH)

assert PROJ_PATH.exists(),        f"Project path not found: {PROJ_PATH}"
assert USNO_DATA_PATH.exists(),   f"USNO data not found: {USNO_DATA_PATH}"
assert BINDINGS_PATH.exists(),    f"Bindings path not found: {BINDINGS_PATH}"

assert LIB_PATH is not None,      f"Shared library not found in {BINDINGS_PATH}"
assert LIB_PATH.exists(),         f"Shared library not found: {LIB_PATH}"


# Define the argument and return types of the C functions.
#
# Every export in `src/shared_lib/celestial.h` needs its `argtypes`/`restype` here: without them
# ctypes assumes `restype = c_int`, which reads a struct return as garbage (#85).
LIB = ctypes.CDLL(str(LIB_PATH))


#region Library-level

LIB.set_log_verbosity.argtypes = [c_uint8]
LIB.set_log_verbosity.restype = c_bool

LIB.last_error.argtypes = []
LIB.last_error.restype = c_char_p

#endregion


#region Delta T functions

class DeltaT(Structure):
  _fields_ = [
    ("valid", c_bool),
    ("value", c_double),
  ]

LIB.delta_t_algo1.argtypes = [c_double]
LIB.delta_t_algo1.restype = DeltaT

LIB.delta_t_algo2.argtypes = [c_double]
LIB.delta_t_algo2.restype = DeltaT

LIB.delta_t_algo3.argtypes = [c_double]
LIB.delta_t_algo3.restype = DeltaT

LIB.delta_t_algo4.argtypes = [c_double]
LIB.delta_t_algo4.restype = DeltaT

LIB.delta_t_algo5.argtypes = [c_double]
LIB.delta_t_algo5.restype = DeltaT


# Wrap C functions with Python functions, so that they can be called from Python.
def delta_t_algo1(year: float) -> float:
  result = LIB.delta_t_algo1(year)
  if not result.valid:
    raise ValueError("Error occurred in delta_t_algo1.")
  return result.value

def delta_t_algo2(year: float) -> float:
  result = LIB.delta_t_algo2(year)
  if not result.valid:
    raise ValueError("Error occurred in delta_t_algo2.")
  return result.value

def delta_t_algo3(year: float) -> float:
  result = LIB.delta_t_algo3(year)
  if not result.valid:
    raise ValueError("Error occurred in delta_t_algo3.")
  return result.value

def delta_t_algo4(year: float) -> float:
  result = LIB.delta_t_algo4(year)
  if not result.valid:
    raise ValueError("Error occurred in delta_t_algo4.")
  return result.value

def delta_t_algo5(year: float) -> float:
  result = LIB.delta_t_algo5(year)
  if not result.valid:
    raise ValueError("Error occurred in delta_t_algo5.")
  return result.value

LIB.delta_t.argtypes = [c_double]
LIB.delta_t.restype = DeltaT

def delta_t(year: float) -> float:
  """The library's default ΔT, currently algo5."""
  result = LIB.delta_t(year)
  if not result.valid:
    raise ValueError("Error occurred in delta_t.")
  return result.value

#endregion


#region Delta T and Julian Day

# Define the JulianDay struct
class _JulianDay(Structure):
  _fields_ = [
    ("valid", c_bool  ),
    ("value", c_double),
  ]

# Define the UT1Time struct
class _UT1Time(Structure):
  _fields_ = [
    ("valid", c_bool),
    ("year",  c_int32),
    ("month", c_uint32),
    ("day",   c_uint32),
    ("fraction", c_double),
  ]

# Define the function signatures
LIB.ut1_to_jd.argtypes = [c_int32, c_uint32, c_uint32, c_double]
LIB.ut1_to_jd.restype = _JulianDay

LIB.ut1_to_jde.argtypes = [c_int32, c_uint32, c_uint32, c_double]
LIB.ut1_to_jde.restype = _JulianDay

LIB.jde_to_ut1.argtypes = [c_double]
LIB.jde_to_ut1.restype = _UT1Time


# Wrap C functions with Python functions, so that they can be called from Python.
def ut1_to_jd(y: int, m: int, d: int, fraction: float) -> float:
  """
  @brief Convert UT1 datetime to Julian Day Number (JD).
  @param y The year.
  @param m The month.
  @param d The day.
  @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
  @returns The Julian Day Number (JD).
  """
  jd = LIB.ut1_to_jd(y, m, d, fraction)

  if not jd.valid:
    raise ValueError("Error occurred in ut1_to_jd.")

  return jd.value

def ut1_to_jde(y: int, m: int, d: int, fraction: float) -> float:
  """
  @brief Convert UT1 datetime to Julian Ephemeris Day Number (JDE).
  @param y The year.
  @param m The month.
  @param d The day.
  @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
  @returns The Julian Ephemeris Day Number (JDE).
  """
  jde = LIB.ut1_to_jde(y, m, d, fraction)

  if not jde.valid:
    raise ValueError("Error occurred in ut1_to_jde.")

  return jde.value


def jde_to_ut1(jde: float) -> datetime:
  """
  @brief Convert Julian Ephemeris Day Number (JDE) to UT1 datetime.
  @param jde The julian ephemeris day number, which is based on TT.
  @returns A `datetime` object representing the UT1 datetime.
  """
  ut1 = LIB.jde_to_ut1(jde)

  if not ut1.valid:
    raise ValueError("Error occurred in jde_to_ut1.")

  date = datetime(ut1.year, ut1.month, ut1.day)
  elapsed_microseconds = int(ut1.fraction * 86400 * 1000000)
  return date + timedelta(microseconds=elapsed_microseconds)

#endregion


#region Sun and Moon Coordinates

# Define the SunCoordinate struct
class _SunCoordinate(Structure):
  _fields_ = [
    ("valid", c_bool  ),
    ("lon",   c_double),
    ("lat",   c_double),
    ("r",     c_double),
  ]

# Define the MoonCoordinate struct
class _MoonCoordinate(Structure):
  _fields_ = [
    ("valid", c_bool  ),
    ("lon",   c_double),
    ("lat",   c_double),
    ("r",     c_double),
  ]

# Define the MoonIllumination struct
class _MoonIllumination(Structure):
  _fields_ = [
    ("valid",          c_bool  ),
    ("illumination",   c_double),
    ("elongation_deg", c_double),
  ]


LIB.sun_apparent_geocentric_coord.argtypes = [c_double]
LIB.sun_apparent_geocentric_coord.restype = _SunCoordinate

LIB.moon_apparent_geocentric_coord.argtypes = [c_double]
LIB.moon_apparent_geocentric_coord.restype = _MoonCoordinate

LIB.moon_illumination.argtypes = [c_double]
LIB.moon_illumination.restype = _MoonIllumination

class _MoonPositionAngle(Structure):
  _fields_ = [
    ("valid",    c_bool  ),
    ("angle_deg", c_double),
  ]

LIB.moon_position_angle.argtypes = [c_double]
LIB.moon_position_angle.restype = _MoonPositionAngle


@dataclass
class SunCoordinate:
  lon: float # In degrees
  lat: float # In degrees
  r:   float # In AU

def sun_apparent_geocentric_coord(jde: float) -> SunCoordinate:
  """
  @brief Compute the apparent geocentric coordinates of the Sun.
  @param jde The julian ephemeris day number, which is based on TT.
  @returns A `SunCoordinate` representing the apparent geocentric coordinates of the Sun.
  """
  coord = LIB.sun_apparent_geocentric_coord(jde)

  if not coord.valid:
    raise ValueError("Error occurred in sun_apparent_geocentric_coord.")

  return SunCoordinate(
    lon = coord.lon,
    lat = coord.lat,
    r   = coord.r,
  )


@dataclass
class MoonCoordinate:
  lon: float # In degrees
  lat: float # In degrees
  r:   float # In KM

def moon_apparent_geocentric_coord(jde: float) -> MoonCoordinate:
  """
  @brief Compute the apparent geocentric coordinates of the Moon.
  @param jde The julian ephemeris day number, which is based on TT.
  @returns A `MoonCoordinate` representing the apparent geocentric coordinates of the Moon.
  """
  coord = LIB.moon_apparent_geocentric_coord(jde)

  if not coord.valid:
    raise ValueError("Error occurred in moon_apparent_geocentric_coord.")

  return MoonCoordinate(
    lon = coord.lon,
    lat = coord.lat,
    r   = coord.r,
  )


@dataclass
class MoonIllumination:
  illumination:   float # In [0, 1]
  elongation_deg: float # In degrees, in [0, 360)

def moon_illumination(jde: float) -> MoonIllumination:
  """
  @brief Compute the Moon's illuminated fraction and elongation (Meeus ch. 48).
  @param jde The julian ephemeris day number, which is based on TT.
  @returns A `MoonIllumination` with the fraction in [0, 1] and the elongation in degrees.
  """
  result = LIB.moon_illumination(jde)

  if not result.valid:
    raise ValueError("Error occurred in moon_illumination.")

  return MoonIllumination(
    illumination   = result.illumination,
    elongation_deg = result.elongation_deg,
  )


@dataclass
class MoonPositionAngle:
  angle_deg: float # In [0, 360)

def moon_position_angle(jde: float) -> MoonPositionAngle:
  """
  @brief Compute the position angle of the Moon's bright limb (Meeus ch. 48, (48.5)).
  @param jde The julian ephemeris day number, which is based on TT.
  @returns A `MoonPositionAngle` with the angle in degrees, in [0, 360).
  """
  result = LIB.moon_position_angle(jde)

  if not result.valid:
    raise ValueError("Error occurred in moon_position_angle.")

  return MoonPositionAngle(angle_deg = result.angle_deg)


LIB.moon_phase_moments.argtypes = [c_int32, c_uint8, POINTER(c_uint32), POINTER(c_double), c_uint32]
LIB.moon_phase_moments.restype = c_uint32

class MoonPhaseKind(Enum):
  NEW_MOON = 0
  FIRST_QUARTER = 1
  FULL_MOON = 2
  LAST_QUARTER = 3

@dataclass
class MoonPhaseMoments:
  year: int
  phase: MoonPhaseKind
  jdes: List[float]
  moments: List[datetime]

def moon_phase_moments(year: int, phase: MoonPhaseKind) -> MoonPhaseMoments:
  """
  Find the Julian Ephemeris Days (JDEs) of the given Moon phase in a year.

  @param year The Gregorian year.
  @param phase The phase kind.
  @returns A `MoonPhaseMoments` with the JDEs and corresponding UT1 datetimes.
  """
  slot_count = 15  # 12 or 13 moments per year per phase.
  root_count = c_uint32(0)
  slots = (c_double * slot_count)()

  num_written = LIB.moon_phase_moments(year, phase.value, ctypes.byref(root_count), slots, slot_count)

  if num_written == 0:
    raise ValueError(f"Error occurred in moon_phase_moments for year {year}, phase {phase}.")

  if num_written != root_count.value:
    raise ValueError(
      f"moon_phase_moments wrote {num_written} of {root_count.value} roots for year {year}; "
      f"slot_count is {slot_count}."
    )

  jdes = [slots[i] for i in range(num_written)]
  moments = [jde_to_ut1(jde) for jde in jdes]
  return MoonPhaseMoments(year=year, phase=phase, jdes=jdes, moments=moments)

#endregion


#region Sidereal Time

class _SiderealTime(Structure):
  _fields_ = [
    ("valid", c_bool  ),
    ("value", c_double),
  ]


LIB.local_apparent_sidereal_time.argtypes = [c_double, c_double]
LIB.local_apparent_sidereal_time.restype = _SiderealTime


def local_apparent_sidereal_time(jd_ut1: float, longitude: float) -> float:
  """
  @brief Compute the Local Apparent Sidereal Time (LAST).
  @param jd_ut1 The julian day number, which is based on UT1.
  @param longitude The observer's geographic longitude in degrees, positive east.
  @returns The LAST in degrees, in [0, 360).
  """
  result = LIB.local_apparent_sidereal_time(jd_ut1, longitude)

  if not result.valid:
    raise ValueError("Error occurred in local_apparent_sidereal_time.")

  return result.value

#endregion


#region Solar Time

class _EquationOfTime(Structure):
  _fields_ = [
    ("valid", c_bool),
    ("value", c_double),
  ]

LIB.equation_of_time.argtypes = [c_double]
LIB.equation_of_time.restype = _EquationOfTime

class _ApparentSolarTime(Structure):
  _fields_ = [
    ("valid",    c_bool),
    ("year",     c_int32),
    ("month",    c_uint32),
    ("day",      c_uint32),
    ("fraction", c_double),
  ]

LIB.apparent_solar_time.argtypes = [c_int32, c_uint32, c_uint32, c_double, c_double]
LIB.apparent_solar_time.restype = _ApparentSolarTime

#endregion


#region Jieqi

class Jieqi(Enum):
  立春 = 0
  雨水 = 1
  惊蛰 = 2
  春分 = 3
  清明 = 4
  谷雨 = 5
  立夏 = 6
  小满 = 7
  芒种 = 8
  夏至 = 9
  小暑 = 10
  大暑 = 11
  立秋 = 12
  处暑 = 13
  白露 = 14
  秋分 = 15
  寒露 = 16
  霜降 = 17
  立冬 = 18
  小雪 = 19
  大雪 = 20
  冬至 = 21
  小寒 = 22
  大寒 = 23

class _JieqiMomentQuery(Structure):
  _fields_ = [
    ("valid", c_bool),
    ("jq_idx", c_uint8),
    ("y", c_int32),
    ("m", c_uint32),
    ("d", c_uint32),
    ("frac", c_double),
  ]

LIB.query_jieqi_moment.argtypes = [c_int32, c_uint8]
LIB.query_jieqi_moment.restype = _JieqiMomentQuery

# `buf` is an output buffer, so it is typed as a pointer rather than `c_char_p` - the latter
# reads as "takes a string" and invites passing a `bytes`, which the C side would write into.
LIB.get_jieqi_name.argtypes = [c_uint8, POINTER(c_char), c_uint32]
LIB.get_jieqi_name.restype = c_bool

class _Discriminant(Structure):
  _fields_ = [
    ("valid", c_bool),
    ("count", c_uint32),
  ]

LIB.solar_lon_root_discriminant.argtypes = [c_int32, c_double]
LIB.solar_lon_root_discriminant.restype = _Discriminant

LIB.solar_lon_roots.argtypes = [c_int32, c_double, POINTER(c_double), c_uint32]
LIB.solar_lon_roots.restype = c_uint32


@dataclass
class JieqiMoment:
  jq: Jieqi
  moment: datetime

def jieqi_moment(year: int, jq: Jieqi) -> JieqiMoment:
  """
  Query the moment for a given year and Jieqi.
  @param year The year to query.
  @param jq The Jieqi to query.
  @returns A `JieqiMoment` representing the moment of the Jieqi.
  """
  query = LIB.query_jieqi_moment(year, jq.value)

  if not query.valid:
    raise ValueError("Error occurred in query_jieqi_moment.")

  if query.jq_idx != jq.value:
    raise ValueError("Unexpected jq_idx.")
  
  # Combine y, m, d, and frac into a datetime
  elapsed_microseconds = int(query.frac * 86400 * 1000000)
  dt = datetime(query.y, query.m, query.d) + timedelta(microseconds=elapsed_microseconds)

  return JieqiMoment(jq, dt)


#endregion

#region New Moon

LIB.new_moons_in_year.argtypes = [c_int32, POINTER(c_uint32), POINTER(c_double), c_uint32]
LIB.new_moons_in_year.restype = c_uint32

LIB.new_moons_after_jde.argtypes = [c_double, POINTER(c_double), c_uint32]
LIB.new_moons_after_jde.restype = c_uint32

@dataclass
class NewMoons:
  """
  A data class to hold the year and the list of Julian Ephemeris Days (JDEs)
  when new moons occur.
  """
  year: int
  new_moon_jdes: List[float]
  new_moon_moments: List[datetime]


def new_moons_in_year(year: int) -> NewMoons:
  """
  Find the Julian Ephemeris Days (JDEs) at which the Sun and Moon are at the
  same longitude in a given year.

  The function uses the shared library function new_moons_in_year to find
  the conjunction moments (new moons) for the specified year.

  @param year The year to search for new moons.
  @returns A NewMoons data class instance containing the year and the list
           of JDEs representing the conjunction moments (new moons).
  """
  # There should be either 12 or 13 new moons in a year, so 15 slots should be enough.
  slot_count = 15 

  # Allocate memory for the number of roots (new moons) and the slots to hold the JDEs.
  root_count = c_uint32(0)
  slots = (c_double * slot_count)()

  # Call the shared library function to get the new moons in the given year.
  num_written = LIB.new_moons_in_year(year, ctypes.byref(root_count), slots, slot_count)

  # The C++ side reports every failure as 0 written, and no year goes without a new moon.
  if num_written == 0:
    raise ValueError(f"Error occurred in new_moons_in_year for year {year}.")

  # `root_count` is what it found, `num_written` is what fit - a gap means roots were dropped.
  # Raised rather than asserted: `python -O` strips asserts.
  if num_written != root_count.value:
    raise ValueError(
      f"new_moons_in_year wrote {num_written} of {root_count.value} roots for year {year}; "
      f"slot_count is {slot_count}."
    )

  # Return the result as an instance of the NewMoons data class.
  jdes = [slots[i] for i in range(num_written)]
  # Rendered in UT1 while the C++ side now attributes years by UTC (#84) — the scales differ
  # by model ΔT − (ΔAT + 32.184 s), harmless for these notebooks' years.
  moments = [jde_to_ut1(jde) for jde in jdes]
  return NewMoons(
    year=year,
    new_moon_jdes=jdes,
    new_moon_moments=moments,
  )

#endregion


#region Lunar Year

class LunarAlgo(Enum):
  ALGO_1 = 1
  ALGO_2 = 2
  ALGO_3 = 3

class _SupportedLunarYearRange(Structure):
  _fields_ = [
    ("valid", c_bool),
    ("start", c_int32),
    ("end", c_int32),
  ]

LIB.get_supported_lunar_year_range.argtypes = [c_uint8]
LIB.get_supported_lunar_year_range.restype = _SupportedLunarYearRange

@dataclass
class SupportedLunarYearRange:
  start: int
  end: int

def get_supported_lunar_year_range(algo: LunarAlgo) -> SupportedLunarYearRange:
  """
  Return the supported lunar year range for the specified algorithm.

  @param algo The algorithm to use.
  @returns A `SupportedLunarYearRange` instance representing the supported lunar year range.
  """
  result = LIB.get_supported_lunar_year_range(algo.value)

  if not result.valid:
    raise ValueError("Error occurred in get_supported_lunar_year_range.")

  return SupportedLunarYearRange(
    start = result.start,
    end   = result.end,
  )


class _LunarYearInfo(Structure):
  _fields_ = [
    ("valid", c_bool),
    ("year", c_int32),
    ("month", c_uint8),
    ("day", c_uint8),
    ("leap_month", c_uint8),
    ("month_len", c_uint16),
  ]

LIB.get_lunar_year_info.argtypes = [c_uint8, c_int32]
LIB.get_lunar_year_info.restype = _LunarYearInfo

@dataclass
class LunarYearInfo:
  first_day: date # The first day of the lunar year in the Gregorian calendar.
  leap_month: int # The month of the leap month (1 <= leap_month <= 12). 0 if there is no leap month.
  month_lengths: List[int] # The number of days in each month of the lunar year.

def get_lunar_year_info(algo: LunarAlgo, year: int) -> LunarYearInfo:
  """
  Return the lunar year information for the specified year.

  @param algo The algorithm to use.
  @param year The year to get the lunar year information for.
  @returns A `LunarYearInfo` instance representing the lunar year information.
  """
  result = LIB.get_lunar_year_info(algo.value, year)

  if not result.valid:
    raise ValueError("Error occurred in get_lunar_year_info.")
  
  month_count = 12 if result.leap_month == 0 else 13
  month_lengths = []
  for i in range(month_count):
    big = (result.month_len >> i) & 1
    month_lengths.append(30 if big else 29)

  return LunarYearInfo(
    first_day = date(result.year, result.month, result.day),
    leap_month = result.leap_month,
    month_lengths = month_lengths,
  )


class _LunarDate(Structure):
  _fields_ = [
    ("valid", c_bool),
    ("year", c_int32),
    ("month", c_uint8),
    ("is_leap", c_bool),
    ("day", c_uint8),
  ]

LIB.gregorian_to_lunar.argtypes = [c_uint8, c_int32, c_uint8, c_uint8]
LIB.gregorian_to_lunar.restype = _LunarDate

@dataclass
class LunarDate:
  year: int
  month: int    # Traditional numbering (1-12).
  is_leap: bool # Whether the month is the leap month.
  day: int

def gregorian_to_lunar(algo: LunarAlgo, year: int, month: int, day: int) -> LunarDate:
  """
  Convert a Gregorian date to a lunar date.

  @param algo The algorithm to use.
  @param year The Gregorian year.
  @param month The Gregorian month.
  @param day The Gregorian day of the month.
  @returns A `LunarDate` instance; the month is in traditional numbering (1-12) plus
           `is_leap` — e.g. 2023-03-22 is the leap 2nd month of lunar 2023
           (month = 2, is_leap = True).
  """
  result = LIB.gregorian_to_lunar(algo.value, year, month, day)

  if not result.valid:
    raise ValueError("Error occurred in gregorian_to_lunar.")

  return LunarDate(
    year = result.year,
    month = result.month,
    is_leap = result.is_leap,
    day = result.day,
  )


class _GregorianDate(Structure):
  _fields_ = [
    ("valid", c_bool),
    ("year", c_int32),
    ("month", c_uint8),
    ("day", c_uint8),
  ]

LIB.lunar_to_gregorian.argtypes = [c_uint8, c_int32, c_uint8, c_bool, c_uint8]
LIB.lunar_to_gregorian.restype = _GregorianDate

def lunar_to_gregorian(algo: LunarAlgo, year: int, month: int, is_leap: bool, day: int) -> date:
  """
  Convert a lunar date to a Gregorian date.

  @param algo The algorithm to use.
  @param year The lunar year.
  @param month The lunar month, in traditional numbering (1-12).
  @param is_leap Whether the month is the leap month; only the year's actual leap month
                 may be flagged, and a leap-less year has none.
  @param day The day of the lunar month.
  @returns A `date` instance representing the Gregorian date.
  """
  result = LIB.lunar_to_gregorian(algo.value, year, month, is_leap, day)

  if not result.valid:
    raise ValueError("Error occurred in lunar_to_gregorian.")

  return date(result.year, result.month, result.day)

#endregion
