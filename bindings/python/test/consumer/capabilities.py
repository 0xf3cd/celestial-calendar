# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Installed capability consumer: records mirror the native layer and stay frozen."""

from __future__ import annotations

import math
from dataclasses import FrozenInstanceError

import celestial_calendar as celestial
from smoke import raises


def main() -> None:
  """Exercise the public capability surface against the installed wheel."""
  events = celestial.capabilities(celestial.CapabilityOperation.EVENTS)
  assert len(events) == 14
  assert [record.subject for record in events] == list(celestial.CapabilitySubject)
  assert all(record.maximum_span_tt_days == 3660 for record in events)
  assert all(record.operation is celestial.CapabilityOperation.EVENTS for record in events)

  pluto = celestial.capabilities(celestial.CapabilityOperation.POSITION, celestial.CapabilitySubject.PLUTO)
  assert len(pluto) == 1
  assert pluto[0].status is celestial.CapabilityStatus.SOURCE_WINDOW
  assert pluto[0].source is celestial.PositionSource.PLUTO_PLUTO95_APPARENT
  assert pluto[0].domain_start_jde_tt == 2409543.5 and pluto[0].domain_end_jde_tt == 2488069.5

  sun = celestial.capabilities(celestial.CapabilityOperation.POSITION, celestial.CapabilitySubject.SUN)[0]
  assert sun.status is celestial.CapabilityStatus.UNBOUNDED_FINITE
  assert math.isnan(sun.domain_start_jde_tt) and math.isnan(sun.domain_end_jde_tt)

  houses = celestial.capabilities(celestial.CapabilityOperation.HOUSES)
  assert len(houses) == 1 and houses[0].subject is None
  assert math.isnan(houses[0].domain_start_jde_tt)

  identities = celestial.model_identities()
  assert len(identities) == 13
  for kind in celestial.ModelKind:
    expected = tuple(identity for identity in identities if identity.kind is kind)
    assert expected
    for _ in range(2):
      actual = celestial.model_identities(kind)
      assert len(actual) == len(expected)
      for record, reference in zip(actual, expected, strict=True):
        assert (record.kind, record.identity_code, record.status, record.default_alias_code) == (
          reference.kind,
          reference.identity_code,
          reference.status,
          reference.default_alias_code,
        )
        for value, target in (
          (record.domain_start_jde_tt, reference.domain_start_jde_tt),
          (record.domain_end_jde_tt, reference.domain_end_jde_tt),
        ):
          assert value == target or (math.isnan(value) and math.isnan(target))
  delta_t = [identity for identity in identities if identity.kind is celestial.ModelKind.DELTA_T]
  assert len(delta_t) == 5
  assert delta_t[4].status is celestial.ModelStatus.CURRENT
  # Kind-internal ordinal, not the CHART_DELTA_T_MODEL_* boundary code (where ALGO5 is 5).
  assert (delta_t[4].kind, delta_t[4].identity_code) == (celestial.ModelKind.DELTA_T, 4)
  assert delta_t[4].default_alias_code == 0
  assert delta_t[3].status is celestial.ModelStatus.FROZEN
  assert delta_t[3].default_alias_code is None
  assert delta_t[3].domain_end_jde_tt == 2464328.5

  text = celestial.capability_text(celestial.CapabilityOperation.POSITION, celestial.CapabilitySubject.PLUTO)
  assert "PLUTO95" in text and "Role:" in text and "Position coverage:" in text
  identity_text = celestial.capability_text(celestial.CapabilityOperation.MODEL_IDENTITY, 4)
  assert "algo5" in identity_text and "DEFAULT" in identity_text

  raises(TypeError, lambda: celestial.capabilities("position"))  # type: ignore[arg-type]
  raises(ValueError, lambda: celestial.capability_text(celestial.CapabilityOperation.POSITION, -1))
  raises(celestial.CelestialError, lambda: celestial.capabilities(celestial.CapabilityOperation.MODEL_IDENTITY))
  raises(FrozenInstanceError, lambda: setattr(events[0], "maximum_span_tt_days", 1))

  print("PASS capabilities consumer")


if __name__ == "__main__":
  main()
