# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Installed independent-query consumer: positions and houses match the snapshot where they overlap."""

from __future__ import annotations

import math

import celestial_calendar as celestial
from smoke import raises


def main() -> None:
  """Exercise the public position and house query surface against the installed wheel."""
  location = celestial.GeoLocation(latitude_deg=51.5, longitude_deg=0.0)
  civil = celestial.CivilDateTime(year=2024, month=6, day=1, fraction=0.5)
  snapshot = celestial.chart_snapshot(civil, celestial.CivilScale.UTC, location, celestial.HouseSystem.PLACIDUS)

  # A selected-target position query reproduces the snapshot body fields exactly, in caller order.
  selection = [
    celestial.ChartTarget.PLUTO,
    celestial.ChartTarget.SUN,
    celestial.ChartTarget.TRUE_ASCENDING,
    celestial.ChartTarget.MOON,
  ]
  positions = celestial.positions(selection, snapshot.times.jde_tt)
  assert [position.target for position in positions] == selection
  bodies = {body.target: body for body in snapshot.bodies}
  for position in positions:
    body = bodies[position.target]
    assert position.longitude_deg == body.longitude_deg
    assert position.latitude_deg == body.latitude_deg
    assert position.distance_au == body.distance_au
  assert positions[2].distance_au is None  # nodes carry no distance
  assert positions[0].source is celestial.PositionSource.PLUTO_PLUTO95_APPARENT
  assert positions[1].source is celestial.PositionSource.SUN_VSOP87D
  assert positions[2].source is celestial.PositionSource.NODE_TRUE_MEEUS_FIVE_TERM

  # No rate stencil and no snapshot joint window: a Pluto-only query succeeds at the provider
  # window start (1885-01-02 0h, JDE(TT) 2409543.5), where a complete snapshot at the same
  # instant rejects the query through the joint window.
  boundary = celestial.positions([celestial.ChartTarget.PLUTO], 2409543.5)
  assert len(boundary) == 1 and 0.0 <= boundary[0].longitude_deg < 360.0
  raises(
    celestial.CelestialError,
    lambda: celestial.chart_snapshot(
      celestial.CivilDateTime(year=1885, month=1, day=2, fraction=0.0),
      celestial.CivilScale.UT1,
      location,
      celestial.HouseSystem.EQUAL,
    ),
  )

  # Selection rules: duplicates and an empty selection are rejected; one failed target fails all.
  raises(ValueError, lambda: celestial.positions([celestial.ChartTarget.SUN, celestial.ChartTarget.SUN], 2451545.0))
  raises(ValueError, lambda: celestial.positions([], 2451545.0))
  raises(
    celestial.CelestialError,
    lambda: celestial.positions([celestial.ChartTarget.PLUTO, celestial.ChartTarget.SUN], 2409543.4),
  )

  # The house query matches the snapshot houses and records the resolved identities.
  houses = celestial.house_query(civil, celestial.CivilScale.UTC, location, celestial.HouseSystem.PLACIDUS)
  assert houses.houses == snapshot.houses
  assert houses.times == snapshot.times
  assert houses.house_system is celestial.HouseSystem.PLACIDUS
  assert houses.delta_t_identity == "algo5"
  assert houses.nutation_identity is celestial.NutationModel.IAU_1980

  # The snapshot joint window does not apply: 1885 resolves, and the nutation selector is
  # honoured. Algo3's resolved identity lands on the result, not a hardcoded default.
  early = celestial.house_query(
    celestial.CivilDateTime(year=1885, month=1, day=1, fraction=0.0),
    celestial.CivilScale.UT1,
    location,
    celestial.HouseSystem.EQUAL,
  )
  assert early.times.jde_tt < 2409543.875
  assert early.delta_t_identity == "algo5"
  algo3 = celestial.house_query(
    civil,
    celestial.CivilScale.UTC,
    location,
    celestial.HouseSystem.EQUAL,
    delta_t_model=celestial.DeltaTModel.ALGO3,
  )
  assert algo3.delta_t_identity == "algo3"
  assert early.delta_t_identity == "algo5"
  meeus = celestial.house_query(
    civil,
    celestial.CivilScale.UTC,
    location,
    celestial.HouseSystem.PLACIDUS,
    nutation_model=celestial.NutationModel.MEEUS,
  )
  assert meeus.nutation_identity is celestial.NutationModel.MEEUS
  assert meeus.houses.ascendant_deg != houses.houses.ascendant_deg

  # A polar Placidus failure leaves positions untouched.
  raises(
    celestial.CelestialError,
    lambda: celestial.house_query(
      civil,
      celestial.CivilScale.UTC,
      celestial.GeoLocation(latitude_deg=89.9, longitude_deg=0.0),
      celestial.HouseSystem.PLACIDUS,
    ),
  )
  after = celestial.positions([celestial.ChartTarget.MARS], 2451545.0)
  assert len(after) == 1 and math.isfinite(after[0].longitude_deg)

  print("PASS independent query consumer")


if __name__ == "__main__":
  main()
