import sys
from pathlib import Path
from datetime import date, datetime, timedelta
from dataclasses import dataclass

import ctypes
from ctypes import (
  c_int32, c_uint32,  c_uint16, c_uint8, c_double, c_bool, 
  POINTER, Structure
)

from typing import Optional, List


def dynamic_lib_ext() -> str:
  '''Return the library extension for the current platform.'''
  if sys.platform == 'win32':
    return '.dll'
  elif sys.platform == 'darwin':
    return '.dylib'
  elif sys.platform == 'linux':
    return '.so'
  raise OSError(f'Unsupported platform: {sys.platform}')


def search_lib_path(folder: Path) -> Optional[Path]:
  '''Search for the shared library in the given folder.'''
  expected_ext = dynamic_lib_ext()

  for path in folder.iterdir():
    if not path.is_file():
      continue
    if expected_ext not in path.name:
      continue
    if 'celestial_calendar' not in path.name:
      continue
    return path


# Define constants for paths.
PROJ_PATH        = Path(__file__).parent.parent
USNO_DATA_PATH   = Path(__file__).parent / 'usno_data.txt'
BINDINGS_PATH    = PROJ_PATH / 'build' / 'shared_lib'
LIB_PATH         = search_lib_path(BINDINGS_PATH)

assert PROJ_PATH.exists(),        f"Project path not found: {PROJ_PATH}"
assert USNO_DATA_PATH.exists(),   f"USNO data not found: {USNO_DATA_PATH}"
assert BINDINGS_PATH.exists(),    f"Bindings path not found: {BINDINGS_PATH}"

assert LIB_PATH is not None,      f"Shared library not found in {BINDINGS_PATH}"
assert LIB_PATH.exists(),         f"Shared library not found: {LIB_PATH}"


# Define the argument and return types of the C functions.
LIB = ctypes.CDLL(str(LIB_PATH))


#region Delta T functions

class DeltaT(Structure):
  _fields_ = [
    ('valid', c_bool),
    ('value', c_double),
  ]

LIB.delta_t_algo1.argtypes = [c_double]
LIB.delta_t_algo1.restype = DeltaT

LIB.delta_t_algo2.argtypes = [c_double]
LIB.delta_t_algo2.restype = DeltaT

LIB.delta_t_algo3.argtypes = [c_double]
LIB.delta_t_algo3.restype = DeltaT

LIB.delta_t_algo4.argtypes = [c_double]
LIB.delta_t_algo4.restype = DeltaT


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
  

#endregion


#region Delta T and Julian Day

# Define the JulianDay struct
class _JulianDay(Structure):
  _fields_ = [
    ('valid', c_bool  ),
    ('value', c_double),
  ]

# Define the UT1Time struct
class _UT1Time(Structure):
  _fields_ = [
    ('valid', c_bool),
    ('year',  c_int32),
    ('month', c_uint32),
    ('day',   c_uint32),
    ('fraction', c_double),
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
  '''
  @brief Convert UT1 datetime to Julian Day Number (JD).
  @param y The year.
  @param m The month.
  @param d The day.
  @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
  @returns The Julian Day Number (JD).
  '''
  jd = LIB.ut1_to_jd(y, m, d, fraction)

  if not jd.valid:
    raise ValueError("Error occurred in ut1_to_jd.")

  return jd.value

def ut1_to_jde(y: int, m: int, d: int, fraction: float) -> float:
  '''
  @brief Convert UT1 datetime to Julian Ephemeris Day Number (JDE).
  @param y The year.
  @param m The month.
  @param d The day.
  @param fraction The fraction of the day. Must be in the range [0.0, 1.0).
  @returns The Julian Ephemeris Day Number (JDE).
  '''
  jde = LIB.ut1_to_jde(y, m, d, fraction)

  if not jde.valid:
    raise ValueError("Error occurred in ut1_to_jde.")

  return jde.value


def jde_to_ut1(jde: float) -> datetime:
  '''
  @brief Convert Julian Ephemeris Day Number (JDE) to UT1 datetime.
  @param jde The julian ephemeris day number, which is based on TT.
  @returns A `datetime` object representing the UT1 datetime.
  '''
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
    ('valid', c_bool  ),
    ('lon',   c_double),
    ('lat',   c_double),
    ('r',     c_double),
  ]

# Define the MoonCoordinate struct
class _MoonCoordinate(Structure):
  _fields_ = [
    ('valid', c_bool  ),
    ('lon',   c_double),
    ('lat',   c_double),
    ('r',     c_double),
  ]


LIB.sun_apparent_geocentric_coord.argtypes = [c_double]
LIB.sun_apparent_geocentric_coord.restype = _SunCoordinate

LIB.moon_apparent_geocentric_coord.argtypes = [c_double]
LIB.moon_apparent_geocentric_coord.restype = _MoonCoordinate


@dataclass
class SunCoordinate:
  lon: float # In degrees
  lat: float # In degrees
  r:   float # In AU

def sun_apparent_geocentric_coord(jde: float) -> SunCoordinate:
  '''
  @brief Compute the apparent geocentric coordinates of the Sun.
  @param jde The julian ephemeris day number, which is based on TT.
  @returns A `SunCoordinate` representing the apparent geocentric coordinates of the Sun.
  '''
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
  '''
  @brief Compute the apparent geocentric coordinates of the Moon.
  @param jde The julian ephemeris day number, which is based on TT.
  @returns A `MoonCoordinate` representing the apparent geocentric coordinates of the Moon.
  '''
  coord = LIB.moon_apparent_geocentric_coord(jde)

  if not coord.valid:
    raise ValueError("Error occurred in moon_apparent_geocentric_coord.")

  return MoonCoordinate(
    lon = coord.lon,
    lat = coord.lat,
    r   = coord.r,
  )

#endregion


#region New Moon

LIB.new_moons_in_year.argtypes = [c_int32, POINTER(c_uint32), POINTER(c_double), c_uint32]
LIB.new_moons_in_year.restype = c_uint32

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

  # Ensure the number of written slots does not exceed the allocated slot count.
  assert num_written <= slot_count

  # Return the result as an instance of the NewMoons data class.
  jdes = [slots[i] for i in range(num_written)]
  moments = [jde_to_ut1(jde) for jde in jdes]
  return NewMoons(
    year=year,
    new_moon_jdes=jdes,
    new_moon_moments=moments,
  )

#endregion


#region Lunar Year

class _SupportedLunarYearRange(Structure):
  _fields_ = [
    ('valid', c_bool),
    ('start', c_int32),
    ('end', c_int32),
  ]

LIB.get_supported_lunar_year_range.argtypes = [c_uint8]
LIB.get_supported_lunar_year_range.restype = _SupportedLunarYearRange

@dataclass
class SupportedLunarYearRange:
  start: int
  end: int

def get_supported_lunar_year_range(algo: int) -> SupportedLunarYearRange:
  '''
  Return the supported lunar year range for the specified algorithm.

  @param algo The algorithm to use. The supported values are 1 and 2.
  @returns A `SupportedLunarYearRange` instance representing the supported lunar year range.
  '''
  if algo not in [1, 2]:
    raise ValueError("algo must be 1 or 2.")

  result = LIB.get_supported_lunar_year_range(algo)

  if not result.valid:
    raise ValueError("Error occurred in get_supported_lunar_year_range.")

  return SupportedLunarYearRange(
    start = result.start,
    end   = result.end,
  )


class _LunarYearInfo(Structure):
  _fields_ = [
    ('valid', c_bool),
    ('year', c_int32),
    ('month', c_uint8),
    ('day', c_uint8),
    ('leap_month', c_uint8),
    ('month_len', c_uint16),
  ]

LIB.get_lunar_year_info.argtypes = [c_uint8, c_int32]
LIB.get_lunar_year_info.restype = _LunarYearInfo

@dataclass
class LunarYearInfo:
  first_day: date # The first day of the lunar year in the Gregorian calendar.
  leap_month: int # The month of the leap month (1 <= leap_month <= 12). 0 if there is no leap month.
  month_lengths: List[int] # The number of days in each month of the lunar year.

def get_lunar_year_info(algo: int, year: int) -> LunarYearInfo:
  '''
  Return the lunar year information for the specified year.

  @param algo The algorithm to use. The supported values are 1 and 2.
  @param year The year to get the lunar year information for.
  @returns A `LunarYearInfo` instance representing the lunar year information.
  '''
  if algo not in [1, 2]:
    raise ValueError("algo must be 1 or 2.")

  result = LIB.get_lunar_year_info(algo, year)

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

#endregion
