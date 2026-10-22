# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Model selectors and resolved identities agree across language boundaries (#357).

These checks reconcile the C defines, ABI manifest codes and Python tables. The compiled
C++ and C ABI tests check the canonical resolvers and the identities returned by queries.
"""

import re

from toolbox.make_abi_manifest import CHART_V1_CODES, REPO_ROOT

HEADER = REPO_ROOT / "src/shared_lib/celestial.h"
INIT = REPO_ROOT / "bindings/python/src/celestial_calendar/__init__.py"

EXPECTED = {
  "DEFAULT": 0,
  "ALGO1": 1,
  "ALGO2": 2,
  "ALGO3": 3,
  "ALGO4": 4,
  "ALGO5": 5,
}


def _header_codes() -> dict[str, int]:
  text = HEADER.read_text(encoding="utf-8")
  found = {}
  for name, value in re.findall(r"#define CHART_DELTA_T_MODEL_(\w+)\s+UINT32_C\((\d+)\)", text):
    found[name] = int(value)
  return found


def _python_chart_codes() -> dict[str, int]:
  """The _CHART_DELTA_T_MODEL table maps DeltaTModel members to codes; mirror it textually."""
  text = INIT.read_text(encoding="utf-8")
  block = re.search(r"_CHART_DELTA_T_MODEL = \{(.*?)\n\}", text, re.DOTALL)
  assert block is not None, "_CHART_DELTA_T_MODEL table not found"
  found = {}
  for member, code in re.findall(r"DeltaTModel\.(\w+): (\d+)", block.group(1)):
    found[member] = int(code)
  return found


def test_header_codes_match_the_expected_table() -> None:
  assert _header_codes() == EXPECTED


def test_manifest_generator_codes_match_the_header() -> None:
  manifest = {name: CHART_V1_CODES[f"CHART_DELTA_T_MODEL_{name}"] for name in EXPECTED}
  assert manifest == EXPECTED


def test_python_chart_mapping_matches_the_header() -> None:
  assert _python_chart_codes() == EXPECTED


def test_python_default_resolves_to_algo5_semantics() -> None:
  """DEFAULT's export name differs from ALGO5's by #303 design, but names the same model."""
  text = INIT.read_text(encoding="utf-8")
  block = re.search(r"_DELTA_T_EXPORT = \{(.*?)\n\}", text, re.DOTALL)
  assert block is not None, "_DELTA_T_EXPORT table not found"
  exports = dict(re.findall(r"DeltaTModel\.(\w+): \"(\w+)\"", block.group(1)))
  assert exports == {
    "DEFAULT": "delta_t",
    "ALGO1": "delta_t_algo1",
    "ALGO2": "delta_t_algo2",
    "ALGO3": "delta_t_algo3",
    "ALGO4": "delta_t_algo4",
    "ALGO5": "delta_t_algo5",
  }
  # The plain delta_t export is Algo5 (delta_t.hpp computes Model::ALGO5); the header
  # states the same in prose next to CHART_DELTA_T_MODEL_DEFAULT.
  header = HEADER.read_text(encoding="utf-8")
  assert re.search(r"CHART_DELTA_T_MODEL_DEFAULT.*?DEFAULT selects Algo5", header, re.DOTALL)


def _capability_codes() -> dict[str, int]:
  """All CAPABILITY_* and MODEL_KIND_*/MODEL_STATUS_* defines in celestial.h."""
  text = HEADER.read_text(encoding="utf-8")
  found = {}
  for name, value in re.findall(r"#define ((?:CAPABILITY|MODEL)_[A-Z0-9_]+)\s+UINT32_C\((\d+)\)", text):
    found[name] = int(value)
  return found


def test_capability_operation_codes_match_python() -> None:
  codes = _capability_codes()
  assert {k: v for k, v in codes.items() if k.startswith("CAPABILITY_OPERATION_")} == {
    "CAPABILITY_OPERATION_POSITION": 0,
    "CAPABILITY_OPERATION_LONGITUDE_RATE": 1,
    "CAPABILITY_OPERATION_EVENTS": 2,
    "CAPABILITY_OPERATION_HOUSES": 3,
    "CAPABILITY_OPERATION_MODEL_IDENTITY": 4,
  }
  capability = (REPO_ROOT / "bindings/python/src/celestial_calendar/capability.py").read_text(encoding="utf-8")
  block = re.search(r"_OPERATION_CODE = \{(.*?)\n\}", capability, re.DOTALL)
  assert block is not None
  python_table = {name: int(code) for name, code in re.findall(r"CapabilityOperation\.(\w+): (\d+)", block.group(1))}
  assert python_table == {"POSITION": 0, "LONGITUDE_RATE": 1, "EVENTS": 2, "HOUSES": 3, "MODEL_IDENTITY": 4}


def test_capability_status_source_and_model_codes_match_python() -> None:
  codes = _capability_codes()
  assert {k: v for k, v in codes.items() if k.startswith("CAPABILITY_STATUS_")} == {
    "CAPABILITY_STATUS_SOURCE_WINDOW": 0,
    "CAPABILITY_STATUS_IMPLEMENTATION_WINDOW": 1,
    "CAPABILITY_STATUS_UNBOUNDED_FINITE": 2,
  }
  assert {k: v for k, v in codes.items() if k.startswith("CAPABILITY_SOURCE_")} == {
    "CAPABILITY_SOURCE_SUN_VSOP87D": 0,
    "CAPABILITY_SOURCE_MOON_ELP2000_82B_TRUNC": 1,
    "CAPABILITY_SOURCE_PLANET_VSOP87D": 2,
    "CAPABILITY_SOURCE_PLUTO_PLUTO95_APPARENT": 3,
    "CAPABILITY_SOURCE_NODE_MEAN": 4,
    "CAPABILITY_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM": 5,
  }
  assert {k: v for k, v in codes.items() if k.startswith("MODEL_KIND_") and k != "MODEL_KIND_ALL"} == {
    "MODEL_KIND_DELTA_T": 0,
    "MODEL_KIND_NUTATION": 1,
    "MODEL_KIND_POSITION_SOURCE": 2,
  }
  assert {k: v for k, v in codes.items() if k.startswith("MODEL_STATUS_")} == {
    "MODEL_STATUS_CURRENT": 0,
    "MODEL_STATUS_FROZEN": 1,
  }

  capability = (REPO_ROOT / "bindings/python/src/celestial_calendar/capability.py").read_text(encoding="utf-8")
  status_block = re.search(r"_STATUS = \{(.*?)\n\}", capability, re.DOTALL)
  source_block = re.search(r"_SOURCE = \{(.*?)\n\}", capability, re.DOTALL)
  kind_block = re.search(r"_KIND_CODE = \{(.*?)\n\}", capability, re.DOTALL)
  assert status_block and source_block and kind_block
  assert dict(re.findall(r"(\d+): CapabilityStatus\.(\w+)", status_block.group(1))) == {
    "0": "SOURCE_WINDOW",
    "1": "IMPLEMENTATION_WINDOW",
    "2": "UNBOUNDED_FINITE",
  }
  assert dict(re.findall(r"(\d+): PositionSource\.(\w+)", source_block.group(1))) == {
    "0": "SUN_VSOP87D",
    "1": "MOON_ELP2000_82B_TRUNC",
    "2": "PLANET_VSOP87D",
    "3": "PLUTO_PLUTO95_APPARENT",
    "4": "NODE_MEAN",
    "5": "NODE_TRUE_MEEUS_FIVE_TERM",
  }
  assert dict(re.findall(r"ModelKind\.(\w+): (\d+)", kind_block.group(1))) == {
    "DELTA_T": "0",
    "NUTATION": "1",
    "POSITION_SOURCE": "2",
  }


def test_kind_internal_identity_ordinals_are_pinned() -> None:
  """The registry's kind-internal ordinals: a separate code space from the CHART_* boundary codes.

  Delta T algo5 is ordinal 4 here but CHART_DELTA_T_MODEL_ALGO5 is 5; comparing a bare
  identity_code against a CHART_* code is always wrong, so the values are pinned by name.
  """
  codes = _capability_codes()
  identity = {k: v for k, v in codes.items() if k.startswith("MODEL_IDENTITY_")}
  assert identity == {
    "MODEL_IDENTITY_DELTA_T_ALGO1": 0,
    "MODEL_IDENTITY_DELTA_T_ALGO2": 1,
    "MODEL_IDENTITY_DELTA_T_ALGO3": 2,
    "MODEL_IDENTITY_DELTA_T_ALGO4": 3,
    "MODEL_IDENTITY_DELTA_T_ALGO5": 4,
    "MODEL_IDENTITY_NUTATION_MEEUS": 0,
    "MODEL_IDENTITY_NUTATION_IAU1980": 1,
    "MODEL_IDENTITY_POSITION_SOURCE_SUN_VSOP87D": 0,
    "MODEL_IDENTITY_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC": 1,
    "MODEL_IDENTITY_POSITION_SOURCE_PLANET_VSOP87D": 2,
    "MODEL_IDENTITY_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT": 3,
    "MODEL_IDENTITY_POSITION_SOURCE_NODE_MEAN": 4,
    "MODEL_IDENTITY_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM": 5,
  }
  # The cross-language consumer asserts the pair, not a bare code: algo5 is (DELTA_T, 4).
  consumer = (REPO_ROOT / "bindings/python/test/consumer/capabilities.py").read_text(encoding="utf-8")
  assert "identity_code) == (celestial.ModelKind.DELTA_T, 4)" in consumer


def test_house_nutation_model_codes_match_python() -> None:
  """The house query's C and Python selectors agree; compiled C ABI tests check the C++ resolver.

  DEFAULT (0) follows the C++ house operation's default IAU 1980; the selector values are
  pinned here so a drift breaks loudly instead of silently selecting another model (#355).
  """
  header = (REPO_ROOT / "src/shared_lib/celestial.h").read_text(encoding="utf-8")
  found = {
    name: int(value)
    for name, value in re.findall(r"#define (HOUSE_NUTATION_MODEL_[A-Z0-9_]+)\s+UINT32_C\((\d+)\)", header)
  }
  assert found == {
    "HOUSE_NUTATION_MODEL_DEFAULT": 0,
    "HOUSE_NUTATION_MODEL_MEEUS": 1,
    "HOUSE_NUTATION_MODEL_IAU_1980": 2,
  }

  query = (REPO_ROOT / "bindings/python/src/celestial_calendar/query.py").read_text(encoding="utf-8")
  table = re.search(r"_NUTATION_MODEL_CODE = \{(.*?)\}", query, re.DOTALL)
  assert table is not None
  assert dict(re.findall(r"NutationModel\.(\w+): (\d+)", table.group(1))) == {
    "DEFAULT": "0",
    "MEEUS": "1",
    "IAU_1980": "2",
  }
  # DEFAULT resolves to IAU 1980, matching the C++ house operation's default.
  resolved = re.search(r"_NUTATION_MODEL_BY_CODE = \{(.*?)\}", query, re.DOTALL)
  assert resolved is not None
  assert dict(re.findall(r"(\d+): NutationModel\.(\w+)", resolved.group(1))) == {
    "0": "IAU_1980",
    "1": "MEEUS",
    "2": "IAU_1980",
  }


def test_query_identity_decoders_match_the_header() -> None:
  query = (REPO_ROOT / "bindings/python/src/celestial_calendar/query.py").read_text(encoding="utf-8")
  codes = _capability_codes()
  delta_t = re.search(r"_DELTA_T_IDENTITY_BY_CODE = \{(.*?)\}", query, re.DOTALL)
  nutation = re.search(r"_NUTATION_IDENTITY_BY_CODE = \{(.*?)\}", query, re.DOTALL)
  assert delta_t is not None and nutation is not None
  assert {int(code): name for code, name in re.findall(r'(\d+): "(\w+)"', delta_t.group(1))} == {
    codes[f"MODEL_IDENTITY_DELTA_T_ALGO{index}"]: f"algo{index}" for index in range(1, 6)
  }
  assert {int(code): name for code, name in re.findall(r"(\d+): NutationModel\.(\w+)", nutation.group(1))} == {
    codes["MODEL_IDENTITY_NUTATION_MEEUS"]: "MEEUS",
    codes["MODEL_IDENTITY_NUTATION_IAU1980"]: "IAU_1980",
  }
