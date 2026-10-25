# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Access the checked-in C constants generated with the ctypes declarations."""

from enum import Enum
from typing import TypeVar

from ._abi import C_CODES


def constant(name: str) -> int:
  return C_CODES[f"CELESTIAL_{name}"]


def enum_code(family: str, member: Enum) -> int:
  return constant(f"{family}_{member.name}")


_Enum = TypeVar("_Enum", bound=Enum)


def enum_member(enum_type: type[_Enum], family: str, code: int) -> _Enum:
  for member in enum_type:
    if enum_code(family, member) == code:
      return member
  raise ValueError(f"unknown {family} code {code}")


def family_values(family: str) -> frozenset[int]:
  prefix = f"CELESTIAL_{family}_"
  return frozenset(value for name, value in C_CODES.items() if name.startswith(prefix))
