# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Installed observer queries, immutable values and malformed-native presence/identity seams."""

from dataclasses import FrozenInstanceError, replace
import ctypes
import math

import celestial_calendar as c
from celestial_calendar import _abi
from protocol_seams import bindings, raises


def main() -> None:
  # Independent native64 pins, separate from generated declaration/layout manifests.
  assert ctypes.sizeof(_abi.CelestialTopocentricPosition) == 88
  assert ctypes.alignment(_abi.CelestialTopocentricPosition) == 8
  assert [
    getattr(_abi.CelestialTopocentricPosition, name).offset for name, _ in _abi.CelestialTopocentricPosition._fields_
  ] == [0, 4, 8, 12, 16, 24, 32, 40, 48, 56, 64, 72, 80]
  civil = c.CivilDateTime(2026, 11, 7, 0.0)
  site = c.Observer(-33.85, 151.2, 50.0)

  def query(target=c.Target.MOON, date=civil, observer=site, **options):
    return c.topocentric_position(target, date, c.CivilScale.UTC, observer, **options)

  for target in tuple(c.Target)[:10]:
    result = query(target)
    assert result.target is target and result.distance_au > 0.0
    assert result.right_ascension_deg is not None and result.azimuth_deg is not None
    assert result.apparent_altitude_deg is None
    assert result.times.jde_tt == c.utc_to_jde(civil)
    assert result.delta_t_model == c.ModelKey(c.ModelKind.DELTA_T, 5)
    raises(FrozenInstanceError, lambda result=result: setattr(result, "altitude_deg", 0.0))
  raises(FrozenInstanceError, lambda: setattr(site, "elevation_m", 0.0))
  for pole in (-90.0, 90.0):
    assert query(observer=replace(site, latitude_deg=pole)).azimuth_deg is None
    assert query(observer=replace(site, latitude_deg=math.nextafter(pole, 0.0))).azimuth_deg is not None
    raises(ValueError, lambda pole=pole: query(observer=replace(site, latitude_deg=math.nextafter(pole, 2 * pole))))
  for model in c.RefractionModel:
    seen = set()
    for hour in range(24):
      result = query(date=replace(civil, fraction=hour / 24), refraction=c.RefractionParams(model))
      seen.add(result.apparent_altitude_deg is None)
    assert seen == {False, True}
    raises(ValueError, lambda model=model: query(refraction=c.RefractionParams(model, -91.0)))
  assert query(c.Target.SUN, c.CivilDateTime(1972, 1, 1, 0.0)).distance_au > 0.0
  assert c.topocentric_position(c.Target.SUN, c.CivilDateTime(1000, 1, 1, 0.0), c.CivilScale.UT1, site).distance_au > 0
  raises(ValueError, lambda: query(date=c.CivilDateTime(1971, 12, 31, 0.0)))
  raises(ValueError, lambda: query(date=c.CivilDateTime(2036, 1, 1, 0.0), delta_t_model=c.DeltaTModel.ALGO4))
  raises(ValueError, lambda: query(c.Target.PLUTO, c.CivilDateTime(2100, 1, 1, 0.0)))
  assert query(c.Target.PLUTO, c.CivilDateTime(2099, 12, 31, 0.0)).distance_au > 0
  for target in tuple(c.Target)[10:]:
    raises(ValueError, lambda target=target: query(target))
  for observer in (
    replace(site, elevation_m=math.inf),
    replace(site, elevation_m=-6378137.0),
    replace(site, longitude_deg=181.0),
  ):
    raises(ValueError, lambda observer=observer: query(observer=observer))
  assert "WGS84" in c.capability_text(c.CapabilityOperation.TOPOCENTRIC)
  print("PASS observer physical targets, units, immutable values, poles, weather and civil/provider guards")

  def native(**changes):
    fields = dict(
      valid=True,
      target_code=1,
      source_code=2,
      delta_t_identity_code=5,
      present_fields=0,
      jd_ut1=2461351.5,
      jde_tt=2461351.501,
      right_ascension_deg=math.nan,
      declination_deg=-10.0,
      distance_au=0.0025,
      altitude_deg=10.0,
      azimuth_deg=math.nan,
      apparent_altitude_deg=math.nan,
    )
    return _abi.CelestialTopocentricPosition(**(fields | changes))

  with bindings(celestial_topocentric_position=lambda *args: native()):
    result = query()
    assert (result.right_ascension_deg, result.azimuth_deg, result.apparent_altitude_deg) == (None, None, None)
  for changes in (
    dict(present_fields=32),
    dict(present_fields=16, apparent_altitude_deg=10),
    dict(target_code=0),
    dict(source_code=99),
    dict(delta_t_identity_code=0),
    dict(delta_t_identity_code=99),
    dict(jd_ut1=math.nan),
    dict(jde_tt=math.inf),
    dict(declination_deg=91),
    dict(distance_au=0),
    dict(altitude_deg=-91),
    dict(present_fields=4, right_ascension_deg=math.nan),
    dict(present_fields=4, right_ascension_deg=360),
    dict(present_fields=8, azimuth_deg=-1),
    dict(present_fields=8, azimuth_deg=math.inf),
  ):
    with bindings(celestial_topocentric_position=lambda *args, changes=changes: native(**changes)):
      assert not raises(c.CelestialError, query).recorded
  with bindings(celestial_topocentric_position=lambda *args: native(delta_t_identity_code=1)):
    assert not raises(c.CelestialError, lambda: query(delta_t_model=c.DeltaTModel.ALGO5)).recorded
  with bindings(celestial_topocentric_position=lambda *args: native(present_fields=8, azimuth_deg=0)):
    assert not raises(c.CelestialError, lambda: query(observer=replace(site, latitude_deg=90))).recorded
  for apparent in (-1.01e-9, 90.01, math.nan):
    with bindings(
      celestial_topocentric_position=lambda *args, apparent=apparent: native(
        present_fields=16, apparent_altitude_deg=apparent
      )
    ):

      def action():
        return query(refraction=c.RefractionParams(c.RefractionModel.SAEMUNDSSON))

      assert not raises(c.CelestialError, action).recorded
  with bindings(celestial_topocentric_position=lambda *args: native(present_fields=16, apparent_altitude_deg=-1e-10)):
    assert query(refraction=c.RefractionParams(c.RefractionModel.SAEMUNDSSON)).apparent_altitude_deg == -1e-10
  print("PASS observer absent NaN storage, masks, finite required fields and resolved identity seams")


if __name__ == "__main__":
  main()
