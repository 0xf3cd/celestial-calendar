# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Installed position/house consumers, resolved-key reuse and independent equatorial calls."""

from __future__ import annotations

import math

import celestial_calendar as celestial
from smoke import raises


def key(kind, identity):
  return celestial.ModelKey(kind, identity)


def main() -> None:
  location = celestial.GeoLocation(latitude_deg=51.5, longitude_deg=0.0)
  civil = celestial.CivilDateTime(year=2024, month=6, day=1, fraction=0.5)
  snapshot = celestial.chart_snapshot(civil, celestial.CivilScale.UTC, location, celestial.HouseSystem.PLACIDUS)
  selection = [celestial.Target.PLUTO, celestial.Target.SUN, celestial.Target.TRUE_ASCENDING, celestial.Target.MOON]
  positions = celestial.positions(selection, snapshot.times.jde_tt)
  assert [position.target for position in positions] == selection
  bodies = {body.target: body for body in snapshot.bodies}
  for position in positions:
    body = bodies[position.target]
    assert position.longitude_deg == body.longitude_deg
    assert position.latitude_deg == body.latitude_deg
    assert position.distance_au == body.distance_au
    assert position.source == body.source
  assert positions[2].distance_au is None
  assert [position.source for position in positions[:3]] == [
    key(celestial.ModelKind.POSITION_SOURCE, code) for code in (4, 1, 6)
  ]

  boundary = celestial.positions([celestial.Target.PLUTO], 2409543.5)
  assert len(boundary) == 1 and 0.0 <= boundary[0].longitude_deg < 360.0
  early_civil = celestial.CivilDateTime(year=1885, month=1, day=2, fraction=0.0)
  early = celestial.house_query(early_civil, celestial.CivilScale.UT1, location, celestial.HouseSystem.EQUAL)
  assert early.times.jde_tt < 2409543.875
  subset = celestial.positions([celestial.Target.SUN, celestial.Target.MOON], early.times.jde_tt)
  assert len(subset) == 2
  raises(
    ValueError,
    lambda: celestial.chart_snapshot(early_civil, celestial.CivilScale.UT1, location, celestial.HouseSystem.EQUAL),
  )

  for operation in (celestial.positions, celestial.equatorial_positions):
    raises(ValueError, lambda operation=operation: operation([celestial.Target.SUN] * 2, 2451545.0))
    raises(ValueError, lambda operation=operation: operation([], 2451545.0))
    raises(ValueError, lambda operation=operation: operation([celestial.Target.PLUTO, celestial.Target.SUN], 2409543.4))
    full = operation(tuple(reversed(tuple(celestial.Target))), 2451545.0)
    assert tuple(record.target for record in full) == tuple(reversed(tuple(celestial.Target)))
  equatorial = celestial.equatorial_positions(selection, snapshot.times.jde_tt)
  assert [position.target for position in equatorial] == selection
  for record, ecliptic in zip(equatorial, positions, strict=True):
    assert type(record) is celestial.EquatorialPosition
    assert 0.0 <= record.right_ascension_deg < 360.0 and -90.0 <= record.declination_deg <= 90.0
    assert record.distance_au == ecliptic.distance_au and record.source == ecliptic.source
    assert celestial.model_identity_text(record.source)

  houses = celestial.house_query(civil, celestial.CivilScale.UTC, location, celestial.HouseSystem.PLACIDUS)
  assert houses.houses == snapshot.houses and houses.times == snapshot.times
  assert houses.house_system is celestial.HouseSystem.PLACIDUS
  assert houses.delta_t_model == snapshot.delta_t_model == key(celestial.ModelKind.DELTA_T, 5)
  assert houses.nutation_model == snapshot.nutation_model == key(celestial.ModelKind.NUTATION, 2)
  repeated = celestial.house_query(
    civil,
    celestial.CivilScale.UTC,
    location,
    celestial.HouseSystem.PLACIDUS,
    delta_t_model=houses.delta_t_model,
    nutation_model=houses.nutation_model,
  )
  assert repeated == houses

  old_houses = celestial.house_query(
    celestial.CivilDateTime(1885, 1, 1, 0.0), celestial.CivilScale.UT1, location, celestial.HouseSystem.EQUAL
  )
  assert old_houses.times.jde_tt < 2409543.875
  assert old_houses.delta_t_model == key(celestial.ModelKind.DELTA_T, 5)
  algo3 = celestial.house_query(
    civil, celestial.CivilScale.UTC, location, celestial.HouseSystem.EQUAL, delta_t_model=celestial.DeltaTModel.ALGO3
  )
  assert algo3.delta_t_model == key(celestial.ModelKind.DELTA_T, 3)
  assert old_houses.delta_t_model == key(celestial.ModelKind.DELTA_T, 5)
  meeus = celestial.house_query(
    civil,
    celestial.CivilScale.UTC,
    location,
    celestial.HouseSystem.PLACIDUS,
    nutation_model=celestial.NutationModel.MEEUS,
  )
  assert meeus.nutation_model == key(celestial.ModelKind.NUTATION, 1)
  assert meeus.houses.ascendant_deg != houses.houses.ascendant_deg

  for delta_model, delta_identity in zip(celestial.DeltaTModel, (5, 1, 2, 3, 4, 5), strict=True):
    for nutation_model, nutation_identity in zip(celestial.NutationModel, (2, 1, 2), strict=True):
      result = celestial.house_query(
        civil,
        celestial.CivilScale.UT1,
        location,
        celestial.HouseSystem.EQUAL,
        delta_t_model=delta_model,
        nutation_model=nutation_model,
      )
      assert result.delta_t_model == key(celestial.ModelKind.DELTA_T, delta_identity)
      assert result.nutation_model == key(celestial.ModelKind.NUTATION, nutation_identity)
      replay = celestial.house_query(
        civil,
        celestial.CivilScale.UT1,
        location,
        celestial.HouseSystem.EQUAL,
        delta_t_model=result.delta_t_model,
        nutation_model=result.nutation_model,
      )
      assert result == replay

  raises(
    ValueError,
    lambda: celestial.house_query(
      civil, celestial.CivilScale.UTC, celestial.GeoLocation(89.9, 0.0), celestial.HouseSystem.PLACIDUS
    ),
  )
  after = celestial.positions([celestial.Target.MARS], 2451545.0)
  assert len(after) == 1 and math.isfinite(after[0].longitude_deg)

  # Both civil scales enforce the same exclusive derived-TT Algo4 cutoff.
  for scale in celestial.CivilScale:
    admitted = celestial.house_query(
      celestial.CivilDateTime(2034, 12, 31, 0.998),
      scale,
      location,
      celestial.HouseSystem.EQUAL,
      delta_t_model=celestial.DeltaTModel.ALGO4,
    )
    assert admitted.times.jde_tt < 2464328.5
    raises(
      ValueError,
      lambda scale=scale: celestial.house_query(
        celestial.CivilDateTime(2034, 12, 31, 0.999999),
        scale,
        location,
        celestial.HouseSystem.EQUAL,
        delta_t_model=celestial.DeltaTModel.ALGO4,
      ),
    )
  print("PASS independent queries, all-target equatorial shape and resolved model-key replay")


if __name__ == "__main__":
  main()
