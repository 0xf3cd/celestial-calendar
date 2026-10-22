# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Capability records and model identities for the public query surface (#357).

Every domain bound is computed by the native layer from the operation's own defining
constants; this module adds no second table of numbers. NaN domain ends mean "unbounded",
never a hidden sentinel, and a record never implies accuracy — measured validation
coverage lives only in the prose of `celestial_capability_text`.
"""

from __future__ import annotations

from dataclasses import dataclass as _dataclass
from enum import IntEnum as _IntEnum
from enum import StrEnum as _StrEnum
from typing import Iterable as _Iterable

from . import _abi


class CapabilityOperation(_StrEnum):
  """A describable operation on the public query surface.

  MODEL_IDENTITY is text-lookup only: `capabilities()` rejects it, while
  `capability_text` uses it to address the identity registry.
  """

  POSITION = "position"
  LONGITUDE_RATE = "longitude_rate"
  EVENTS = "events"
  HOUSES = "houses"
  MODEL_IDENTITY = "model_identity"


class CapabilityStatus(_StrEnum):
  """What kind of bound a capability domain carries."""

  SOURCE_WINDOW = "source_window"
  IMPLEMENTATION_WINDOW = "implementation_window"
  UNBOUNDED_FINITE = "unbounded_finite"


class PositionSource(_StrEnum):
  """Stable identity of a position computation, per target (#357)."""

  SUN_VSOP87D = "sun_vsop87d"
  MOON_ELP2000_82B_TRUNC = "moon_elp2000_82b_trunc"
  PLANET_VSOP87D = "planet_vsop87d"
  PLUTO_PLUTO95_APPARENT = "pluto_pluto95_apparent"
  NODE_MEAN = "node_mean"
  NODE_TRUE_MEEUS_FIVE_TERM = "node_true_meeus_five_term"


class ModelKind(_StrEnum):
  """Model family for the identity registry."""

  DELTA_T = "delta_t"
  NUTATION = "nutation"
  POSITION_SOURCE = "position_source"


class ModelStatus(_StrEnum):
  """Lifecycle status of a registered model."""

  CURRENT = "current"
  FROZEN = "frozen"


class CapabilitySubject(_IntEnum):
  """The fourteen subjects a per-target capability record can describe (CHART_TARGET_* codes)."""

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


@_dataclass(frozen=True)
class CapabilityRecord:
  """One operation's accepted-input description for one subject.

  `domain_start_jde_tt` / `domain_end_jde_tt` are the half-open accepted range in JDE(TT);
  NaN marks the absence of an explicit bound. `maximum_span_tt_days` limits EVENTS queries;
  0 means no span limit.
  """

  operation: CapabilityOperation
  subject: CapabilitySubject | None
  status: CapabilityStatus
  source: PositionSource | None
  domain_start_jde_tt: float
  domain_end_jde_tt: float
  maximum_span_tt_days: int


@_dataclass(frozen=True)
class ModelIdentity:
  """One registered model identity with its applicability.

  `default_alias_code` is the boundary code this identity answers to as the default
  (0 for Delta T algo5, the resolved meaning of DEFAULT); None means no alias.
  """

  kind: ModelKind
  identity_code: int
  status: ModelStatus
  default_alias_code: int | None
  domain_start_jde_tt: float
  domain_end_jde_tt: float


_OPERATION_CODE = {
  CapabilityOperation.POSITION: 0,
  CapabilityOperation.LONGITUDE_RATE: 1,
  CapabilityOperation.EVENTS: 2,
  CapabilityOperation.HOUSES: 3,
  CapabilityOperation.MODEL_IDENTITY: 4,
}

_STATUS = {
  0: CapabilityStatus.SOURCE_WINDOW,
  1: CapabilityStatus.IMPLEMENTATION_WINDOW,
  2: CapabilityStatus.UNBOUNDED_FINITE,
}

_SOURCE = {
  0: PositionSource.SUN_VSOP87D,
  1: PositionSource.MOON_ELP2000_82B_TRUNC,
  2: PositionSource.PLANET_VSOP87D,
  3: PositionSource.PLUTO_PLUTO95_APPARENT,
  4: PositionSource.NODE_MEAN,
  5: PositionSource.NODE_TRUE_MEEUS_FIVE_TERM,
}


def position_source(code: int) -> PositionSource | None:
  """Resolve a CAPABILITY_SOURCE_* code; unknown codes return None."""
  return _SOURCE.get(code)


_KIND_CODE = {
  ModelKind.DELTA_T: 0,
  ModelKind.NUTATION: 1,
  ModelKind.POSITION_SOURCE: 2,
}

_KIND = {code: kind for kind, code in _KIND_CODE.items()}
_MODEL_STATUS = {
  0: ModelStatus.CURRENT,
  1: ModelStatus.FROZEN,
}

_NO_ALIAS = 0xFFFFFFFF


def operation_code(operation: CapabilityOperation) -> int:
  """Map one CapabilityOperation to its CAPABILITY_OPERATION_* code."""
  return _OPERATION_CODE[operation]


def kind_code(kind: ModelKind) -> int:
  """Map one ModelKind to its MODEL_KIND_* code."""
  return _KIND_CODE[kind]


def decode_capability_records(
  operation: CapabilityOperation,
  records: _Iterable[_abi.CapabilityRecordV1],
) -> tuple[CapabilityRecord, ...]:
  """Copy native capability records into frozen dataclasses."""
  return tuple(
    CapabilityRecord(
      operation,
      CapabilitySubject(record.subject_code) if record.operation_code != 3 else None,
      _STATUS[record.status_code],
      _SOURCE.get(record.source_code) if record.operation_code != 3 else None,
      record.domain_start_jde_tt,
      record.domain_end_jde_tt,
      record.maximum_span_tt_days,
    )
    for record in records
  )


def decode_model_identities(records: _Iterable[_abi.ModelIdentityRecordV1]) -> tuple[ModelIdentity, ...]:
  """Copy native model identity records into frozen dataclasses."""
  return tuple(
    ModelIdentity(
      _KIND[record.kind_code],
      record.identity_code,
      _MODEL_STATUS[record.status_code],
      None if record.default_alias_code == _NO_ALIAS else record.default_alias_code,
      record.domain_start_jde_tt,
      record.domain_end_jde_tt,
    )
    for record in records
  )
