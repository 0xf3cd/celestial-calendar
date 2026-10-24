# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Native-owned TT domains, civil-house policy and model registry values."""

from __future__ import annotations

import math as _math
from dataclasses import dataclass as _dataclass
from datetime import date as _date
from enum import StrEnum as _StrEnum

from . import _abi
from ._codes import enum_code
from ._models import ModelKey, ModelKind, Target, identity_values
from ._protocol import finite
from .chart import CivilScale


class CapabilityOperation(_StrEnum):
  """Operation described by capability records or operation text."""

  POSITION = "position"
  LONGITUDE_RATE = "longitude_rate"
  EVENTS = "events"
  HOUSES = "houses"
  EQUATORIAL = "equatorial"
  CHART = "chart"
  DELTA_T = "delta_t"


class CapabilityStatus(_StrEnum):
  """Meaning of a domain or admission policy, never an accuracy guarantee.

  FINITE_INPUT_EVALUATION declares no hard input interval. Evaluation at a finite
  epoch may still fail and establishes neither accuracy nor physical plausibility.
  """

  SOURCE_WINDOW = "source_window"
  IMPLEMENTATION_WINDOW = "implementation_window"
  FINITE_INPUT_EVALUATION = "finite_input_evaluation"


class ModelStatus(_StrEnum):
  """Lifecycle status of a registered model."""

  CURRENT = "current"
  FROZEN = "frozen"


@_dataclass(frozen=True)
class CapabilityRecord:
  """One target's TT domain: inclusive start, exclusive end, NaN for no bound.

  maximum_span_tt_days limits event queries; zero means no span limit.
  """

  operation: CapabilityOperation
  subject: Target
  status: CapabilityStatus
  source: ModelKey
  domain_start_jde_tt: float
  domain_end_jde_tt: float
  maximum_span_tt_days: int


@_dataclass(frozen=True)
class CivilHouseCapability:
  """Civil-house admission policy, separate from geometric solvability.

  Civil years form [civil_start_year, civil_end_year_exclusive); utc_start_date is
  an inclusive (year, month, day). An optional derived_tt_end_jde_tt is exclusive.
  UT1 derived from future UTC uses the held leap table and selected Delta T model,
  not observed DUT1. Admission does not promise successful house geometry.
  """

  status: CapabilityStatus
  civil_start_year: int
  civil_end_year_exclusive: int
  civil_scales: tuple[CivilScale, ...]
  utc_start_date: tuple[int, int, int]
  delta_t_model: ModelKey
  derived_tt_end_jde_tt: float | None


@_dataclass(frozen=True)
class ModelIdentity:
  """Registered key, lifecycle status and native applicability in JDE(TT).

  NaN endpoints have no declared bound. DEFAULT belongs to operation selectors,
  not the identity registry; operation text describes its resolution.
  """

  key: ModelKey
  status: ModelStatus
  domain_start_jde_tt: float
  domain_end_jde_tt: float


def _member(enum_type, family: str, code: int):
  for member in enum_type:
    if enum_code(family, member) == code:
      return member
  raise ValueError(f"unknown {family} code {code}")


def _domain(start: float, end: float) -> tuple[float, float]:
  if _math.isinf(start) or _math.isinf(end) or (not _math.isnan(start) and not _math.isnan(end) and start >= end):
    raise ValueError("invalid domain endpoints")
  return start, end


def decode_capability_records(
  operation: CapabilityOperation, records, expected_targets: tuple[Target, ...]
) -> tuple[CapabilityRecord, ...]:
  if len(records) != len(expected_targets):
    raise ValueError("unexpected capability count")
  decoded = []
  for record, target in zip(records, expected_targets, strict=True):
    if not isinstance(record, _abi.CelestialCapabilityRecord):
      raise TypeError("unexpected capability record")
    if _member(CapabilityOperation, "OPERATION", record.operation_code) is not operation:
      raise ValueError("unexpected capability operation")
    if _member(Target, "TARGET", record.subject_code) is not target:
      raise ValueError("unexpected capability target")
    start, end = _domain(record.domain_start_jde_tt, record.domain_end_jde_tt)
    decoded.append(
      CapabilityRecord(
        operation,
        target,
        _member(CapabilityStatus, "STATUS", record.status_code),
        ModelKey(ModelKind.POSITION_SOURCE, record.source_code),
        start,
        end,
        record.maximum_span_tt_days,
      )
    )
  return tuple(decoded)


def decode_model_identities(records, kind: ModelKind | None) -> tuple[ModelIdentity, ...]:
  decoded = []
  for record in records:
    if not isinstance(record, _abi.CelestialModelIdentityRecord):
      raise TypeError("unexpected identity record")
    actual_kind = _member(ModelKind, "MODEL_KIND", record.kind_code)
    if kind is not None and actual_kind is not kind:
      raise ValueError("unexpected model kind")
    start, end = _domain(record.domain_start_jde_tt, record.domain_end_jde_tt)
    decoded.append(
      ModelIdentity(
        ModelKey(actual_kind, record.identity_code),
        _member(ModelStatus, "MODEL_STATUS", record.status_code),
        start,
        end,
      )
    )
  expected = {ModelKey(k, identity) for k in ModelKind if kind is None or k is kind for identity in identity_values(k)}
  if len(decoded) != len(expected) or {record.key for record in decoded} != expected:
    raise ValueError("missing or duplicate registry identity")
  return tuple(decoded)


def decode_house_capability(result: _abi.CelestialCivilHouseCapability) -> CivilHouseCapability:
  if not isinstance(result, _abi.CelestialCivilHouseCapability):
    raise TypeError("unexpected civil-house capability record")
  status = _member(CapabilityStatus, "STATUS", result.status_code)
  if status is not CapabilityStatus.IMPLEMENTATION_WINDOW:
    raise ValueError("unexpected civil-house policy status")
  if result.civil_start_year >= result.civil_end_year_exclusive:
    raise ValueError("invalid civil-house year window")
  mask = sum(1 << enum_code("CIVIL_SCALE", scale) for scale in CivilScale)
  if not result.civil_scale_mask or result.civil_scale_mask & ~mask:
    raise ValueError("unknown civil scale bits")
  scales = tuple(scale for scale in CivilScale if result.civil_scale_mask & (1 << enum_code("CIVIL_SCALE", scale)))
  utc_start = (result.utc_start_year, result.utc_start_month, result.utc_start_day)
  _date(*utc_start)
  cutoff = finite(result.derived_tt_end_jde_tt, "derived_tt_end_jde_tt") if result.has_derived_tt_cutoff else None
  return CivilHouseCapability(
    status,
    result.civil_start_year,
    result.civil_end_year_exclusive,
    scales,
    utc_start,
    ModelKey(ModelKind.DELTA_T, result.delta_t_identity_code),
    cutoff,
  )
