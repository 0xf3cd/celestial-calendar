# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Load the packaged native library and bind the declared C entry points."""

from __future__ import annotations

import atexit
import ctypes
import sys
from importlib import resources

from ._abi import BINDING_SPECS as BINDING_SPECS
from ._abi import RECORDING_EXPORTS as RECORDING_EXPORTS
from ._abi import STRUCT_TYPES as STRUCT_TYPES
from ._abi import P_U32 as P_U32
from ._abi import P_DOUBLE as P_DOUBLE
from ._abi import P_CHAR as P_CHAR
from ._abi import JulianDay as JulianDay
from ._abi import UT1Time as UT1Time
from ._abi import SunCoordinate as SunCoordinate
from ._abi import MoonCoordinate as MoonCoordinate
from ._abi import MoonIllumination as MoonIllumination
from ._abi import MoonPositionAngle as MoonPositionAngle
from ._abi import Discriminant as Discriminant
from ._abi import EquationOfTime as EquationOfTime
from ._abi import ApparentSolarTime as ApparentSolarTime
from ._abi import SiderealTime as SiderealTime
from ._abi import JieqiMomentQuery as JieqiMomentQuery
from ._abi import SupportedLunarYearRange as SupportedLunarYearRange
from ._abi import LunarYearInfo as LunarYearInfo
from ._abi import LunarDate as LunarDate
from ._abi import GregorianDate as GregorianDate
from ._abi import DeltaT as DeltaT
from ._abi import ChartBodyV1 as ChartBodyV1
from ._abi import ChartHousesV1 as ChartHousesV1
from ._abi import ChartSnapshotV1 as ChartSnapshotV1
from ._abi import LongitudeDifferenceEventRecord as LongitudeDifferenceEventRecord
from ._abi import LongitudeEventRecord as LongitudeEventRecord
from ._abi import StationEventRecord as StationEventRecord


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
