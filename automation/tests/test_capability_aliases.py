# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Independent model, target and operation identities for the approved C vocabulary."""

import re

from automation.abi_layout import load_abi
from automation.c_abi_schema import parse_header
from toolbox.make_abi_manifest import REPO_ROOT, build_manifest


def codes():
  return parse_header(REPO_ROOT / "src/shared_lib/celestial.h").constants


def family(prefix):
  return {name.removeprefix(prefix): value for name, value in codes().items() if name.startswith(prefix)}


def test_single_target_roster_has_named_independent_values():
  assert family("CELESTIAL_TARGET_") == {
    "SUN": 0,
    "MOON": 1,
    "MERCURY": 2,
    "VENUS": 3,
    "MARS": 4,
    "JUPITER": 5,
    "SATURN": 6,
    "URANUS": 7,
    "NEPTUNE": 8,
    "PLUTO": 9,
    "MEAN_ASCENDING": 10,
    "MEAN_DESCENDING": 11,
    "TRUE_ASCENDING": 12,
    "TRUE_DESCENDING": 13,
    "ALL": 0xFFFFFFFF,
  }
  assert not any(name.startswith(("CHART_", "EVENT_TARGET_", "MODEL_IDENTITY_")) for name in codes())


def test_selectable_model_identities_are_explicit_and_default_is_input_only():
  assert family("CELESTIAL_DELTA_T_MODEL_") == {
    "DEFAULT": 0,
    "ALGO1": 1,
    "ALGO2": 2,
    "ALGO3": 3,
    "ALGO4": 4,
    "ALGO5": 5,
  }
  assert family("CELESTIAL_NUTATION_MODEL_") == {"DEFAULT": 0, "MEEUS": 1, "IAU_1980": 2}
  assert family("CELESTIAL_POSITION_SOURCE_") == {
    "SUN_VSOP87D": 1,
    "MOON_ELP2000_82B_TRUNC": 2,
    "PLANET_VSOP87D": 3,
    "PLUTO_PLUTO95_APPARENT": 4,
    "NODE_MEAN": 5,
    "NODE_TRUE_MEEUS_FIVE_TERM": 6,
  }
  assert family("CELESTIAL_MODEL_KIND_") == {"DELTA_T": 0, "NUTATION": 1, "POSITION_SOURCE": 2, "ALL": 0xFFFFFFFF}
  assert family("CELESTIAL_MODEL_STATUS_") == {"CURRENT": 0, "FROZEN": 1}


def test_operation_roster_and_tt_records_do_not_invent_house_domains():
  assert family("CELESTIAL_OPERATION_") == {
    "POSITION": 0,
    "LONGITUDE_RATE": 1,
    "EVENTS": 2,
    "HOUSES": 3,
    "EQUATORIAL": 4,
    "CHART": 5,
    "DELTA_T": 6,
  }
  assert family("CELESTIAL_STATUS_") == {
    "SOURCE_WINDOW": 0,
    "IMPLEMENTATION_WINDOW": 1,
    "FINITE_INPUT_EVALUATION": 2,
  }
  abi = load_abi()
  assert abi.PROTOCOLS["celestial_capabilities"]["operations"] == [
    "CELESTIAL_OPERATION_POSITION",
    "CELESTIAL_OPERATION_LONGITUDE_RATE",
    "CELESTIAL_OPERATION_EVENTS",
    "CELESTIAL_OPERATION_EQUATORIAL",
  ]
  assert abi.PROTOCOLS["celestial_capability_text"]["nontarget_operations"] == [
    "CELESTIAL_OPERATION_HOUSES",
    "CELESTIAL_OPERATION_CHART",
    "CELESTIAL_OPERATION_DELTA_T",
  ]
  assert abi.PROTOCOLS["celestial_capability_text"]["subject_policy"] == (
    "concrete-target-for-target-operations-all-for-nontarget-operations"
  )


def test_all_generated_code_views_preserve_exact_names_and_values():
  assert load_abi().C_CODES == codes()
  assert build_manifest("python")["codes"] == build_manifest("javascript")["codes"] == codes()
  assert family("CELESTIAL_CIVIL_SCALE_") == {"UTC": 0, "UT1": 1}
  assert family("CELESTIAL_HOUSE_SYSTEM_") == {"EQUAL": 0, "WHOLE_SIGN": 1, "PLACIDUS": 2}
  assert family("CELESTIAL_PRESENT_") == {"LATITUDE": 1, "DISTANCE": 2}


def test_capability_validation_owner_names_resolve_to_tests():
  source = (REPO_ROOT / "src/shared_lib/lib_capability.cpp").read_text()
  names = set(re.findall(r"\b\w+_test\.cpp\b", source))
  test_names = {path.name for path in (REPO_ROOT / "src/test").rglob("*_test.cpp")}
  assert names and not names - test_names
