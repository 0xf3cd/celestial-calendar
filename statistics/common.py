import sys
from pathlib import Path
from dataclasses import dataclass

import ctypes
from ctypes import c_int32, c_uint32, c_double, c_bool, Structure


def dynamic_lib_ext() -> str:
  '''Return the library extension for the current platform.'''
  if sys.platform == 'win32':
    return '.dll'
  elif sys.platform == 'darwin':
    return '.dylib'
  elif sys.platform == 'linux':
    return '.so'
  raise OSError(f'Unsupported platform: {sys.platform}')


# Define constants for paths.
PROJ_PATH        = Path(__file__).parent.parent
USNO_DATA_PATH   = Path(__file__).parent / 'usno_data.txt'
BINDINGS_PATH    = PROJ_PATH / 'build' / 'shared_lib'
LIB_PATH         = BINDINGS_PATH / ('libcelestial_calendar' + dynamic_lib_ext())

assert PROJ_PATH.exists(),        f"Project path not found: {PROJ_PATH}"
assert USNO_DATA_PATH.exists(),   f"USNO data not found: {USNO_DATA_PATH}"
assert BINDINGS_PATH.exists(),    f"Bindings path not found: {BINDINGS_PATH}"
assert LIB_PATH.exists(),         f"Shared library not found: {LIB_PATH}"


# Define the argument and return types of the C functions.
LIB = ctypes.CDLL(LIB_PATH)


#region Delta T functions

LIB.delta_t_algo1.argtypes = [c_double]
LIB.delta_t_algo1.restype = c_double

LIB.delta_t_algo2.argtypes = [c_double]
LIB.delta_t_algo2.restype = c_double

LIB.delta_t_algo3.argtypes = [c_double]
LIB.delta_t_algo3.restype = c_double

LIB.delta_t_algo4.argtypes = [c_double]
LIB.delta_t_algo4.restype = c_double


# Wrap C functions with Python functions, so that they can be called from Python.
def delta_t_algo1(year: float) -> float:
  if year < -4000:
    raise ValueError(f"Year {year} is not supported by algorithm 1.")
  return LIB.delta_t_algo1(year)

def delta_t_algo2(year: float) -> float:
  return LIB.delta_t_algo2(year)

def delta_t_algo3(year: float) -> float:
  if year >= 3000:
    raise ValueError(f"Year {year} is not supported by algorithm 3.")
  return LIB.delta_t_algo3(year)

def delta_t_algo4(year: float) -> float:
  if year >= 2035:
    raise ValueError(f"Year {year} is not supported by algorithm 4.")
  return LIB.delta_t_algo4(year)

#endregion


#region Astro functions

# Define the JulianDay struct
class _JulianDay(Structure):
  _fields_ = [
    ('valid', c_bool  ),
    ('value', c_double),
  ]

# Define the SunCoordinate struct
class _SunCoordinate(Structure):
  _fields_ = [
    ('valid', c_bool  ),
    ('lon',   c_double),
    ('lat',   c_double),
    ('r',     c_double),
  ]

# Define the function signatures
LIB.ut1_to_jd.argtypes = [c_int32, c_uint32, c_uint32, c_double]
LIB.ut1_to_jd.restype = _JulianDay

LIB.ut1_to_jde.argtypes = [c_int32, c_uint32, c_uint32, c_double]
LIB.ut1_to_jde.restype = _JulianDay

LIB.sun_apparent_geocentric_coord.argtypes = [c_double]
LIB.sun_apparent_geocentric_coord.restype = _SunCoordinate


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
    raise ValueError(f"Error occurred in ut1_to_jd.")

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
  if y >= 2035:
    raise ValueError(f"Year {y} is not supported by algorithm 4."
                      "Algorithm 4 is the underlying algorithm to compute Delta T when converting ut1 to jde."
                      "Considering use `ut1_to_jd` instead, though some accuracy may be lost due to Delta T.")
  
  jde = LIB.ut1_to_jde(y, m, d, fraction)

  if not jde.valid:
    raise ValueError(f"Error occurred in ut1_to_jde.")

  return jde.value


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
    raise ValueError(f"Error occurred in sun_apparent_geocentric_coord.")

  return SunCoordinate(
    lon = coord.lon,
    lat = coord.lat,
    r   = coord.r,
  )

#endregion
