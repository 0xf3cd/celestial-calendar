# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Typed centre-altitude conditions and interval-query result decoders."""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
from enum import Enum, StrEnum
from itertools import groupby

from . import _abi
from ._codes import constant, enum_code
from ._models import ModelKey, ModelKind, Target
from ._protocol import finite
from .events import EventDirection, decode_direction
from .observer import RefractionModel, RefractionParams


@dataclass(frozen=True)
class GeometricLevel:
  """Geometric centre-altitude threshold, strictly inside (-90,90) degrees."""

  altitude_deg: float


@dataclass(frozen=True)
class RefractedLevel:
  """Apparent centre-altitude threshold with an explicit refraction model and weather."""

  altitude_deg: float
  params: RefractionParams


class AltitudeSense(StrEnum):
  """Side of the centre-altitude threshold that satisfies a condition."""

  ABOVE = "above"
  BELOW = "below"


@dataclass(frozen=True)
class AltitudeCondition:
  """One physical target, geometric/refracted level and satisfying side."""

  target: Target
  level: GeometricLevel | RefractedLevel
  sense: AltitudeSense


@dataclass(frozen=True)
class AltitudeConditionState:
  """Requested condition, resolved geometric level and truth after start-event replay."""

  condition: AltitudeCondition
  source: ModelKey
  delta_t_model: ModelKey
  geometric_level_deg: float
  initially_satisfied: bool


@dataclass(frozen=True)
class AltitudeEvent:
  """Centre-altitude crossing in JDE(TT), associated with its zero-based condition index."""

  jde_tt: float
  condition_index: int
  direction: EventDirection


@dataclass(frozen=True)
class AltitudeEvents:
  """Single-condition state and detected crossings over the requested half-open TT interval."""

  condition: AltitudeConditionState
  events: tuple[AltitudeEvent, ...]


@dataclass(frozen=True)
class ObservingWindow:
  """Positive-duration operational observing span in JDE(TT)."""

  start_jde_tt: float
  end_jde_tt: float


@dataclass(frozen=True)
class ObservingWindows:
  """Condition states, detected crossings and maximal reconstructed observing spans."""

  conditions: tuple[AltitudeConditionState, ...]
  events: tuple[AltitudeEvent, ...]
  windows: tuple[ObservingWindow, ...]


@dataclass(frozen=True)
class UpperTransit:
  """Forward-side upper meridian passage in JDE(TT), relative to the site's supplied longitude."""

  jde_tt: float
  target: Target
  source: ModelKey
  delta_t_model: ModelKey


def _number(value: float, name: str) -> float:
  if isinstance(value, (bool, Enum)) or not isinstance(value, (int, float)):
    raise TypeError(f"{name} must be a real number")
  try:
    return finite(float(value), name)
  except OverflowError as error:
    raise ValueError(f"{name} must fit a finite native double") from error


def encode_condition(condition: AltitudeCondition) -> _abi.CelestialAltitudeCondition:
  """Normalize only fields belonging to the requested level convention."""
  if not isinstance(condition, AltitudeCondition):
    raise TypeError("condition must be an AltitudeCondition")
  if not isinstance(condition.target, Target) or not isinstance(condition.sense, AltitudeSense):
    raise TypeError("condition requires Target and AltitudeSense values")
  level = condition.level
  if not isinstance(level, (GeometricLevel, RefractedLevel)):
    raise TypeError("condition.level must be GeometricLevel or RefractedLevel")
  code, temperature, pressure = constant("REFRACTION_NONE"), 0.0, 0.0
  if isinstance(level, RefractedLevel):
    if not isinstance(level.params, RefractionParams) or not isinstance(level.params.model, RefractionModel):
      raise TypeError("refracted level requires RefractionParams with a RefractionModel")
    code = enum_code("REFRACTION", level.params.model)
    temperature = _number(level.params.temperature_c, "temperature_c")
    pressure = _number(level.params.pressure_hpa, "pressure_hpa")
  return _abi.CelestialAltitudeCondition(
    enum_code("TARGET", condition.target),
    enum_code("ALTITUDE_SENSE", condition.sense),
    code,
    0,
    _number(level.altitude_deg, "altitude_deg"),
    temperature,
    pressure,
  )


def _unused(record: _abi.CelestialAltitudeRecord, fields: set[str]) -> None:
  if not isinstance(record, _abi.CelestialAltitudeRecord):
    raise TypeError("unexpected altitude stream record")
  if any(getattr(record, name) != 0 for name, _ in record._fields_ if name not in fields | {"tag"}):
    raise ValueError("nonzero unused altitude record field")


def _identities(record: _abi.CelestialAltitudeRecord, target: Target) -> tuple[ModelKey, ModelKey]:
  code = enum_code("TARGET", target)
  if record.target_code != code or not 0 <= code <= 9:
    raise ValueError("unexpected physical altitude target")
  source = {0: "SUN_VSOP87D", 1: "MOON_ELP2000_82B_TRUNC", 9: "PLUTO_PLUTO95_APPARENT"}.get(code, "PLANET_VSOP87D")
  if record.position_source_code != constant("POSITION_SOURCE_" + source):
    raise ValueError("unexpected altitude position source")
  if record.delta_t_model_code != constant("DELTA_T_MODEL_ALGO5"):
    raise ValueError("unexpected altitude clock model")
  return ModelKey(ModelKind.POSITION_SOURCE, record.position_source_code), ModelKey(
    ModelKind.DELTA_T, record.delta_t_model_code
  )


def decode_transits(
  records: Iterable[_abi.CelestialAltitudeRecord], target: Target, start: float, end: float
) -> tuple[UpperTransit, ...]:
  """Require a strictly ordered, in-range transit-only stream with fixed model identities."""
  result = []
  previous = -float("inf")
  for record in records:
    _unused(record, {"target_code", "position_source_code", "delta_t_model_code", "jde_tt"})
    if record.tag != constant("ALTITUDE_RECORD_UPPER_TRANSIT"):
      raise ValueError("unexpected transit record tag")
    time = finite(record.jde_tt, "jde_tt")
    if not start <= time < end or time <= previous:
      raise ValueError("transit outside interval or not strictly ordered")
    source, clock = _identities(record, target)
    result.append(UpperTransit(time, target, source, clock))
    previous = time
  return tuple(result)


def _windows(
  states: tuple[AltitudeConditionState, ...], events: tuple[AltitudeEvent, ...], start: float, end: float
) -> tuple[ObservingWindow, ...]:
  """Replay equal-time groups atomically; start events are already in the initial state."""
  satisfied = [state.initially_satisfied for state in states]
  unsatisfied = sum(not value for value in satisfied)
  opening = start
  spans = []
  for time, group in groupby(events, key=lambda event: event.jde_tt):
    if time == start:
      continue
    was_active = unsatisfied == 0
    for event in group:
      index = event.condition_index
      above = event.direction is EventDirection.INCREASING
      value = above if states[index].condition.sense is AltitudeSense.ABOVE else not above
      unsatisfied += int(not value) - int(not satisfied[index])
      satisfied[index] = value
    active = unsatisfied == 0
    if was_active and not active and opening < time:
      spans.append(ObservingWindow(opening, time))
    elif not was_active and active:
      opening = time
  if unsatisfied == 0 and opening < end:
    spans.append(ObservingWindow(opening, end))
  return tuple(spans)


def decode_conditions(
  records: Iterable[_abi.CelestialAltitudeRecord],
  requested: tuple[AltitudeCondition, ...],
  start: float,
  end: float,
  windows: bool,
) -> AltitudeEvents | ObservingWindows:
  """Require ordered metadata/events/windows and preserve every condition association."""
  records = tuple(records)
  if len(records) < len(requested):
    raise ValueError("missing altitude condition metadata")
  states = []
  for index, condition in enumerate(requested):
    record = records[index]
    _unused(
      record,
      {
        "condition_index",
        "target_code",
        "position_source_code",
        "delta_t_model_code",
        "refraction_code",
        "sense_code",
        "initially_satisfied",
        "requested_level_deg",
        "geometric_level_deg",
        "temperature_c",
        "pressure_hpa",
      },
    )
    if record.tag != constant("ALTITUDE_RECORD_CONDITION") or record.condition_index != index:
      raise ValueError("missing or misordered altitude condition metadata")
    expected = encode_condition(condition)
    for field, requested_field in (
      ("refraction_code", "refraction_code"),
      ("sense_code", "sense_code"),
      ("requested_level_deg", "level_deg"),
      ("temperature_c", "temperature_c"),
      ("pressure_hpa", "pressure_hpa"),
    ):
      if getattr(record, field) != getattr(expected, requested_field):
        raise ValueError("altitude metadata differs from the requested condition")
    geometric = finite(record.geometric_level_deg, "geometric_level_deg")
    if not -90.0 <= geometric <= 90.0 or record.initially_satisfied not in (0, 1):
      raise ValueError("invalid resolved altitude level or condition state")
    if isinstance(condition.level, GeometricLevel) and geometric != condition.level.altitude_deg:
      raise ValueError("geometric threshold changed in the native result")
    source, clock = _identities(record, condition.target)
    states.append(AltitudeConditionState(condition, source, clock, geometric, bool(record.initially_satisfied)))
  events, spans = [], []
  previous = (-float("inf"), -1)
  for record in records[len(requested) :]:
    if record.tag == constant("ALTITUDE_RECORD_EVENT") and not spans:
      _unused(record, {"condition_index", "direction_code", "jde_tt"})
      time = finite(record.jde_tt, "jde_tt")
      key = (time, record.condition_index)
      if not 0 <= record.condition_index < len(requested) or not start <= time < end or key <= previous:
        raise ValueError("altitude event has invalid association, interval or order")
      direction = decode_direction(record.direction_code)
      if direction is EventDirection.TANGENT:
        raise ValueError("altitude event must be a crossing")
      events.append(AltitudeEvent(time, record.condition_index, direction))
      previous = key
    elif record.tag == constant("ALTITUDE_RECORD_WINDOW") and windows:
      _unused(record, {"jde_tt", "end_jde_tt"})
      left, right = finite(record.jde_tt, "jde_tt"), finite(record.end_jde_tt, "end_jde_tt")
      if not start <= left < right <= end or (spans and spans[-1].end_jde_tt >= left):
        raise ValueError("observing windows overlap, touch or leave the requested interval")
      spans.append(ObservingWindow(left, right))
    else:
      raise ValueError("unexpected altitude stream tag or record order")
  if windows:
    if tuple(spans) != _windows(tuple(states), tuple(events), start, end):
      raise ValueError("observing windows disagree with condition states and companion events")
    return ObservingWindows(tuple(states), tuple(events), tuple(spans))
  if len(states) != 1:
    raise ValueError("altitude query requires exactly one condition state")
  return AltitudeEvents(states[0], tuple(events))
