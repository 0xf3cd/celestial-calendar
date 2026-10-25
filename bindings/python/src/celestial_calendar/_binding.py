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
  message = FUNCTIONS["celestial_last_error"]()
  return "" if message is None else message.decode("utf-8", errors="replace")


def last_error_kind() -> int:
  """Read the current thread's error kind without clearing its state."""
  return FUNCTIONS["celestial_last_error_kind"]()
