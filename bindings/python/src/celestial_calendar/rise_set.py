# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Owned solar transit-centred and lunar/planetary UT1-day results."""

from dataclasses import dataclass
from enum import StrEnum

from . import _abi
from ._codes import constant, enum_code, enum_member
from ._models import ModelKey, ModelKind, Target, position_source_code
from ._protocol import finite
from .observer import RefractionParams


class PolarTopology(StrEnum):
  """No polar verdict, or altitude entirely above/below the event threshold."""

  NONE = "none"
  DAY = "day"
  NIGHT = "night"


@dataclass(frozen=True)
class SolarRiseSet:
  """TT events around the date's local solar transit; rise/set can lie outside that UT1 date."""

  source: ModelKey
  delta_t_model: ModelKey
  nutation_model: ModelKey
  polar: PolarTopology
  rise_jde_tt: float | None
  transit_jde_tt: float
  set_jde_tt: float | None
  h0_deg: float


@dataclass(frozen=True)
class LunarRiseSet:
  """Representative TT events in a UT1 day; a missing event is not a polar verdict.

  Of two rises or sets, the later is returned; of two transits, the earlier.
  A rise or set exactly at 0h UT1 belongs to neither neighbouring day.
  The geocentric threshold uses midday parallax and the supplied horizon atmosphere.
  """

  source: ModelKey
  delta_t_model: ModelKey
  nutation_model: ModelKey
  polar: PolarTopology
  rise_jde_tt: float | None
  transit_jde_tt: float | None
  set_jde_tt: float | None
  refraction: RefractionParams


@dataclass(frozen=True)
class PlanetRiseSet:
  """Representative TT events in a UT1 day, using an airless topocentric centre altitude."""

  target: Target
  source: ModelKey
  delta_t_model: ModelKey
  nutation_model: ModelKey
  polar: PolarTopology
  rise_jde_tt: float | None
  transit_jde_tt: float | None
  set_jde_tt: float | None
  h0_deg: float


def _events(native, target: Target) -> dict:
  """Validate identities, topology and every absent slot before copying the scalar values."""
  if native.source_code != position_source_code(target):
    raise ValueError("unexpected rise/set position source")
  if native.delta_t_identity_code != constant("DELTA_T_MODEL_ALGO5"):
    raise ValueError("unexpected rise/set Delta T model")
  if native.nutation_identity_code != constant("NUTATION_MODEL_IAU_1980"):
    raise ValueError("unexpected rise/set nutation model")
  rise, transit, setting = (constant("PRESENT_" + name) for name in ("RISE", "TRANSIT", "SET"))
  mask = native.present_fields
  if mask & ~(rise | transit | setting):
    raise ValueError("unknown rise/set presence bits")
  polar = enum_member(PolarTopology, "RISE_SET_POLAR", native.polar_code)
  if polar is not PolarTopology.NONE and mask & (rise | setting):
    raise ValueError("polar result contains a crossing")
  if target is Target.SUN and not mask & transit:
    raise ValueError("solar transit is absent")

  result = {
    "source": ModelKey(ModelKind.POSITION_SOURCE, native.source_code),
    "delta_t_model": ModelKey(ModelKind.DELTA_T, native.delta_t_identity_code),
    "nutation_model": ModelKey(ModelKind.NUTATION, native.nutation_identity_code),
    "polar": polar,
  }
  for field, bit in (("rise_jde_tt", rise), ("transit_jde_tt", transit), ("set_jde_tt", setting)):
    value = finite(getattr(native, field), field)
    if not mask & bit and value != 0.0:
      raise ValueError("nonzero absent event slot")
    result[field] = value if mask & bit else None
  return result


def decode_solar(native: _abi.CelestialSolarRiseSet, h0: float) -> SolarRiseSet:
  if not isinstance(native, _abi.CelestialSolarRiseSet):
    raise TypeError("unexpected solar rise/set record")
  if finite(native.h0_deg, "h0_deg") != h0:
    raise ValueError("unexpected solar altitude")
  return SolarRiseSet(**_events(native, Target.SUN), h0_deg=h0)


def decode_lunar(native: _abi.CelestialLunarRiseSet, refraction: RefractionParams) -> LunarRiseSet:
  if not isinstance(native, _abi.CelestialLunarRiseSet):
    raise TypeError("unexpected lunar rise/set record")
  if (
    native.refraction_code != enum_code("REFRACTION", refraction.model)
    or finite(native.temperature_c, "temperature_c") != refraction.temperature_c
    or finite(native.pressure_hpa, "pressure_hpa") != refraction.pressure_hpa
  ):
    raise ValueError("unexpected lunar atmosphere")
  atmosphere = RefractionParams(refraction.model, native.temperature_c, native.pressure_hpa)
  return LunarRiseSet(**_events(native, Target.MOON), refraction=atmosphere)


def decode_planet(native: _abi.CelestialPlanetRiseSet, target: Target, h0: float) -> PlanetRiseSet:
  if not isinstance(native, _abi.CelestialPlanetRiseSet):
    raise TypeError("unexpected planetary rise/set record")
  if native.target_code != enum_code("TARGET", target) or finite(native.h0_deg, "h0_deg") != h0:
    raise ValueError("unexpected planetary target/altitude")
  return PlanetRiseSet(target=target, **_events(native, target), h0_deg=h0)
