# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Fake-native corruption controls for errors, strings, selectors and owned queries."""

from __future__ import annotations

import ctypes
import math
from contextlib import contextmanager
from dataclasses import FrozenInstanceError
from enum import IntEnum
from types import SimpleNamespace
from unittest.mock import patch

import celestial_calendar as celestial
from celestial_calendar import _abi, _binding, _protocol


@contextmanager
def bindings(**replacements):
  missing = object()
  previous = {name: _binding.FUNCTIONS.get(name, missing) for name in replacements}
  _binding.FUNCTIONS.update(replacements)
  try:
    yield
  finally:
    for name, value in previous.items():
      if value is missing:
        del _binding.FUNCTIONS[name]
      else:
        _binding.FUNCTIONS[name] = value


def raises(error_type, action):
  try:
    action()
  except error_type as error:
    return error
  raise AssertionError(f"expected {error_type.__name__}")


class CountFill:
  def __init__(self, records, *, count=None, written=None, fill_total=None, count_written=0):
    self.records = records
    self.total = len(records) if count is None else count
    self.written = written
    self.fill_total = fill_total
    self.count_written = count_written
    self.calls = []

  def __call__(self, *args):
    self.calls.append(args)
    total, slots, capacity = args[-3:]
    if capacity == 0:
      total._obj.value = self.total
      return self.count_written
    total._obj.value = self.total if self.fill_total is None else self.fill_total
    for index, record in enumerate(self.records[:capacity]):
      slots[index] = record
    return len(self.records) if self.written is None else self.written


class Text:
  def __init__(self, payload=b"model\0", *, required=None, written=None):
    self.payload = payload
    self.required = len(payload) if required is None else required
    self.written = self.required if written is None else written
    self.calls = []

  def __call__(self, *args):
    self.calls.append(args)
    buffer, capacity = args[-2:]
    if capacity == 0:
      return self.required
    ctypes.memmove(buffer, self.payload, min(capacity, len(self.payload)))
    return self.written


def position(target=celestial.Target.SUN, *, equatorial=False):
  record_type = _abi.CelestialEquatorialPositionRecord if equatorial else _abi.CelestialPositionRecord
  record = record_type()
  record.target_code = tuple(celestial.Target).index(target)
  node = record.target_code >= 10
  record.present_fields = 1 if node else 3
  record.distance_au = math.nan if node else 1.0
  record.source_code = 5 if node else 1
  if equatorial:
    record.right_ascension_deg = 359.9
    record.declination_deg = -23.0
  else:
    record.longitude_deg = 359.9
    record.latitude_deg = 0.0
  return record


def run_errors():
  for kind, expected in (
    (1, ValueError),
    (2, celestial.CelestialError),
    (3, celestial.CelestialError),
    (4, celestial.CelestialError),
    (99, celestial.CelestialError),
  ):
    for message in (b"reason", b""):
      reads = []

      def read_kind(kind=kind, reads=reads):
        reads.append("kind")
        return kind

      with bindings(
        celestial_last_error_kind=read_kind,
        celestial_last_error=lambda message=message: message,
        celestial_delta_t=lambda *args: SimpleNamespace(valid=False),
      ):
        error = raises(expected, lambda: celestial.delta_t(2024.5))
        if isinstance(error, celestial.CelestialError):
          assert error.recorded is (kind != 99)
        assert reads == ["kind"]
  with bindings(celestial_delta_t=lambda *args: SimpleNamespace(valid=False)):
    error = raises(celestial.CelestialError, lambda: celestial.delta_t(2024.5))
    assert not error.recorded

  # A message alone cannot turn error-kind NONE into failure.
  with bindings(
    celestial_last_error=lambda: b"stale",
    celestial_delta_t=lambda *args: SimpleNamespace(valid=True, value=69.0, model_identity_code=5),
  ):
    assert celestial.delta_t(2024.5) == 69.0
  print("PASS native error kinds, empty messages, corrupt kind and message-independent success")


def run_strings():
  calls = (
    ("celestial_get_jieqi_name", lambda: celestial.jieqi_name(celestial.Jieqi.LICHUN)),
    (
      "celestial_capability_text",
      lambda: celestial.capability_text(celestial.CapabilityOperation.POSITION, celestial.Target.SUN),
    ),
    (
      "celestial_model_identity_text",
      lambda: celestial.model_identity_text(celestial.ModelKey(celestial.ModelKind.DELTA_T, 5)),
    ),
  )
  for export, action in calls:
    for payload in (b"\0", "立春\0".encode(), b"named model\0"):
      text = Text(payload)
      with bindings(**{export: text}):
        assert action() == payload[:-1].decode()
      assert len(text.calls) == 2 and text.calls[0][-2:] == (None, 0)
      assert text.calls[1][-1] == len(payload)
    for text in (
      Text(required=0),
      Text(required=-1),
      Text(required=2**32),
      Text(required=64 * 1024 * 1024 + 1),
      Text(b"bad"),
      Text(b"a\0b\0"),
      Text(b"\xff\0"),
      Text(b"a\0", written=1),
      Text(b"a", required=4),
    ):
      with bindings(**{export: text}):
        error = raises(celestial.CelestialError, action)
      assert not error.recorded
    for phase in (1, 2):
      text = Text()
      with bindings(
        **{
          export: text,
          "celestial_last_error_kind": lambda text=text, phase=phase: 3 if len(text.calls) == phase else 0,
          "celestial_last_error": lambda: b"",
        }
      ):
        error = raises(celestial.CelestialError, action)
      assert error.recorded and len(text.calls) == phase
  print("PASS shared sized strings: UTF-8, NUL, changed size, error at either pass")


def counted_cases():
  target = celestial.Target.SUN
  event = _abi.CelestialLongitudeEventRecord(
    jde_tt=1.5, longitude_deg=2.0, longitude_rate_deg_per_tt_day=1.0, direction_code=0
  )
  station = _abi.CelestialStationEventRecord(
    jde_tt=1.5, longitude_deg=2.0, longitude_rate_deg_per_tt_day=0.0, motion_before_code=0, motion_after_code=1
  )
  difference = _abi.CelestialLongitudeDifferenceEventRecord(
    jde_tt=1.5,
    longitude_a_deg=2.0,
    longitude_b_deg=1.0,
    longitude_rate_a_deg_per_tt_day=1.0,
    longitude_rate_b_deg_per_tt_day=0.0,
    signed_difference_deg=1.0,
    direction_code=0,
  )
  capability = _abi.CelestialCapabilityRecord(
    operation_code=0,
    subject_code=0,
    status_code=2,
    source_code=1,
    domain_start_jde_tt=math.nan,
    domain_end_jde_tt=math.nan,
    maximum_span_tt_days=0,
  )
  identity = _abi.CelestialModelIdentityRecord(
    kind_code=0, identity_code=1, status_code=1, domain_start_jde_tt=math.nan, domain_end_jde_tt=math.nan
  )
  return (
    ("celestial_moon_phase_moments", lambda: celestial.moon_phase_moments(2024, celestial.MoonPhase.NEW), 1.5, False),
    ("celestial_new_moons_in_year", lambda: celestial.new_moons_in_year(2024), 1.5, False),
    ("celestial_positions", lambda: celestial.positions((target,), 2451545.0), position(), False),
    (
      "celestial_equatorial_positions",
      lambda: celestial.equatorial_positions((target,), 2451545.0),
      position(equatorial=True),
      False,
    ),
    ("celestial_longitude_events", lambda: celestial.longitude_events(target, 2.0, 1.0, 2.0), event, True),
    ("celestial_longitude_stations", lambda: celestial.longitude_stations(target, 1.0, 2.0), station, True),
    (
      "celestial_longitude_difference_events",
      lambda: celestial.longitude_difference_events(target, celestial.Target.MOON, 1.0, 1.0, 2.0),
      difference,
      True,
    ),
    (
      "celestial_capabilities",
      lambda: celestial.capabilities(celestial.CapabilityOperation.POSITION, target),
      capability,
      False,
    ),
    ("celestial_model_identities", celestial.model_identities, identity, False),
  )


def run_count_fill():
  for export, action, record, legal_empty in counted_cases():
    empty = CountFill([])
    with bindings(**{export: empty}):
      if legal_empty:
        assert action() == ()
      else:
        raises(celestial.CelestialError, action)
    assert len(empty.calls) == 1
    for fake in (
      CountFill([record], count_written=1),
      CountFill([record], written=0),
      CountFill([record], written=2),
      CountFill([record], fill_total=0),
      CountFill([record], fill_total=2),
      CountFill([record], count=0xFFFFFFFF),
    ):
      with bindings(**{export: fake}):
        error = raises(celestial.CelestialError, action)
      assert not error.recorded
    for phase in (1, 2):
      fake = CountFill([record])
      with bindings(
        **{
          export: fake,
          "celestial_last_error_kind": lambda fake=fake, phase=phase: 2 if len(fake.calls) == phase else 0,
          "celestial_last_error": lambda: b"",
        }
      ):
        error = raises(celestial.CelestialError, action)
      assert error.recorded and len(fake.calls) == phase
  print("PASS shared count/fill: legal empty, forbidden empty, partial fill, changed totals, both error passes")


def run_export_protocols():
  original = _abi.PROTOCOLS
  invalid_cases = 0
  mutations = 0
  for export, action, _, _ in counted_cases():
    bad_metadata = (
      None,
      [],
      {},
      {export: None},
      {export: []},
      {export: {}},
      {export: {"allows_empty": False}},
      *({export: {"legal_empty": value}} for value in (None, 0, 1, "true", [], {})),
    )
    for metadata in bad_metadata:
      native = CountFill([])
      with patch.object(_abi, "PROTOCOLS", metadata), bindings(**{export: native}):
        error = raises(celestial.CelestialError, action)
      assert not error.recorded and not native.calls
      invalid_cases += 1

    native = CountFill([])
    with patch.object(_abi, "PROTOCOLS", {}), bindings(**{export: native}):
      del _abi.PROTOCOLS
      error = raises(celestial.CelestialError, action)
      assert not error.recorded and not native.calls
    invalid_cases += 1

    for metadata in (None, [], {}, {export: None}, {export: 3}, {export: []}):
      native = CountFill([])
      with patch.object(_abi, "C_PARAMETERS", metadata), bindings(**{export: native}):
        error = raises(celestial.CelestialError, action)
      assert not error.recorded and not native.calls
      invalid_cases += 1
    native = CountFill([])
    with patch.object(_abi, "C_PARAMETERS", {}), bindings(**{export: native}):
      del _abi.C_PARAMETERS
      error = raises(celestial.CelestialError, action)
      assert not error.recorded and not native.calls
    invalid_cases += 1

    for key, bad in (
      ("kind", "struct"),
      ("total_must_match", False),
      ("written_policy", "partial"),
      ("null_slots_requires_zero_capacity", False),
      ("error_channel", "message"),
      ("count_parameter", "missing_count"),
      ("slots_parameter", "missing_slots"),
      ("capacity_parameter", "missing_capacity"),
    ):
      protocol = {**original[export], key: bad}
      native = CountFill([])
      with patch.object(_abi, "PROTOCOLS", {**original, export: protocol}), bindings(**{export: native}):
        error = raises(celestial.CelestialError, action)
      assert not error.recorded and not native.calls
      invalid_cases += 1

    policy = {**original[export], "legal_empty": True}
    metadata = {**original, export: policy}
    native = CountFill([])
    with patch.object(_abi, "PROTOCOLS", metadata), bindings(**{export: native}):
      for value in (True, False, True):
        policy["legal_empty"] = value
        if value:
          args = (0,) * (len(_abi.C_PARAMETERS[export]) - 3)
          assert len(_protocol.count_fill(export, args, ctypes.c_double, "metadata_control")) == 0
        else:
          error = raises(
            celestial.CelestialError,
            lambda export=export: _protocol.count_fill(
              export, (0,) * (len(_abi.C_PARAMETERS[export]) - 3), ctypes.c_double, "metadata_control"
            ),
          )
          assert not error.recorded
        mutations += 1
      assert len(native.calls) == 3

    # Annual/event wrappers have no selection-specific total to hide a copied empty policy.
    if export in {
      "celestial_moon_phase_moments",
      "celestial_new_moons_in_year",
      "celestial_longitude_events",
      "celestial_longitude_stations",
      "celestial_longitude_difference_events",
    }:
      native = CountFill([])
      with patch.object(_abi, "PROTOCOLS", metadata), bindings(**{export: native}):
        for value in (True, False, True):
          policy["legal_empty"] = value
          if value:
            assert action() == ()
          else:
            error = raises(celestial.CelestialError, action)
            assert not error.recorded
          mutations += 1
        assert len(native.calls) == 3
    else:
      # Concrete target totals and a complete identity registry remain independent checks.
      policy["legal_empty"] = True
      with patch.object(_abi, "PROTOCOLS", metadata), bindings(**{export: CountFill([])}):
        raises(celestial.CelestialError, action)
  print(f"PASS generated protocol metadata: {invalid_cases} invalid controls; {mutations} live flag mutations")


def run_capability_subjects():
  rejected = 0
  calls = []

  def trap(*args):
    calls.append(args)
    raise AssertionError("invalid capability operation/subject reached native")

  target_operations = (
    celestial.CapabilityOperation.POSITION,
    celestial.CapabilityOperation.LONGITUDE_RATE,
    celestial.CapabilityOperation.EVENTS,
    celestial.CapabilityOperation.EQUATORIAL,
  )
  other_operations = (
    celestial.CapabilityOperation.HOUSES,
    celestial.CapabilityOperation.CHART,
    celestial.CapabilityOperation.DELTA_T,
  )
  with bindings(celestial_capabilities=trap, celestial_capability_text=trap, celestial_house_capability=trap):
    for operation in other_operations:
      for subject in (None, celestial.Target.SUN):
        raises(ValueError, lambda operation=operation, subject=subject: celestial.capabilities(operation, subject))
        rejected += 1
      raises(ValueError, lambda operation=operation: celestial.capability_text(operation, celestial.Target.SUN))
      rejected += 1
      for subject in (True, 0, "sun", celestial.ModelKind.DELTA_T):
        raises(TypeError, lambda operation=operation, subject=subject: celestial.capability_text(operation, subject))
        rejected += 1
    for operation in target_operations:
      for subject in (None, True, 0, "sun", celestial.ModelKind.DELTA_T):
        raises(TypeError, lambda operation=operation, subject=subject: celestial.capability_text(operation, subject))
        rejected += 1
    assert not calls
  for operation in target_operations:
    text = Text()
    with bindings(celestial_capability_text=text):
      assert celestial.capability_text(operation, celestial.Target.SUN) == "model"
    assert len(text.calls) == 2 and all(args[1] == 0 for args in text.calls)
  for operation in other_operations:
    text = Text()
    with bindings(celestial_capability_text=text):
      assert celestial.capability_text(operation) == "model"
    assert len(text.calls) == 2 and all(args[1] == 0xFFFFFFFF for args in text.calls)
  print(f"PASS capability operation/subject guards: {rejected} rejected before native; target/operation text dispatch")


def run_selectors():
  class Foreign(IntEnum):
    ONE = 1

  class IndexOnly:
    def __index__(self):
      return 1

  for kind, identities in (
    (celestial.ModelKind.DELTA_T, range(1, 6)),
    (celestial.ModelKind.NUTATION, range(1, 3)),
    (celestial.ModelKind.POSITION_SOURCE, range(1, 7)),
  ):
    for identity in identities:
      key = celestial.ModelKey(kind, identity)
      raises(FrozenInstanceError, lambda key=key: setattr(key, "identity", 0))
    for bad, error_type in (
      (0, ValueError),
      (2**32, ValueError),
      (-1, ValueError),
      (True, TypeError),
      (Foreign.ONE, TypeError),
      (IndexOnly(), TypeError),
      (1.0, TypeError),
    ):
      raises(error_type, lambda bad=bad, kind=kind: celestial.ModelKey(kind, bad))
  raises(TypeError, lambda: celestial.ModelKey("delta_t", 1))
  for bad in (
    False,
    1,
    Foreign.ONE,
    IndexOnly(),
    celestial.NutationModel.MEEUS,
    celestial.ModelKey(celestial.ModelKind.NUTATION, 1),
  ):
    raises(TypeError, lambda bad=bad: celestial.delta_t(2024.5, bad))
  for bad, error_type in (
    (True, TypeError),
    (Foreign.ONE, TypeError),
    (IndexOnly(), TypeError),
    (2**31, ValueError),
    (-(2**31) - 1, ValueError),
  ):
    raises(error_type, lambda bad=bad: celestial.ut1_to_jd(celestial.CivilDateTime(bad, 1, 1, 0.5)))

  selected = []

  def delta(year, code):
    selected.append((year, code))
    return _abi.CelestialDeltaT(valid=True, model_identity_code=code or 5, value=69.0)

  with bindings(celestial_delta_t=delta):
    for member, code in zip(celestial.DeltaTModel, range(6), strict=True):
      assert celestial.delta_t(2024.5, member) == 69.0 and selected[-1] == (2024.5, code)
    for code in range(1, 6):
      assert celestial.delta_t(2024.5, celestial.ModelKey(celestial.ModelKind.DELTA_T, code)) == 69.0
      assert selected[-1] == (2024.5, code)
  for identity in (0, 6, 256):
    with bindings(
      celestial_delta_t=lambda *args, identity=identity: _abi.CelestialDeltaT(
        valid=True, model_identity_code=identity, value=69.0
      )
    ):
      raises(celestial.CelestialError, lambda: celestial.delta_t(2024.5))
  with bindings(celestial_delta_t=lambda *args: _abi.CelestialDeltaT(valid=True, model_identity_code=4, value=69.0)):
    raises(celestial.CelestialError, lambda: celestial.delta_t(2024.5, celestial.DeltaTModel.ALGO5))
  with bindings(celestial_delta_t=lambda *args: SimpleNamespace(valid=True, value=69.0)):
    raises(celestial.CelestialError, lambda: celestial.delta_t(2024.5))
  print("PASS model keys, generated selectors, kind checks and resolved-key validation")


def run_positions():
  for equatorial, operation, export in (
    (False, celestial.positions, "celestial_positions"),
    (True, celestial.equatorial_positions, "celestial_equatorial_positions"),
  ):
    selection = tuple(reversed(tuple(celestial.Target)))
    native = [position(target, equatorial=equatorial) for target in selection]
    fake = CountFill(native)
    with bindings(**{export: fake}):
      result = operation(iter(selection), 2451545.0)
    assert tuple(record.target for record in result) == selection and len(result) == 14
    assert type(result) is tuple and all(type(record.source) is celestial.ModelKey for record in result)
    for record in result:
      raises(FrozenInstanceError, lambda record=record: setattr(record, "distance_au", 2.0))
    assert result[0].distance_au is None and result[-1].distance_au == 1.0
    before = hash(result)
    native[-1].distance_au = 100.0
    assert hash(result) == before and result[-1].distance_au == 1.0
    assert len(fake.calls) == 2 and list(fake.calls[0][0]) == list(reversed(range(14)))
    for targets, error_type in (
      ([], ValueError),
      ([celestial.Target.SUN] * 2, ValueError),
      ([0], TypeError),
      (["sun"], TypeError),
      ([True], TypeError),
      (None, TypeError),
    ):
      raises(error_type, lambda targets=targets, operation=operation: operation(targets, 2451545.0))
    for value, error_type in (
      (True, TypeError),
      (math.nan, ValueError),
      (math.inf, ValueError),
      (10**1000, ValueError),
    ):
      raises(error_type, lambda value=value, operation=operation: operation((celestial.Target.SUN,), value))
    angular = "right_ascension_deg" if equatorial else "longitude_deg"
    vertical = "declination_deg" if equatorial else "latitude_deg"
    corruptions = (
      ("target_code", 256),
      ("present_fields", 2),
      ("source_code", 0),
      ("source_code", 7),
      (angular, 360.0),
      (angular, math.nan),
      (vertical, 90.1),
      (vertical, math.nan),
      ("distance_au", 0.0),
      ("distance_au", math.inf),
    )
    for name, bad in corruptions:
      native = position(equatorial=equatorial)
      setattr(native, name, bad)
      with bindings(**{export: CountFill([native])}):
        error = raises(
          celestial.CelestialError, lambda operation=operation: operation((celestial.Target.SUN,), 2451545.0)
        )
      assert not error.recorded
    node = position(celestial.Target.TRUE_ASCENDING, equatorial=equatorial)
    node.present_fields = 2
    with bindings(**{export: CountFill([node])}):
      raises(
        celestial.CelestialError, lambda operation=operation: operation((celestial.Target.TRUE_ASCENDING,), 2451545.0)
      )
  print("PASS ecliptic/equatorial ownership, all targets, order, selection guards and coordinate corruption")


def run_native_domain_ownership():
  calls = []

  def native(*args):
    calls.append(args)
    return SimpleNamespace(valid=False)

  civil = celestial.CivilDateTime(1884, 13, 32, 1.0)
  location = celestial.GeoLocation(90.0, 181.0)
  for name, action in (
    (
      "celestial_chart_snapshot",
      lambda: celestial.chart_snapshot(civil, celestial.CivilScale.UTC, location, celestial.HouseSystem.EQUAL),
    ),
    (
      "celestial_house_query",
      lambda: celestial.house_query(civil, celestial.CivilScale.UTC, location, celestial.HouseSystem.EQUAL),
    ),
    ("celestial_delta_t", lambda: celestial.delta_t(2035.0, celestial.DeltaTModel.ALGO4)),
  ):
    with bindings(**{name: native, "celestial_last_error_kind": lambda: 1}):
      raises(ValueError, action)
    assert calls
  assert calls[0][:4] == calls[1][:4] == (1884, 13, 32, 1.0)
  print("PASS native-owned civil/year/geographic/model admission receives un-narrowed values")


def run_capability_and_house_payloads():
  policy = _abi.CelestialCivilHouseCapability(
    valid=True,
    status_code=1,
    civil_start_year=1885,
    civil_end_year_exclusive=2100,
    civil_scale_mask=3,
    utc_start_year=1972,
    utc_start_month=1,
    utc_start_day=1,
    delta_t_identity_code=4,
    has_derived_tt_cutoff=True,
    derived_tt_end_jde_tt=2464328.5,
  )
  with bindings(celestial_house_capability=lambda *args: policy):
    decoded = celestial.house_capability(delta_t_model=celestial.DeltaTModel.ALGO4)
    assert decoded.delta_t_model == celestial.ModelKey(celestial.ModelKind.DELTA_T, 4)
    assert decoded.civil_scales == tuple(celestial.CivilScale)
    assert decoded.utc_start_date == (1972, 1, 1) and decoded.derived_tt_end_jde_tt == 2464328.5
    assert not hasattr(decoded, "domain_start_jde_tt")
    raises(ValueError, lambda: celestial.capabilities(celestial.CapabilityOperation.HOUSES))
  for name, bad in (
    ("status_code", 99),
    ("status_code", 0),
    ("civil_scale_mask", 4),
    ("civil_scale_mask", 0),
    ("delta_t_identity_code", 0),
    ("derived_tt_end_jde_tt", math.nan),
    ("utc_start_month", 13),
    ("civil_start_year", 2100),
  ):
    saved = getattr(policy, name)
    setattr(policy, name, bad)
    with bindings(celestial_house_capability=lambda *args: policy):
      error = raises(celestial.CelestialError, celestial.house_capability)
      assert not error.recorded
    setattr(policy, name, saved)
  policy.has_derived_tt_cutoff = False
  policy.derived_tt_end_jde_tt = math.nan
  with bindings(celestial_house_capability=lambda *args: policy):
    assert celestial.house_capability().derived_tt_end_jde_tt is None
  assert decoded.derived_tt_end_jde_tt == 2464328.5

  record = _abi.CelestialCapabilityRecord(
    operation_code=0,
    subject_code=0,
    status_code=2,
    source_code=1,
    domain_start_jde_tt=math.nan,
    domain_end_jde_tt=math.nan,
  )
  for name, bad in (
    ("operation_code", 99),
    ("operation_code", 1),
    ("status_code", 99),
    ("subject_code", 99),
    ("source_code", 0),
    ("source_code", 7),
    ("domain_start_jde_tt", math.inf),
  ):
    saved = getattr(record, name)
    setattr(record, name, bad)
    with bindings(celestial_capabilities=CountFill([record])):
      raises(
        celestial.CelestialError,
        lambda: celestial.capabilities(celestial.CapabilityOperation.POSITION, celestial.Target.SUN),
      )
    setattr(record, name, saved)
  civil = celestial.CivilDateTime(2024, 6, 1, 0.5)
  location = celestial.GeoLocation(51.5, 0.0)
  house = _abi.CelestialHouseQueryResult(
    valid=True,
    house_system_code=0,
    delta_t_identity_code=3,
    nutation_identity_code=1,
    jd_ut1=2451545.0,
    jde_tt=2451545.001,
  )
  with bindings(celestial_house_query=lambda *args: house):
    result = celestial.house_query(
      civil,
      celestial.CivilScale.UT1,
      location,
      celestial.HouseSystem.EQUAL,
      delta_t_model=celestial.DeltaTModel.ALGO3,
      nutation_model=celestial.NutationModel.MEEUS,
    )
    assert result.delta_t_model == celestial.ModelKey(celestial.ModelKind.DELTA_T, 3)
    assert result.nutation_model == celestial.ModelKey(celestial.ModelKind.NUTATION, 1)
    replay = celestial.house_query(
      civil,
      celestial.CivilScale.UT1,
      location,
      celestial.HouseSystem.EQUAL,
      delta_t_model=result.delta_t_model,
      nutation_model=result.nutation_model,
    )
    assert replay == result
  for name, bad in (
    ("house_system_code", 1),
    ("delta_t_identity_code", 0),
    ("nutation_identity_code", 3),
    ("jde_tt", math.nan),
  ):
    saved = getattr(house, name)
    setattr(house, name, bad)
    with bindings(celestial_house_query=lambda *args: house):
      raises(
        celestial.CelestialError,
        lambda: celestial.house_query(civil, celestial.CivilScale.UT1, location, celestial.HouseSystem.EQUAL),
      )
    setattr(house, name, saved)
  print("PASS TT/civil capability corruption, absent cutoff storage and house model-key replay")


def run_registry_and_events():
  records = []
  for kind, identities in ((0, range(1, 6)), (1, range(1, 3)), (2, range(1, 7))):
    for identity in identities:
      records.append(
        _abi.CelestialModelIdentityRecord(
          kind_code=kind,
          identity_code=identity,
          status_code=0,
          domain_start_jde_tt=math.nan,
          domain_end_jde_tt=math.nan,
        )
      )
  with bindings(celestial_model_identities=CountFill(list(reversed(records)))):
    registry = celestial.model_identities()
  assert len(registry) == 13 and len({item.key for item in registry}) == 13
  for name, bad in (("kind_code", 99), ("identity_code", 0), ("identity_code", 6), ("status_code", 99)):
    saved = getattr(records[0], name)
    setattr(records[0], name, bad)
    with bindings(celestial_model_identities=CountFill(records)):
      raises(celestial.CelestialError, celestial.model_identities)
    setattr(records[0], name, saved)
  with bindings(celestial_model_identities=CountFill([records[0]] * 13)):
    raises(celestial.CelestialError, celestial.model_identities)
  with bindings(celestial_model_identity_text=Text()):
    for item in registry:
      assert celestial.model_identity_text(item.key) == "model"

  event = _abi.CelestialLongitudeEventRecord(
    jde_tt=1.5, longitude_deg=1.0, longitude_rate_deg_per_tt_day=1.0, direction_code=0
  )
  for name, bad in (
    ("direction_code", 99),
    ("jde_tt", math.nan),
    ("jde_tt", 2.0),
    ("longitude_deg", math.nan),
    ("longitude_rate_deg_per_tt_day", math.inf),
  ):
    saved = getattr(event, name)
    setattr(event, name, bad)
    with bindings(celestial_longitude_events=CountFill([event])):
      raises(celestial.CelestialError, lambda: celestial.longitude_events(celestial.Target.SUN, 1.0, 1.0, 2.0))
    setattr(event, name, saved)
  with bindings(celestial_longitude_events=CountFill([event, event])):
    raises(celestial.CelestialError, lambda: celestial.longitude_events(celestial.Target.SUN, 1.0, 1.0, 2.0))
  station = _abi.CelestialStationEventRecord(
    jde_tt=1.5, longitude_deg=1.0, longitude_rate_deg_per_tt_day=0.0, motion_before_code=99, motion_after_code=0
  )
  with bindings(celestial_longitude_stations=CountFill([station])):
    raises(celestial.CelestialError, lambda: celestial.longitude_stations(celestial.Target.SUN, 1.0, 2.0))
  print("PASS registry completeness, pair lookup independent of order and unknown event/motion codes")


def run_calendar_and_angle_payloads():
  civil = dict(year=2024, month=2, day=29, fraction=0.5)
  date = dict(year=2024, month=2, day=29)
  input_civil = celestial.CivilDateTime(**civil)
  input_date = celestial.GregorianDate(**date)
  algorithm = celestial.LunarAlgorithm.ALGO3
  cases = (
    (
      "celestial_jde_to_ut1",
      lambda: celestial.jde_to_ut1(2451545.0),
      civil,
      (("month", 13), ("day", 30), ("year", 1900), ("fraction", -0.1), ("fraction", 1.0), ("fraction", math.nan)),
    ),
    (
      "celestial_apparent_solar_time",
      lambda: celestial.apparent_solar_time(input_civil, 0.0),
      civil,
      (("month", 0), ("day", 30), ("fraction", 1.5)),
    ),
    (
      "celestial_query_jieqi_moment",
      lambda: celestial.jieqi_moment(2024, celestial.Jieqi.LICHUN),
      dict(jq_idx=0, y=2024, m=2, d=29, frac=0.5),
      (("m", 13), ("d", 30), ("frac", 1.5)),
    ),
    (
      "celestial_moon_illumination",
      lambda: celestial.moon_illumination(2451545.0),
      dict(illumination=0.5, elongation_deg=180.0),
      (("illumination", -0.1), ("illumination", 1.5), ("illumination", math.nan), ("elongation_deg", 400.0)),
    ),
    (
      "celestial_moon_position_angle",
      lambda: celestial.moon_bright_limb_position_angle(2451545.0),
      dict(angle_deg=180.0),
      (("angle_deg", -1.0), ("angle_deg", 360.0), ("angle_deg", math.nan)),
    ),
    (
      "celestial_get_supported_lunar_year_range",
      lambda: celestial.supported_lunar_year_range(algorithm),
      dict(start=1600, end=2199),
      (("start", 2200), ("end", 1599)),
    ),
    (
      "celestial_get_lunar_year_info",
      lambda: celestial.lunar_year_info(algorithm, 2024),
      {**date, "leap_month": 0, "month_len": 0},
      (("month", 13), ("day", 30), ("leap_month", 13)),
    ),
    (
      "celestial_gregorian_to_lunar",
      lambda: celestial.gregorian_to_lunar(algorithm, input_date),
      {**date, "is_leap": False},
      (("month", 13), ("day", 0), ("day", 31)),
    ),
    (
      "celestial_lunar_to_gregorian",
      lambda: celestial.lunar_to_gregorian(algorithm, celestial.LunarDate(2024, 2, 29, False)),
      date,
      (("month", 13), ("day", 0), ("day", 30)),
    ),
  )
  count = 0
  for export, action, payload, corruptions in cases:
    with bindings(**{export: lambda *args, payload=payload: SimpleNamespace(valid=True, **payload)}):
      action()
    for field, bad in corruptions:
      corrupted = {**payload, field: bad}
      with bindings(**{export: lambda *args, payload=corrupted: SimpleNamespace(valid=True, **payload)}):
        error = raises(celestial.CelestialError, action)
      assert not error.recorded
      count += 1
  print(f"PASS calendar/angle payloads: {len(cases)} valid controls and {count} malformed successes")

  for export, action, payload, _ in cases:
    field = "y" if export == "celestial_query_jieqi_moment" else "year"
    if field not in payload or export == "celestial_gregorian_to_lunar":
      continue
    for year in (-32767, 0, 32767):
      changed = {**payload, field: year}
      if year != 0:
        if "day" in changed:
          changed["day"] = 1
        else:
          changed["d"] = 1
      with bindings(**{export: lambda *args, payload=changed: SimpleNamespace(valid=True, **payload)}):
        result = action()
      if export == "celestial_get_lunar_year_info":
        date_result = result.first_day
      elif export == "celestial_query_jieqi_moment":
        date_result = result.moment_ut1
      else:
        date_result = result
      assert date_result.year == year
    for year in (-32768, 32768, 99999):
      changed = {**payload, field: year}
      with bindings(**{export: lambda *args, payload=changed: SimpleNamespace(valid=True, **payload)}):
        error = raises(celestial.CelestialError, action)
      assert not error.recorded
  raises(ValueError, celestial.GregorianDate(0, 1, 1).to_date)
  print("PASS representable Gregorian/civil output years, including year zero; Date bridge limits unchanged")


def main():
  with bindings(celestial_last_error_kind=lambda: 0, celestial_last_error=lambda: b""):
    run_errors()
    run_strings()
    run_count_fill()
    run_export_protocols()
    run_calendar_and_angle_payloads()
    run_capability_subjects()
    run_selectors()
    run_positions()
    run_native_domain_ownership()
    run_capability_and_house_payloads()
    run_registry_and_events()
  for old in ("ChartTarget", "EventTarget", "CapabilitySubject", "LogVerbosity", "set_log_verbosity"):
    assert not hasattr(celestial, old)
  assert len(celestial.__all__) == len(set(celestial.__all__))
  public = {
    name
    for name in vars(celestial)
    if not name.startswith("_") and name not in {"chart", "events", "capability", "query"}
  } | {"__version__"}
  assert set(celestial.__all__) == public
  print("PASS Python protocol construction controls")


if __name__ == "__main__":
  main()
