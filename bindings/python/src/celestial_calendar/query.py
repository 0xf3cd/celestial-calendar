# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Independent position and house queries (#355).

`positions()` evaluates only the caller's selected targets at one JDE(TT) instant — no rate
stencil, no snapshot joint window — so a subset succeeds where a complete snapshot rejects
an excluded target's window. `house_query()` runs the snapshot's civil conversion without
the joint window and records the resolved model identities (#357). Both are thin decoders
over the native V1 records; every domain bound stays owned by the native layer.
"""

from __future__ import annotations

import math as _math
from dataclasses import dataclass as _dataclass
from enum import StrEnum as _StrEnum
from typing import Tuple as _Tuple

from . import _abi
from . import capability as _capability
from . import chart as _chart
from .capability import PositionSource as _PositionSource
from .chart import ChartHouses as _ChartHouses
from .chart import ChartTarget as _ChartTarget
from .chart import ChartTimes as _ChartTimes
from .chart import HouseSystem as _HouseSystem


class NutationModel(_StrEnum):
  """Nutation model selector for the independent house query.

  DEFAULT follows the C++ house operation's default (IAU 1980). The resolved identity is
  never the unresolved DEFAULT code: it is one of the registry's MODEL_IDENTITY_NUTATION_*
  ordinals.
  """

  DEFAULT = "default"
  MEEUS = "meeus"
  IAU_1980 = "iau_1980"


_NUTATION_MODEL_CODE = {NutationModel.DEFAULT: 0, NutationModel.MEEUS: 1, NutationModel.IAU_1980: 2}
_NUTATION_MODEL_BY_CODE = {0: NutationModel.IAU_1980, 1: NutationModel.MEEUS, 2: NutationModel.IAU_1980}

_DELTA_T_IDENTITY_BY_CODE = {
  0: "algo1",
  1: "algo2",
  2: "algo3",
  3: "algo4",
  4: "algo5",
}
_NUTATION_IDENTITY_BY_CODE = {0: NutationModel.MEEUS, 1: NutationModel.IAU_1980}


@_dataclass(frozen=True)
class Position:
  """Apparent geocentric ecliptic position of one selected target at one JDE(TT).

  Distances are AU for Sun, Moon, planets and Pluto; nodes carry no distance. `source` is
  the resolved position-source identity, stable across releases (#357).
  """

  target: _ChartTarget
  longitude_deg: float
  latitude_deg: float
  distance_au: float | None
  source: _PositionSource


@_dataclass(frozen=True)
class HouseQueryResult:
  """House geometry at one civil instant, with the resolved model identities.

  `times` carries both resolved scales; `delta_t_identity` and `nutation_identity` record
  which models produced this result, so a later DEFAULT change cannot rewrite it (#357).
  """

  times: _ChartTimes
  houses: _ChartHouses
  house_system: _HouseSystem
  delta_t_identity: str
  nutation_identity: NutationModel


def nutation_model_code(model: NutationModel) -> int:
  return _NUTATION_MODEL_CODE[model]


def _finite(value: float, name: str) -> float:
  if not _math.isfinite(value):
    raise ValueError(f"{name} is not finite")
  return value


def _longitude(value: float, name: str) -> float:
  result = _finite(value, name)
  if not 0.0 <= result < 360.0:
    raise ValueError(f"{name} is outside [0, 360)")
  return result


def decode_positions(records, expected_targets: _Tuple[_ChartTarget, ...]) -> _Tuple[Position, ...]:
  """Copy successful V1 position records; malformed protocol fields raise decoding errors."""
  if len(records) != len(expected_targets):
    raise ValueError("native returned an unexpected position count")

  positions = []
  for record, expected in zip(records, expected_targets, strict=True):
    if not isinstance(record, _abi.PositionRecordV1):
      raise TypeError("unexpected V1 position record")
    if _chart._CHART_TARGET_CODE[expected] != record.target_code:
      raise ValueError(f"unexpected target code for {expected.name}")

    is_node = expected.name.startswith(("MEAN_", "TRUE_"))
    has_distance = (record.present_fields & 0x2) != 0  # POSITION_PRESENT_DISTANCE
    if has_distance == is_node:
      raise ValueError(f"unexpected presence bits for {expected.name}")

    latitude = _finite(record.latitude_deg, f"{expected.name}.latitude_deg")
    if not -90.0 <= latitude <= 90.0 or (is_node and latitude != 0.0):
      raise ValueError(f"unexpected latitude for {expected.name}")
    distance = None if is_node else _finite(record.distance_au, f"{expected.name}.distance_au")
    if distance is not None and distance <= 0.0:
      raise ValueError(f"nonpositive distance for {expected.name}")
    source = _capability.position_source(record.source_code)
    if source is None:
      raise ValueError(f"unexpected position source code {record.source_code}")

    positions.append(
      Position(expected, _longitude(record.longitude_deg, f"{expected.name}.longitude_deg"), latitude, distance, source)
    )
  return tuple(positions)


def decode_house_query(result, house_system: _HouseSystem) -> HouseQueryResult:
  """Copy a successful V1 house query; malformed protocol fields raise decoding errors."""
  if not isinstance(result, _abi.HouseQueryResultV1):
    raise TypeError("unexpected V1 house query record")
  if not result.valid:
    raise ValueError("invalid V1 house query payload")

  delta_t_identity = _DELTA_T_IDENTITY_BY_CODE.get(result.delta_t_identity_code)
  if delta_t_identity is None:
    raise ValueError(f"unexpected Delta T identity code {result.delta_t_identity_code}")
  nutation_identity = _NUTATION_IDENTITY_BY_CODE.get(result.nutation_identity_code)
  if nutation_identity is None:
    raise ValueError(f"unexpected nutation identity code {result.nutation_identity_code}")

  native = result.houses
  houses = _ChartHouses(
    _longitude(native.ascendant_deg, "ascendant_deg"),
    _longitude(native.midheaven_deg, "midheaven_deg"),
    _longitude(native.descendant_deg, "descendant_deg"),
    _longitude(native.imum_coeli_deg, "imum_coeli_deg"),
    tuple(_longitude(cusp, "cusps_deg") for cusp in native.cusps_deg),
  )
  times = _ChartTimes(_finite(result.jd_ut1, "jd_ut1"), _finite(result.jde_tt, "jde_tt"))
  return HouseQueryResult(times, houses, house_system, delta_t_identity, nutation_identity)
