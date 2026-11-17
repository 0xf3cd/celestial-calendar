# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Installed daily queries: native parity, literal layouts, malformed records and ownership."""

import ctypes
import json
import math
import struct
from dataclasses import FrozenInstanceError
from pathlib import Path

import celestial_calendar as c
from celestial_calendar import _abi, _binding
from protocol_seams import bindings, raises


def f64(bits):
  return struct.unpack("<d", struct.pack("<Q", int(bits, 16)))[0]


def request(day, target, custom):
  year, month, date, latitude, longitude, elevation = day
  label = c.GregorianDate(year, month, date)
  site = c.GeoLocation(latitude, longitude)
  args = [year, month, date, latitude, longitude]
  if target == 0:
    h0 = -18.0 if custom else -50.0 / 60
    return "celestial_solar_rise_set", args + [h0], lambda: c.solar_rise_set(label, site, h0_deg=h0)
  if target == 1:
    weather = (
      c.RefractionParams(c.RefractionModel.SAEMUNDSSON, -20, 800)
      if custom
      else c.RefractionParams(c.RefractionModel.BENNETT)
    )
    return (
      "celestial_lunar_rise_set",
      args + ([2, -20.0, 800.0] if custom else [1, 15.0, 1013.25]),
      lambda: c.lunar_rise_set(label, site, refraction=weather),
    )
  h0 = 0.0 if custom else -34.0 / 60
  return (
    "celestial_planet_rise_set",
    [target, *args, elevation, h0],
    lambda: c.planet_rise_set(tuple(c.Target)[target], label, c.Observer(latitude, longitude, elevation), h0_deg=h0),
  )


def main():
  # Independent C11/native64 pins, deliberately not read from a generated manifest.
  layouts = (
    (_abi.CelestialSolarRiseSet, 56, [0, 4, 8, 12, 16, 20, 24, 32, 40, 48]),
    (_abi.CelestialLunarRiseSet, 72, [0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 56, 64]),
    (_abi.CelestialPlanetRiseSet, 64, [0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 56]),
  )
  for record, size, offsets in layouts:
    assert ctypes.sizeof(record) == size and ctypes.alignment(record) == 8
    assert [getattr(record, field).offset for field, _ in record._fields_] == offsets
  golden = json.loads((Path(__file__).resolve().parents[4] / "toolbox/rise_set_golden.json").read_text())
  assert golden["schema"] == "celestial-calendar/rise-set-parity@1" and len(golden["entries"]) == 360
  sources = [1, 2, 3, 3, 3, 3, 3, 3, 3, 4]
  for entry in golden["entries"]:
    name, args, call = request(golden["days"][entry["day_index"]], entry["target"], entry["custom"])
    raw = _binding.FUNCTIONS[name](*args)
    assert _binding.last_error_kind() == entry["error"]
    if entry["error"]:
      assert not raw.valid
      raises(ValueError, call)
      continue
    actual = call()
    assert raw.valid and actual.source == c.ModelKey(c.ModelKind.POSITION_SOURCE, sources[entry["target"]])
    assert actual.delta_t_model == c.ModelKey(c.ModelKind.DELTA_T, 5)
    assert actual.nutation_model == c.ModelKey(c.ModelKind.NUTATION, 2)
    assert actual.polar.value == {0: "none", 1: "day", 2: "night"}[raw.polar_code]
    for field, bit in (("rise_jde_tt", 32), ("transit_jde_tt", 64), ("set_jde_tt", 128)):
      expected = getattr(raw, field) if raw.present_fields & bit else None
      assert getattr(actual, field) == expected
      # Existing binding moment parity cap; it is not an astronomical accuracy claim.
      assert abs(getattr(raw, field) - f64(entry["fields"][field])) <= 1e-8
    raises(FrozenInstanceError, lambda actual=actual: setattr(actual, "polar", c.PolarTopology.DAY))
  assert "Pluto" in c.capability_text(c.CapabilityOperation.RISE_SET)

  for record, _, _ in layouts:
    lunar = record is _abi.CelestialLunarRiseSet
    planetary = record is _abi.CelestialPlanetRiseSet
    name = (
      "celestial_lunar_rise_set" if lunar else "celestial_planet_rise_set" if planetary else "celestial_solar_rise_set"
    )
    fields = dict(
      valid=True,
      source_code=3 if planetary else 2 if lunar else 1,
      delta_t_identity_code=5,
      nutation_identity_code=2,
      present_fields=224,
      polar_code=0,
      rise_jde_tt=2460000.1,
      transit_jde_tt=2460000.5,
      set_jde_tt=2460000.9,
    )
    if lunar:
      fields |= dict(refraction_code=1, temperature_c=15.0, pressure_hpa=1013.25)
    else:
      fields["h0_deg"] = -34.0 / 60 if planetary else -50.0 / 60
    if planetary:
      fields["target_code"] = 4
    _, _, call = request((2025, 1, 1, 40, 0, 0), 4 if planetary else 1 if lunar else 0, False)
    corruptions = [
      {"valid": False},
      {"present_fields": 225},
      {"source_code": 99},
      {"source_code": 4},
      {"delta_t_identity_code": 0},
      {"delta_t_identity_code": 2},
      {"nutation_identity_code": 0},
      {"nutation_identity_code": 1},
      {"polar_code": 99},
      {"polar_code": 1},
      {"present_fields": 96, "set_jde_tt": 1.0},
    ]
    corruptions += [
      {field: value} for field in ("rise_jde_tt", "transit_jde_tt", "set_jde_tt") for value in (math.nan, math.inf)
    ]
    if lunar:
      corruptions += [{"refraction_code": 2}, {"temperature_c": 16}, {"pressure_hpa": math.nan}]
    else:
      corruptions += [{"h0_deg": 0.0}]
    if planetary:
      corruptions += [{"target_code": 0}]
    else:
      if not lunar:
        corruptions += [{"present_fields": 160}]
    with bindings(celestial_last_error_kind=lambda: 0):
      with bindings(**{name: lambda *args, record=record, fields=fields: record(**fields)}):
        assert call().transit_jde_tt == 2460000.5
      for change in corruptions:
        with bindings(**{name: lambda *args, change=change, record=record, fields=fields: record(**(fields | change))}):
          raises(c.CelestialError, call)
      for mask in (64,) if not lunar and not planetary else (0, 32, 64, 96, 128, 160, 192, 224):
        clean = fields | {"present_fields": mask}
        for field, bit in (("rise_jde_tt", 32), ("transit_jde_tt", 64), ("set_jde_tt", 128)):
          if not mask & bit:
            clean[field] = 0
        with bindings(**{name: lambda *args, clean=clean, record=record: record(**clean)}):
          assert (call().transit_jde_tt is None) == (not mask & 64)
      for kind, error in ((1, ValueError), (2, c.CelestialError), (3, c.CelestialError), (4, c.CelestialError)):
        with bindings(
          celestial_last_error_kind=lambda kind=kind: kind, **{name: lambda *args, record=record: record()}
        ):
          raises(error, call)
  cold = c.lunar_rise_set(
    c.GregorianDate(2025, 1, 1),
    c.GeoLocation(40, 0),
    refraction=c.RefractionParams(c.RefractionModel.BENNETT, -100, 200),
  )
  assert cold.refraction.temperature_c == -100
  raises(FrozenInstanceError, lambda: setattr(cold.refraction, "temperature_c", 0))
  print("PASS rise/set 360 native cases, literal C layouts, masks, identities, owned values and errors")


if __name__ == "__main__":
  main()
