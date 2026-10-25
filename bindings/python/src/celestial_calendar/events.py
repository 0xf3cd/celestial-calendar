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
from enum import StrEnum as _StrEnum
from typing import Iterable as _Iterable

from . import _abi
from ._codes import enum_member
from ._protocol import finite


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

  Time is JDE(TT); apparent geocentric longitude is in unnormalized degrees and rate is
  degrees per TT day.
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

  Time is JDE(TT), apparent geocentric longitudes are unnormalized degrees, and rates
  are degrees per TT day. signed_difference_deg is first minus second in (-180, 180] degrees.
  """

  jde_tt: float
  longitude_a_deg: float
  longitude_b_deg: float
  longitude_rate_a_deg_per_tt_day: float
  longitude_rate_b_deg_per_tt_day: float
  signed_difference_deg: float
  direction: EventDirection


def decode_direction(code: int) -> EventDirection:
  return enum_member(EventDirection, "EVENT_DIRECTION", code)


def decode_motion(code: int) -> StationMotion:
  return enum_member(StationMotion, "EVENT_MOTION", code)


def decode_longitude_events(records: _Iterable[_abi.CelestialLongitudeEventRecord]) -> tuple[LongitudeEvent, ...]:
  """Copy native longitude-event records into frozen dataclasses."""
  return tuple(
    LongitudeEvent(
      finite(record.jde_tt, "jde_tt"),
      finite(record.longitude_deg, "longitude_deg"),
      finite(record.longitude_rate_deg_per_tt_day, "longitude_rate_deg_per_tt_day"),
      decode_direction(record.direction_code),
    )
    for record in records
  )


def decode_station_events(records: _Iterable[_abi.CelestialStationEventRecord]) -> tuple[StationEvent, ...]:
  """Copy native station-event records into frozen dataclasses."""
  return tuple(
    StationEvent(
      finite(record.jde_tt, "jde_tt"),
      finite(record.longitude_deg, "longitude_deg"),
      finite(record.longitude_rate_deg_per_tt_day, "longitude_rate_deg_per_tt_day"),
      decode_motion(record.motion_before_code),
      decode_motion(record.motion_after_code),
    )
    for record in records
  )


def decode_difference_events(
  records: _Iterable[_abi.CelestialLongitudeDifferenceEventRecord],
) -> tuple[LongitudeDifferenceEvent, ...]:
  """Copy native difference-event records into frozen dataclasses."""
  records = tuple(records)
  if any(not -180.0 < record.signed_difference_deg <= 180.0 for record in records):
    raise ValueError("difference is outside (-180, 180]")
  return tuple(
    LongitudeDifferenceEvent(
      finite(record.jde_tt, "jde_tt"),
      finite(record.longitude_a_deg, "longitude_a_deg"),
      finite(record.longitude_b_deg, "longitude_b_deg"),
      finite(record.longitude_rate_a_deg_per_tt_day, "longitude_rate_a_deg_per_tt_day"),
      finite(record.longitude_rate_b_deg_per_tt_day, "longitude_rate_b_deg_per_tt_day"),
      finite(record.signed_difference_deg, "signed_difference_deg"),
      decode_direction(record.direction_code),
    )
    for record in records
  )
