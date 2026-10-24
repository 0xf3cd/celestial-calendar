# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Installed TT capabilities, civil-house policy and kind-qualified model registry."""

from __future__ import annotations

import math
from dataclasses import FrozenInstanceError

import celestial_calendar as celestial
from smoke import raises


def main() -> None:
  events = celestial.capabilities(celestial.CapabilityOperation.EVENTS)
  assert len(events) == 14
  assert [record.subject for record in events] == list(celestial.Target)
  assert all(record.maximum_span_tt_days == 3660 for record in events)
  assert all(record.operation is celestial.CapabilityOperation.EVENTS for record in events)
  pluto = celestial.capabilities(celestial.CapabilityOperation.POSITION, celestial.Target.PLUTO)
  assert len(pluto) == 1 and pluto[0].status is celestial.CapabilityStatus.SOURCE_WINDOW
  assert pluto[0].source == celestial.ModelKey(celestial.ModelKind.POSITION_SOURCE, 4)
  assert pluto[0].domain_start_jde_tt == 2409543.5 and pluto[0].domain_end_jde_tt == 2488069.5
  sun = celestial.capabilities(celestial.CapabilityOperation.POSITION, celestial.Target.SUN)[0]
  assert sun.status is celestial.CapabilityStatus.FINITE_INPUT_EVALUATION
  assert math.isnan(sun.domain_start_jde_tt) and math.isnan(sun.domain_end_jde_tt)
  equatorial = celestial.capabilities(celestial.CapabilityOperation.EQUATORIAL)
  assert len(equatorial) == 14 and [record.subject for record in equatorial] == list(celestial.Target)

  houses = celestial.house_capability()
  assert type(houses) is celestial.CivilHouseCapability
  assert not hasattr(houses, "domain_start_jde_tt")
  for model, identity in zip(celestial.DeltaTModel, (5, 1, 2, 3, 4, 5), strict=True):
    policy = celestial.house_capability(delta_t_model=model)
    assert policy.status is celestial.CapabilityStatus.IMPLEMENTATION_WINDOW
    assert (policy.civil_start_year, policy.civil_end_year_exclusive) == (1885, 2100)
    assert policy.civil_scales == tuple(celestial.CivilScale) and policy.utc_start_date == (1972, 1, 1)
    assert policy.delta_t_model == celestial.ModelKey(celestial.ModelKind.DELTA_T, identity)
    assert policy.derived_tt_end_jde_tt == (2464328.5 if identity == 4 else None)
    assert celestial.house_capability(delta_t_model=policy.delta_t_model) == policy

  identities = celestial.model_identities()
  assert len(identities) == 13 and len({record.key for record in identities}) == 13
  for kind in celestial.ModelKind:
    expected = tuple(identity for identity in identities if identity.key.kind is kind)
    assert expected
    for _ in range(2):
      actual = celestial.model_identities(kind)
      assert len(actual) == len(expected)
      for record, reference in zip(actual, expected, strict=True):
        assert (record.key, record.status) == (reference.key, reference.status)
        for value, target in (
          (record.domain_start_jde_tt, reference.domain_start_jde_tt),
          (record.domain_end_jde_tt, reference.domain_end_jde_tt),
        ):
          assert value == target or (math.isnan(value) and math.isnan(target))
        text = celestial.model_identity_text(record.key)
        assert text
        if kind is celestial.ModelKind.DELTA_T:
          assert f"algo{record.key.identity}" in text.lower()
  delta_t = [identity for identity in identities if identity.key.kind is celestial.ModelKind.DELTA_T]
  assert len(delta_t) == 5 and delta_t[4].status is celestial.ModelStatus.CURRENT
  assert delta_t[4].key == celestial.ModelKey(celestial.ModelKind.DELTA_T, 5)
  assert not hasattr(delta_t[4], "default_alias_code")
  assert delta_t[3].status is celestial.ModelStatus.FROZEN and delta_t[3].domain_end_jde_tt == 2464328.5
  text = celestial.capability_text(celestial.CapabilityOperation.POSITION, celestial.Target.PLUTO)
  assert "Meeus Chapter 37" in text and "PLUTO95 supplies independent references, not the runtime evaluator" in text
  assert "Direct selected-target position" in text and "JDE(TT) in [2409543.5, 2488069.5)" in text
  assert "neither accuracy nor physical plausibility" in text and "planet_test.cpp" in text
  assert "algo5" in celestial.model_identity_text(delta_t[4].key).lower()
  for operation in (
    celestial.CapabilityOperation.HOUSES,
    celestial.CapabilityOperation.CHART,
    celestial.CapabilityOperation.DELTA_T,
  ):
    assert "default" in celestial.capability_text(operation).lower()
    raises(ValueError, lambda operation=operation: celestial.capabilities(operation))
    raises(ValueError, lambda operation=operation: celestial.capabilities(operation, celestial.Target.SUN))
    raises(ValueError, lambda operation=operation: celestial.capability_text(operation, celestial.Target.SUN))
  for operation in (
    celestial.CapabilityOperation.POSITION,
    celestial.CapabilityOperation.LONGITUDE_RATE,
    celestial.CapabilityOperation.EVENTS,
    celestial.CapabilityOperation.EQUATORIAL,
  ):
    raises(TypeError, lambda operation=operation: celestial.capability_text(operation))
  raises(TypeError, lambda: celestial.capabilities("position"))
  raises(TypeError, lambda: celestial.capability_text(celestial.CapabilityOperation.POSITION, -1))
  raises(FrozenInstanceError, lambda: setattr(events[0], "maximum_span_tt_days", 1))
  print("PASS TT/civil capabilities and registry-key text lookup")


if __name__ == "__main__":
  main()
