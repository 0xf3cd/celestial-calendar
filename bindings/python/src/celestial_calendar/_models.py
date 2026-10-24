# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Shared target vocabulary and kind-qualified resolved model identities."""

from dataclasses import dataclass
from enum import StrEnum

from ._codes import enum_code, family_values


class Target(StrEnum):
  """A body or lunar node used by positions, charts, events and capabilities."""

  SUN = "sun"
  MOON = "moon"
  MERCURY = "mercury"
  VENUS = "venus"
  MARS = "mars"
  JUPITER = "jupiter"
  SATURN = "saturn"
  URANUS = "uranus"
  NEPTUNE = "neptune"
  PLUTO = "pluto"
  MEAN_ASCENDING = "mean_ascending"
  MEAN_DESCENDING = "mean_descending"
  TRUE_ASCENDING = "true_ascending"
  TRUE_DESCENDING = "true_descending"


class DeltaTModel(StrEnum):
  """Delta T selector; DEFAULT is resolved by the native operation."""

  DEFAULT = "default"
  ALGO1 = "algo1"
  ALGO2 = "algo2"
  ALGO3 = "algo3"
  ALGO4 = "algo4"
  ALGO5 = "algo5"


class NutationModel(StrEnum):
  """House nutation selector; DEFAULT follows the native house default."""

  DEFAULT = "default"
  MEEUS = "meeus"
  IAU_1980 = "iau_1980"


class ModelKind(StrEnum):
  """Family qualifying an identity in the model registry."""

  DELTA_T = "delta_t"
  NUTATION = "nutation"
  POSITION_SOURCE = "position_source"


def identity_values(kind: ModelKind) -> frozenset[int]:
  family = kind.name if kind is ModelKind.POSITION_SOURCE else f"{kind.name}_MODEL"
  return family_values(family) - {0}


@dataclass(frozen=True)
class ModelKey:
  """A registered (kind, identity) pair; identity is a nonzero native integer code.

  Keys from results or the registry can be passed to matching-kind selectors and
  model_identity_text(). DEFAULT is input-only and cannot form a key.
  """

  kind: ModelKind
  identity: int

  def __post_init__(self) -> None:
    if not isinstance(self.kind, ModelKind):
      raise TypeError("kind must be a ModelKind member")
    if not any(self.kind is member for member in ModelKind):
      raise ValueError("kind must be a declared ModelKind member")
    if type(self.identity) is not int:
      raise TypeError("identity must be an integer, not a bool or enum")
    if self.identity not in identity_values(self.kind):
      raise ValueError(f"unknown {self.kind.name} identity {self.identity}")


def selector_code(model: DeltaTModel | NutationModel | ModelKey, kind: ModelKind, name: str) -> int:
  if isinstance(model, ModelKey):
    # Revalidate at the input boundary, including objects forged without __init__.
    ModelKey(model.kind, model.identity)
    if model.kind is not kind:
      raise TypeError(f"{name} requires a {kind.name} ModelKey")
    return model.identity
  enum_type = DeltaTModel if kind is ModelKind.DELTA_T else NutationModel
  if not isinstance(model, enum_type):
    raise TypeError(f"{name} must be a {enum_type.__name__} or matching ModelKey")
  if not any(model is member for member in enum_type):
    raise ValueError(f"{name} must be a declared {enum_type.__name__} member")
  return enum_code(f"{kind.name}_MODEL", model)


def is_node(target: Target) -> bool:
  return target in (Target.MEAN_ASCENDING, Target.MEAN_DESCENDING, Target.TRUE_ASCENDING, Target.TRUE_DESCENDING)
