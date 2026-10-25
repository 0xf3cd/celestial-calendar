# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Verify the installed declaration owner, loaded library and independent protocol contracts."""

import ast
import copy
import ctypes
import json
import re
import sys
from pathlib import Path

from celestial_calendar import _abi, _binding

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
sys.path.insert(0, str(REPO))
from automation.c_abi_schema import C_TO_CTYPES, parse_function, parse_header_text, record_layouts  # noqa: E402

HEADER = REPO / "src/shared_lib/celestial.h"
EXPECTED_EXPORT_COUNT = 36
EXPECTED_LAYOUT_COUNT = 28
EXPECTED_RECORDING_COUNT = 34
DECLARATION_SEAMS = ("BINDING_SPECS", "RECORDING_EXPORTS", "STRUCT_TYPES")
READERS = {"celestial_last_error", "celestial_last_error_kind"}
POINTEE_TYPES = {
  **{c: getattr(ctypes, python) for c, python in C_TO_CTYPES.items()},
  **_abi.STRUCT_TYPES,
  "char": ctypes.c_char,
}
CTYPE_NAMES = {
  **{getattr(ctypes, python): c for c, python in C_TO_CTYPES.items()},
  ctypes.c_char_p: "const char *",
  **{
    ctypes.POINTER(POINTEE_TYPES[p.type.base]): p.type.base + " *"
    for function in parse_header_text(HEADER.read_text(encoding="utf-8")).functions.values()
    for p in function.parameters
    if p.type.pointer
  },
}


def canonical(value: str) -> str:
  value = re.sub(r"\s+", " ", value)
  value = re.sub(r"\s*\*\s*", " *", value)
  value = re.sub(r"\s*\(\s*", "(", value)
  value = re.sub(r"\s*\)\s*", ")", value)
  return re.sub(r"\s*,\s*", ", ", value).strip()


def parse_header_exports(header: str) -> dict[str, str]:
  functions = parse_header_text(header).functions
  assert all(f.exported for f in functions.values()), "missing CELESTIAL_API"
  return {name: canonical(f.signature()) for name, f in functions.items()}


def parse_header_layouts(header: str) -> dict:
  return record_layouts(parse_header_text(header).structs, "python")


def parse_codes(header: str) -> dict[str, int]:
  return parse_header_text(header).constants


def signature_types(signature: str) -> tuple[str, list[str]]:
  function = parse_function(signature, parse_header_text(HEADER.read_text(encoding="utf-8")).structs)
  return function.result.spelling(), [p.type.spelling() for p in function.parameters]


def ctypes_type_name(kind: type) -> str:
  if issubclass(kind, ctypes.Array):
    assert kind._length_ > 0, "unbounded or empty ctypes array"
    return f"{ctypes_type_name(kind._type_)}[{kind._length_}]"
  if issubclass(kind, ctypes.Structure):
    assert _abi.STRUCT_TYPES.get(kind.__name__) is kind, "unregistered ctypes record"
    return kind.__name__
  assert kind in CTYPE_NAMES, f"unsupported ctypes type: {kind}"
  return CTYPE_NAMES[kind]


def ctypes_types(name: str) -> tuple[str, list[str]]:
  arguments, result = _binding.BINDING_SPECS[name]
  _, declarations = _abi.C_SIGNATURES[name]
  types = []
  for argument, declared in zip(arguments, declarations, strict=True):
    spelling = ctypes_type_name(argument)
    if declared.startswith("const "):
      spelling = "const " + spelling
    if declared.endswith("* const"):
      spelling += " const"
    types.append(spelling)
  return ctypes_type_name(result), types


def verify_binding_compatibility() -> None:
  """Check shared declaration objects and generated primitive pointer types."""
  for name in DECLARATION_SEAMS:
    assert getattr(_binding, name, None) is getattr(_abi, name), f"declaration owner identity: {name}"
  for name, primitive in (("P_U32", ctypes.c_uint32), ("P_DOUBLE", ctypes.c_double), ("P_CHAR", ctypes.c_char)):
    assert getattr(_abi, name) is ctypes.POINTER(primitive), f"pointer identity: {name}"


def verify_record_layouts(manifest: dict, header: dict) -> None:
  assert len(manifest) == EXPECTED_LAYOUT_COUNT
  assert set(manifest) == set(header) == set(_abi.STRUCT_TYPES)
  assert manifest == header, "header/manifest field layouts"
  for name, layout in manifest.items():
    structure = _abi.STRUCT_TYPES[name]
    fields = [(f, ctypes_type_name(k)) for f, k in structure._fields_]
    assert fields == [(f["name"], f["type"]) for f in layout["fields"]], f"ctypes field schema: {name}"
    assert (ctypes.sizeof(structure), ctypes.alignment(structure)) == (layout["size"], layout["alignment"])
    for field in layout["fields"]:
      assert getattr(structure, field["name"]).offset == field["offset"]
      if "extent" in field:
        array = dict(structure._fields_)[field["name"]]
        assert array._length_ == field["extent"] and ctypes.sizeof(array._type_) == field["stride"]


def verify_protocol_contracts(protocols: dict) -> None:
  """Independent semantic fixtures; these are not a declaration/layout baseline."""
  empty_events = {"celestial_longitude_events", "celestial_longitude_stations", "celestial_longitude_difference_events"}
  nonempty = {
    "celestial_moon_phase_moments",
    "celestial_new_moons_in_year",
    "celestial_capabilities",
    "celestial_model_identities",
    "celestial_positions",
    "celestial_equatorial_positions",
  }
  fills = {name for name, p in protocols.items() if p["kind"] == "count-fill"}
  assert fills == empty_events | nonempty, "count/fill roster"
  for name in fills:
    policy = protocols[name]
    assert policy["legal_empty"] is (name in empty_events), f"legal-empty contract: {name}"
    assert policy["total_must_match"] is True and policy["written_policy"] == "min-total-capacity"
    assert policy["null_slots_requires_zero_capacity"] is True
  strings = {name for name, p in protocols.items() if p["kind"] == "sized-string"}
  assert strings == {"celestial_get_jieqi_name", "celestial_capability_text", "celestial_model_identity_text"}
  for name in strings:
    assert all(
      protocols[name][key] is True
      for key in ("size_includes_nul", "null_requires_zero_capacity", "truncates", "terminates")
    ), f"sized-string contract: {name}"
  assert {name for name, p in protocols.items() if p["error_channel"] == "preserve"} == READERS
  assert all(protocols[name]["error_channel"] == "kind" for name in protocols.keys() - READERS)
  assert protocols["celestial_last_error"]["error_reader"] == "message"
  assert protocols["celestial_last_error_kind"]["error_reader"] == "kind"


def verify_manifest(manifest: dict, exports: dict, layouts: dict, recording: set, codes: dict) -> None:
  names = [entry["name"] for entry in manifest["exports"]]
  assert len(names) == len(set(names)) == EXPECTED_EXPORT_COUNT
  assert set(names) == set(exports) == set(_binding.BINDING_SPECS) == set(_binding.FUNCTIONS)
  verify_binding_compatibility()
  assert manifest["codes"] == _abi.C_CODES == codes
  expected_types = {name: signature_types(signature) for name, signature in exports.items()}
  assert {name: (result, tuple(args)) for name, (result, args) in expected_types.items()} == _abi.C_SIGNATURES
  protocols = {entry["name"]: entry["protocol"] for entry in manifest["exports"]}
  assert protocols == _abi.PROTOCOLS
  verify_protocol_contracts(protocols)
  for entry in manifest["exports"]:
    name = entry["name"]
    assert canonical(entry["signature"]) == exports[name], f"header signature: {name}"
    assert signature_types(entry["signature"]) == ctypes_types(name), f"ctypes signature: {name}"
    args, result = _binding.BINDING_SPECS[name]
    assert getattr(_binding.LIB, name).restype is result
    assert tuple(getattr(_binding.LIB, name).argtypes) == args
    expected_return = (
      {"kind": "struct", "layout": result.__name__}
      if result in _abi.STRUCT_TYPES.values()
      else (
        {"kind": "borrowed-string", "pointer_width_bits": 64}
        if result is ctypes.c_char_p
        else {"kind": "primitive", "type": ctypes_type_name(result)}
      )
    )
    assert entry["return"] == expected_return
  assert {e["name"] for e in manifest["exports"] if e["recording"]} == recording == set(_abi.RECORDING_EXPORTS)
  assert recording == set(exports) - READERS and len(recording) == EXPECTED_RECORDING_COUNT
  assert manifest["native_width_bits"] == {
    "bool": 8,
    "uint8_t": 8,
    "uint16_t": 16,
    "int32_t": 32,
    "uint32_t": 32,
    "pointer": 64,
    "double": 64,
  }
  verify_record_layouts(manifest["layouts"], layouts)


def run_mutation_self_tests(manifest: dict, exports: dict, layouts: dict, recording: set, codes: dict) -> int:
  mutations = {}
  missing_export = copy.deepcopy(manifest)
  missing_export["exports"].pop()
  mutations["missing export"] = missing_export
  for label, path, value in (
    ("wrong signature", ("exports", 0, "signature"), "uint32_t celestial_last_error(void)"),
    ("same-width field type", ("layouts", "CelestialLunarDate", "fields", 3, "type"), "uint8_t"),
    ("same-width event field type", ("layouts", "CelestialStationEventRecord", "fields", 3, "type"), "int32_t"),
    ("wrong offset", ("layouts", "CelestialJieqiMomentQuery", "fields", 1, "offset"), 2),
    ("missing field", ("layouts", "CelestialChartBody", "fields"), []),
    ("unknown field", ("layouts", "CelestialChartBody", "fields", 0, "name"), "unknown"),
    ("identity signedness", ("layouts", "CelestialChartBody", "fields", 0, "type"), "int32_t"),
    ("presence signedness", ("layouts", "CelestialChartBody", "fields", 2, "type"), "int32_t"),
    ("array type extent", ("layouts", "CelestialChartHouses", "fields", 4, "type"), "double[11]"),
    ("array extent", ("layouts", "CelestialChartSnapshot", "fields", 5, "extent"), 13),
    ("array stride", ("layouts", "CelestialChartSnapshot", "fields", 5, "stride"), 40),
    ("nested type", ("layouts", "CelestialChartSnapshot", "fields", 6, "type"), "CelestialChartBody"),
    ("chart identity code", ("codes", "CELESTIAL_TARGET_PLUTO"), 10),
    ("chart presence bit", ("codes", "CELESTIAL_PRESENT_DISTANCE"), 4),
  ):
    changed = copy.deepcopy(manifest)
    node = changed
    for part in path[:-1]:
      node = node[part]
    node[path[-1]] = value
    mutations[label] = changed
  for label, name, key, value in (
    ("missing recording marker", "celestial_moon_illumination", "recording", False),
    ("recording last_error", "celestial_last_error", "recording", True),
    ("chart recording policy", "celestial_chart_snapshot", "recording", False),
    ("swapped event fill protocol", "celestial_longitude_events", "protocol", {"kind": "companion-fill"}),
  ):
    changed = copy.deepcopy(manifest)
    next(e for e in changed["exports"] if e["name"] == name)[key] = value
    mutations[label] = changed
  missing = copy.deepcopy(manifest)
  del missing["codes"]["CELESTIAL_PRESENT_DISTANCE"]
  mutations["missing chart code"] = missing
  extra = copy.deepcopy(manifest)
  extra["codes"]["CELESTIAL_TARGET_CERES"] = 14
  mutations["extra chart code"] = extra
  assert len(mutations) == 21, "independent ABI defect-case count"
  for label, changed in mutations.items():
    try:
      verify_manifest(changed, exports, layouts, recording, codes)
    except AssertionError:
      continue
    raise AssertionError(f"ABI gate accepted mutation: {label}")
  return len(mutations)


def verify_raw_inventory(source: str, names: set[str]) -> None:
  functions = {node.name for node in ast.parse(source).body if isinstance(node, ast.FunctionDef)}
  for name in names:
    assert {"happy_" + name, "edge_" + name} <= functions, f"raw-protocol coverage: {name}"


def function_body(sources: str, name: str) -> str:
  start = re.search(rf"auto\s+{re.escape(name)}\s*\(", sources)
  assert start is not None, f"missing implementation: {name}"
  opening = sources.index("{", start.start())
  depth = 0
  for index in range(opening, len(sources)):
    if sources[index] == "{":
      depth += 1
    elif sources[index] == "}":
      depth -= 1
      if depth == 0:
        return sources[opening : index + 1]
  raise AssertionError(f"unterminated implementation: {name}")


def main() -> None:
  assert ctypes.sizeof(ctypes.c_void_p) == 8, "native wheel pointer width"
  header = HEADER.read_text(encoding="utf-8")
  exports, layouts, codes = parse_header_exports(header), parse_header_layouts(header), parse_codes(header)
  manifest = json.loads((HERE / "manifest.json").read_text(encoding="utf-8"))
  recording = set(exports) - READERS
  verify_manifest(manifest, exports, layouts, recording, codes)
  sources = "\n".join(path.read_text(encoding="utf-8") for path in (REPO / "src/shared_lib").glob("lib*.cpp"))
  assert {name for name in exports if "lib::wrap_export(" in function_body(sources, name)} == recording
  mutations = run_mutation_self_tests(manifest, exports, layouts, recording, codes)
  verify_raw_inventory((HERE / "raw_protocol.py").read_text(encoding="utf-8"), set(exports))
  functions = _binding.FUNCTIONS
  assert not functions["celestial_delta_t"](float("nan"), codes["CELESTIAL_DELTA_T_MODEL_DEFAULT"]).valid
  kind = functions["celestial_last_error_kind"]()
  assert kind != codes["CELESTIAL_ERROR_NONE"]
  functions["celestial_last_error"]()
  assert functions["celestial_last_error_kind"]() == kind, "preserving error readers"
  assert functions["celestial_delta_t"](2000.0, codes["CELESTIAL_DELTA_T_MODEL_DEFAULT"]).valid
  assert functions["celestial_last_error_kind"]() == codes["CELESTIAL_ERROR_NONE"], "recording clears error"
  print(
    f"PASS installed ABI exports={len(exports)} layouts={len(layouts)} recording={len(recording)} mutations={mutations}"
  )


if __name__ == "__main__":
  main()
