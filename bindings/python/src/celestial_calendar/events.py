# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Event types and native decoders for the interval longitude-event exports (#303)."""

from __future__ import annotations

from dataclasses import dataclass as _dataclass
from enum import IntEnum as _IntEnum
from enum import StrEnum as _StrEnum
from typing import Iterable as _Iterable

from . import _abi


class EventTarget(_IntEnum):
  """One of the fourteen moving bodies or lunar nodes an event query can follow."""

  SUN = 0
  MOON = 1
  MERCURY = 2
  VENUS = 3
  MARS = 4
  JUPITER = 5
  SATURN = 6
  URANUS = 7
  NEPTUNE = 8
  PLUTO = 9
  MEAN_ASCENDING = 10
  MEAN_DESCENDING = 11
  TRUE_ASCENDING = 12
  TRUE_DESCENDING = 13


class EventDirection(_StrEnum):
  """How the queried level is approached on the two sides of an event."""

  INCREASING = "increasing"
  DECREASING = "decreasing"
  TANGENT = "tangent"


class StationMotion(_StrEnum):
  """The certified sign of longitude motion on one side of a stationary instant."""

  DIRECT = "direct"
  RETROGRADE = "retrograde"


@_dataclass(frozen=True)
class LongitudeEvent:
  """A crossing or exact numerical contact at a supplied longitude.

  Times are JDE(TT) on the published 2^-30-day reporting lattice; rates are degrees
  per TT day from the published seven-point operator. `longitude_deg` is the apparent
  geocentric longitude at the event, not normalized — values on either side of a
  branch cut stay raw.
  """

  jde_tt: float
  longitude_deg: float
  longitude_rate_deg_per_tt_day: float
  direction: EventDirection


@_dataclass(frozen=True)
class StationEvent:
  """A stationary instant of the published seven-point longitude rate.

  The reported rate need not be bitwise zero; the motion codes describe the certified
  sign on each side.
  """

  jde_tt: float
  longitude_deg: float
  longitude_rate_deg_per_tt_day: float
  motion_before: StationMotion
  motion_after: StationMotion


@_dataclass(frozen=True)
class LongitudeDifferenceEvent:
  """An event of the directed, canonical longitude difference first minus second.

  `signed_difference_deg` is the canonical (-180, 180] value of first minus second.
  """

  jde_tt: float
  longitude_a_deg: float
  longitude_b_deg: float
  longitude_rate_a_deg_per_tt_day: float
  longitude_rate_b_deg_per_tt_day: float
  signed_difference_deg: float
  direction: EventDirection


_EVENT_DIRECTION = {
  0: EventDirection.INCREASING,
  1: EventDirection.DECREASING,
  2: EventDirection.TANGENT,
}
_STATION_MOTION = {
  0: StationMotion.DIRECT,
  1: StationMotion.RETROGRADE,
}


def decode_direction(code: int) -> EventDirection:
  """Map one EVENT_DIRECTION_* code to its enum member."""
  try:
    return _EVENT_DIRECTION[code]
  except KeyError as error:
    raise ValueError(f"Unsupported event direction code: {code}") from error


def decode_motion(code: int) -> StationMotion:
  """Map one EVENT_MOTION_* code to its enum member."""
  try:
    return _STATION_MOTION[code]
  except KeyError as error:
    raise ValueError(f"Unsupported station motion code: {code}") from error


def decode_longitude_events(records: _Iterable[_abi.LongitudeEventRecord]) -> tuple[LongitudeEvent, ...]:
  """Copy native longitude-event records into frozen dataclasses."""
  return tuple(
    LongitudeEvent(
      record.jde_tt,
      record.longitude_deg,
      record.longitude_rate_deg_per_tt_day,
      decode_direction(record.direction_code),
    )
    for record in records
  )


def decode_station_events(records: _Iterable[_abi.StationEventRecord]) -> tuple[StationEvent, ...]:
  """Copy native station-event records into frozen dataclasses."""
  return tuple(
    StationEvent(
      record.jde_tt,
      record.longitude_deg,
      record.longitude_rate_deg_per_tt_day,
      decode_motion(record.motion_before_code),
      decode_motion(record.motion_after_code),
    )
    for record in records
  )


def decode_difference_events(
  records: _Iterable[_abi.LongitudeDifferenceEventRecord],
) -> tuple[LongitudeDifferenceEvent, ...]:
  """Copy native difference-event records into frozen dataclasses."""
  return tuple(
    LongitudeDifferenceEvent(
      record.jde_tt,
      record.longitude_a_deg,
      record.longitude_b_deg,
      record.longitude_rate_a_deg_per_tt_day,
      record.longitude_rate_b_deg_per_tt_day,
      record.signed_difference_deg,
      decode_direction(record.direction_code),
    )
    for record in records
  )
