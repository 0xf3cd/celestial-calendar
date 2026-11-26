# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Installed scalar disks: source facts, literal ABI forwarding, ownership and malformed native records."""

import ctypes
import math
from dataclasses import FrozenInstanceError

import celestial_calendar as c
from celestial_calendar import _abi, _binding
from protocol_seams import bindings, raises


def main() -> None:
  record = _abi.CelestialApparentDiameter
  assert ctypes.sizeof(record) == 64 and ctypes.alignment(record) == 8
  assert [(name, getattr(record, name).offset) for name, _ in record._fields_] == [
    ("valid", 0),
    ("target_code", 4),
    ("centre_code", 8),
    ("source_code", 12),
    ("radius_definition_code", 16),
    ("delta_t_identity_code", 20),
    ("jd_ut1", 24),
    ("jde_tt", 32),
    ("radius_km", 40),
    ("distance_au", 48),
    ("diameter_arcsec", 56),
  ]
  raw = _binding.FUNCTIONS["celestial_apparent_diameter"]
  assert raw.restype is record
  assert tuple(raw.argtypes) == (
    ctypes.c_uint32,
    ctypes.c_int32,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_double,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_double,
    ctypes.c_double,
    ctypes.c_double,
  )
  civil = c.CivilDateTime(2026, 11, 7, 0.0)
  site = c.Observer(-33.85, 151.2, 1706.0)
  # IAU/JPL radius facts and ephemeris identities pinned independently of the native production table.
  radii = (695700.0, 1737.4, 2440.53, 6051.8, 3396.19, 71492.0, 60268.0, 25559.0, 24764.0, 1188.3)
  sources = (1, 2, 3, 3, 3, 3, 3, 3, 3, 4)
  definitions = (c.RadiusDefinition.IAU_2015_SOLAR_NOMINAL, c.RadiusDefinition.JPL_SSD_SATELLITE_MEAN)
  for index, target in enumerate(tuple(c.Target)[:10]):
    geo = c.apparent_diameter(target, civil, c.CivilScale.UTC)
    assert geo.target is target and geo.centre is c.DiameterCentre.GEOCENTRIC
    assert geo.radius_km == radii[index] and geo.source == c.ModelKey(c.ModelKind.POSITION_SOURCE, sources[index])
    assert geo.radius_definition is (definitions[index] if index < 2 else c.RadiusDefinition.JPL_SSD_PLANET_EQUATORIAL)
    assert geo.delta_t_model == c.ModelKey(c.ModelKind.DELTA_T, 5)
    assert geo.times.jde_tt == c.utc_to_jde(civil)
    assert geo.distance_au == c.positions([target], geo.times.jde_tt)[0].distance_au
    ratio = radii[index] / 149597870.7 / geo.distance_au
    expected = math.degrees(2 * math.atan2(ratio, math.sqrt(1 - ratio * ratio))) * 3600
    assert abs(geo.diameter_arcsec - expected) <= 1e-10
    for latitude in (-90.0, -33.85, 90.0):
      observer = c.Observer(latitude, 151.2, 1706.0)
      topo = c.apparent_diameter(target, civil, c.CivilScale.UTC, observer)
      assert topo.centre is c.DiameterCentre.TOPOCENTRIC
      assert topo.distance_au == c.topocentric_position(target, civil, c.CivilScale.UTC, observer).distance_au
      assert topo.radius_km == radii[index] and topo.source == geo.source
    raises(FrozenInstanceError, lambda geo=geo: setattr(geo, "diameter_arcsec", 0))
    raises(FrozenInstanceError, lambda geo=geo: setattr(geo.times, "jde_tt", 0))
  print("PASS diameter ten-target radius/source/convention facts, tangent geometry, poles and owned results")

  def forward(*args):
    assert args == (1, 2026, 11, 7, 0.0, 1, 2, 1, -33.85, 151.2, 1706.0)
    return raw(*args)

  with bindings(celestial_apparent_diameter=forward):
    selected = c.apparent_diameter(c.Target.MOON, civil, c.CivilScale.UT1, site, delta_t_model=c.DeltaTModel.ALGO2)
  assert selected.delta_t_model == c.ModelKey(c.ModelKind.DELTA_T, 2)
  assert selected.times.jde_tt != c.apparent_diameter(c.Target.MOON, civil, c.CivilScale.UT1, site).times.jde_tt
  assert raw(1, 2026, 11, 7, 0.0, 0, 0, 0, math.nan, math.nan, math.nan).valid
  for selector_index in (0, 5, 6, 7):
    for bad in (99, 256, 0xFFFFFFFF):
      args = [1, 2026, 11, 7, 0.0, 0, 0, 0, 0.0, 0.0, 0.0]
      args[selector_index] = bad
      assert not raw(*args).valid and _binding.last_error_kind() == 1
      assert raw(1, 2026, 11, 7, 0.0, 0, 0, 0, 0.0, 0.0, 0.0).valid and _binding.last_error_kind() == 0
  for target in tuple(c.Target)[10:]:
    raises(ValueError, lambda target=target: c.apparent_diameter(target, civil, c.CivilScale.UTC))
  raises(ValueError, lambda: c.apparent_diameter(c.Target.PLUTO, c.CivilDateTime(2100, 1, 1, 0.0), c.CivilScale.UTC))
  raises(
    ValueError,
    lambda: c.apparent_diameter(
      c.Target.SUN, c.CivilDateTime(2036, 1, 1, 0.0), c.CivilScale.UT1, delta_t_model=c.DeltaTModel.ALGO4
    ),
  )
  assert c.apparent_diameter(c.Target.SUN, c.CivilDateTime(1000, 1, 1, 0.0), c.CivilScale.UT1).diameter_arcsec > 0
  assert "spherical" in c.capability_text(c.CapabilityOperation.APPARENT_DIAMETER)
  huge = c.apparent_diameter(c.Target.MOON, civil, c.CivilScale.UTC, c.Observer(0, 0, 1e303))
  assert 0 < huge.diameter_arcsec < 1e-280
  print("PASS diameter literal ABI units/forwarding, selectors, provider/model guards and recovery")
  print(f"PASS diameter huge observer: {huge.diameter_arcsec:.17e} arcsec")

  fields = dict(
    valid=True,
    target_code=1,
    centre_code=0,
    source_code=2,
    radius_definition_code=2,
    delta_t_identity_code=5,
    jd_ut1=2461351.5,
    jde_tt=2461351.501,
    radius_km=1737.4,
    distance_au=0.0025,
    diameter_arcsec=1900.0,
  )
  corruptions = [
    dict(valid=False),
    dict(target_code=0),
    dict(centre_code=1),
    dict(centre_code=99),
    dict(source_code=0),
    dict(source_code=99),
    dict(radius_definition_code=0),
    dict(radius_definition_code=99),
    dict(delta_t_identity_code=0),
    dict(delta_t_identity_code=99),
    dict(radius_km=0),
    dict(radius_km=-1),
    dict(distance_au=0),
    dict(distance_au=-1),
    dict(distance_au=1737.4 / 149597870.7),
    dict(diameter_arcsec=0),
    dict(diameter_arcsec=-1),
    dict(diameter_arcsec=648000),
  ]
  for field in ("jd_ut1", "jde_tt", "radius_km", "distance_au", "diameter_arcsec"):
    corruptions += [{field: value} for value in (math.nan, math.inf, -math.inf)]
  for changes in corruptions:
    with bindings(celestial_apparent_diameter=lambda *args, changes=changes: record(**(fields | changes))):
      raises(c.CelestialError, lambda: c.apparent_diameter(c.Target.MOON, civil, c.CivilScale.UTC))
  # These synthetic SUN records test code recognition, not native per-body metadata correctness.
  for source_code, definition_code, definition in (
    (4, 1, c.RadiusDefinition.IAU_2015_SOLAR_NOMINAL),
    (1, 3, c.RadiusDefinition.JPL_SSD_PLANET_EQUATORIAL),
  ):
    reported_fields = fields | dict(target_code=0, source_code=source_code, radius_definition_code=definition_code)
    with bindings(celestial_apparent_diameter=lambda *args, reported_fields=reported_fields: record(**reported_fields)):
      reported = c.apparent_diameter(c.Target.SUN, civil, c.CivilScale.UTC)
    assert reported.target is c.Target.SUN
    assert reported.source == c.ModelKey(c.ModelKind.POSITION_SOURCE, source_code)
    assert reported.radius_definition is definition
  print("PASS diameter recognized source/definition passthrough for synthetic SUN records")
  with bindings(celestial_apparent_diameter=lambda *args: record(**fields)):
    raises(
      c.CelestialError,
      lambda: c.apparent_diameter(c.Target.MOON, civil, c.CivilScale.UTC, delta_t_model=c.DeltaTModel.ALGO2),
    )
  with bindings(celestial_apparent_diameter=lambda *args: record(**(fields | {"diameter_arcsec": 1e-300}))):
    assert c.apparent_diameter(c.Target.MOON, civil, c.CivilScale.UTC).diameter_arcsec > 0
  print(f"PASS diameter {len(corruptions)} malformed-native controls, recognized request identity and tiny positives")


if __name__ == "__main__":
  main()
