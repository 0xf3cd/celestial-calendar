# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Independent semantic controls consume policy fields, never protocol prose."""

import copy

import pytest

from automation.abi_layout import load_abi
from toolbox import make_abi_manifest

EMPTY_EVENTS = {"celestial_longitude_events", "celestial_longitude_stations", "celestial_longitude_difference_events"}
NONEMPTY = {
  "celestial_moon_phase_moments",
  "celestial_new_moons_in_year",
  "celestial_capabilities",
  "celestial_model_identities",
  "celestial_positions",
  "celestial_equatorial_positions",
}
STRINGS = {"celestial_get_jieqi_name", "celestial_capability_text", "celestial_model_identity_text"}
READERS = {"celestial_last_error", "celestial_last_error_kind"}


def test_preserving_readers_and_authoritative_error_kind():
  abi = load_abi()
  assert abi.C_CODES["CELESTIAL_ERROR_NONE"] == 0
  assert {name for name in abi.BINDING_SPECS if name not in abi.RECORDING_EXPORTS} == READERS
  assert {name for name, p in abi.PROTOCOLS.items() if p["error_channel"] == "preserve"} == READERS
  assert {name for name, p in abi.PROTOCOLS.items() if p.get("error_reader")} == READERS
  assert all(abi.PROTOCOLS[name]["error_channel"] == "kind" for name in abi.RECORDING_EXPORTS)
  assert not hasattr(abi, "DeltaT") and not hasattr(abi, "ChartSnapshotV1")


def consume_count_fill(policy, query_total, fill_total, written, *, error_kind=0, query_error_kind=0, message=""):
  """A fake binding consumer demonstrates that an empty message is not a success signal."""
  for kind in (query_error_kind, error_kind):
    if kind != 0:
      raise RuntimeError(f"native kind {kind}: {message}")
  if query_total == 0 and not policy["legal_empty"]:
    raise RuntimeError("unexpected empty result")
  if policy["total_must_match"] and fill_total != query_total:
    raise RuntimeError("changed full total")
  if policy["written_policy"] == "min-total-capacity" and written != query_total:
    raise RuntimeError("partial fill")
  return written


@pytest.mark.parametrize("name", sorted(EMPTY_EVENTS | NONEMPTY))
def test_count_fill_contracts_are_machine_readable_and_discriminate_corruption(name):
  abi = load_abi()
  policy = abi.PROTOCOLS[name]
  assert policy["kind"] == "count-fill"
  assert policy["legal_empty"] is (name in EMPTY_EVENTS)
  assert policy["total_must_match"] is True
  assert policy["null_slots_requires_zero_capacity"] is True
  assert consume_count_fill(policy, 3, 3, 3) == 3
  with pytest.raises(RuntimeError, match="changed full total"):
    consume_count_fill(policy, 3, 4, 3)
  with pytest.raises(RuntimeError, match="partial fill"):
    consume_count_fill(policy, 3, 3, 2)
  with pytest.raises(RuntimeError, match="native kind"):
    consume_count_fill(policy, 0, 0, 0, error_kind=2, message="")
  with pytest.raises(RuntimeError, match="native kind"):
    consume_count_fill(policy, 0, 0, 0, query_error_kind=2, message="")
  if name in EMPTY_EVENTS:
    assert consume_count_fill(policy, 0, 0, 0) == 0
  else:
    with pytest.raises(RuntimeError, match="unexpected empty"):
      consume_count_fill(policy, 0, 0, 0)
  changed_prose = copy.deepcopy(policy)
  changed_prose["description"] = "an empty result is fine"
  assert changed_prose["legal_empty"] is policy["legal_empty"]


@pytest.mark.parametrize("name", sorted(STRINGS))
def test_all_sized_strings_have_nul_inclusive_truncating_contract(name):
  abi = load_abi()
  policy = abi.PROTOCOLS[name]
  assert policy["kind"] == "sized-string"
  assert all(
    policy[key] is True for key in ("size_includes_nul", "null_requires_zero_capacity", "truncates", "terminates")
  )
  signature = abi.C_SIGNATURES[name]
  assert signature[0] == "uint32_t" and "char *" in signature[1]
  # For a required five-byte string, capacity one writes only NUL; null with capacity one is invalid.
  capacity, required = 1, 5
  assert min(required - 1, capacity - 1) == 0
  assert policy["null_requires_zero_capacity"] and capacity != 0


@pytest.mark.parametrize("key,value", [("legal_empty", "yes"), ("legal_empty", None), ("total_must_match", False)])
def test_generator_refuses_incomplete_count_policy(monkeypatch, key, value):
  protocol = make_abi_manifest.EXPORT_SEMANTICS["celestial_positions"]["protocol"]
  monkeypatch.setitem(protocol, key, value)
  with pytest.raises(RuntimeError, match="count/fill policy"):
    make_abi_manifest.declaration_source()


@pytest.mark.parametrize("key", ["size_includes_nul", "null_requires_zero_capacity", "truncates", "terminates"])
def test_generator_refuses_incomplete_string_policy(monkeypatch, key):
  protocol = make_abi_manifest.EXPORT_SEMANTICS["celestial_model_identity_text"]["protocol"]
  monkeypatch.delitem(protocol, key)
  with pytest.raises(RuntimeError, match="sized-string policy"):
    make_abi_manifest.declaration_source()


def test_protocol_pointer_roles_are_declared_parameters(monkeypatch):
  protocol = make_abi_manifest.EXPORT_SEMANTICS["celestial_positions"]["protocol"]
  monkeypatch.setitem(protocol, "count_parameter", "not_a_parameter")
  with pytest.raises(RuntimeError, match="Unknown protocol parameter"):
    make_abi_manifest.declaration_source()


def test_protocol_role_cannot_point_at_an_incompatible_existing_parameter(monkeypatch):
  protocol = make_abi_manifest.EXPORT_SEMANTICS["celestial_positions"]["protocol"]
  monkeypatch.setitem(protocol, "count_parameter", "slots")
  with pytest.raises(RuntimeError, match="Incompatible protocol pointer role"):
    make_abi_manifest.declaration_source()


def test_protocol_operation_references_must_exist(monkeypatch):
  protocol = make_abi_manifest.EXPORT_SEMANTICS["celestial_capabilities"]["protocol"]
  monkeypatch.setitem(protocol, "operations", ["CELESTIAL_OPERATION_UNKNOWN"])
  with pytest.raises(RuntimeError, match="Unknown protocol operation"):
    make_abi_manifest.declaration_source()
