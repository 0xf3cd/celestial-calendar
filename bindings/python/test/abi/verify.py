# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Verify the native header, manifest, ctypes declarations, and loaded library as one ABI."""

from __future__ import annotations

import ast
import copy
import ctypes
import json
import re
import sys
from pathlib import Path

import celestial_calendar
from celestial_calendar import _abi, _binding


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]

# Import the standalone grammar without automation's build-tool package initialization.
sys.path.insert(0, str(REPO / "automation"))
from c_abi_schema import C_TO_CTYPES, parse_c_structs_text, split_field_type  # noqa: E402

HEADER = REPO / "src" / "shared_lib" / "celestial.h"
SOURCE_DIR = REPO / "src" / "shared_lib"
PYTHON_WRAPPERS = Path(_binding.__file__).with_name("__init__.py")
EXPECTED_EXPORT_COUNT = 36
EXPECTED_LAYOUT_COUNT = 24
EXPECTED_RECORDING_COUNT = 35

# Compatibility names of the legacy private binding module.
LEGACY_BINDING_NAMES = (
  "BINDING_SPECS",
  "RECORDING_EXPORTS",
  "STRUCT_TYPES",
  "P_U32",
  "P_DOUBLE",
  "P_CHAR",
  "JulianDay",
  "UT1Time",
  "SunCoordinate",
  "MoonCoordinate",
  "MoonIllumination",
  "MoonPositionAngle",
  "Discriminant",
  "EquationOfTime",
  "ApparentSolarTime",
  "SiderealTime",
  "JieqiMomentQuery",
  "SupportedLunarYearRange",
  "LunarYearInfo",
  "LunarDate",
  "GregorianDate",
  "DeltaT",
  "ChartBodyV1",
  "ChartHousesV1",
  "ChartSnapshotV1",
  "LongitudeEventRecord",
  "StationEventRecord",
  "LongitudeDifferenceEventRecord",
)

TYPE_LAYOUT = {
  "bool": (1, 1),
  "uint8_t": (1, 1),
  "uint16_t": (2, 2),
  "int32_t": (4, 4),
  "uint32_t": (4, 4),
  "double": (8, 8),
}
CTYPE_NAMES = {
  **{getattr(ctypes, python): c for c, python in C_TO_CTYPES.items()},
  ctypes.c_char_p: "const char *",
  ctypes.POINTER(ctypes.c_uint32): "uint32_t *",
  ctypes.POINTER(ctypes.c_double): "double *",
  ctypes.POINTER(ctypes.c_char): "char *",
  ctypes.POINTER(_abi.LongitudeEventRecord): "LongitudeEventRecord *",
  ctypes.POINTER(_abi.StationEventRecord): "StationEventRecord *",
  ctypes.POINTER(_abi.LongitudeDifferenceEventRecord): "LongitudeDifferenceEventRecord *",
  ctypes.POINTER(_abi.CapabilityRecordV1): "CapabilityRecordV1 *",
  ctypes.POINTER(_abi.ModelIdentityRecordV1): "ModelIdentityRecordV1 *",
}


def canonical(value: str) -> str:
  """Normalize inconsequential C whitespace."""
  value = re.sub(r"\s+", " ", value)
  value = re.sub(r"\s*\*\s*", " *", value)
  value = re.sub(r"\s*\(\s*", "(", value)
  value = re.sub(r"\s*\)\s*", ")", value)
  value = re.sub(r"\s*,\s*", ", ", value)
  return value.strip()


def unique(values: list[str], expected: int, label: str) -> set[str]:
  """Require an exact count without duplicates."""
  assert len(values) == expected, f"{label}: expected {expected}, got {len(values)}"
  result = set(values)
  assert len(result) == expected, f"{label}: duplicate entries"
  return result


def parse_header_exports(header: str) -> dict[str, str]:
  """Read all CELESTIAL_API declarations after the public API begins."""
  region = header[header.index("/* ---------- Global configuration ---------- */") :]
  declarations = re.findall(r"CELESTIAL_API\s+([^;]+);", region)
  entries = {}
  for signature in declarations:
    match = re.search(r"([A-Za-z_]\w*)\s*\(", signature)
    assert match is not None, f"cannot parse declaration: {signature}"
    entries[match.group(1)] = canonical(signature)
  assert len(entries) == len(declarations), "duplicate header declarations"
  return entries


def align_to(value: int, alignment: int) -> int:
  """Round a byte offset up to an alignment boundary."""
  return (value + alignment - 1) // alignment * alignment


def parse_header_layouts(header: str) -> dict[str, dict[str, object]]:
  """Compute native 64-bit struct layouts from the C field schemas."""
  layouts = {}
  for name, schema in parse_c_structs_text(header).items():
    fields = []
    offset = 0
    alignment = 1
    for field_name, field_type in schema:
      base, extent = split_field_type(field_type)
      if base in TYPE_LAYOUT:
        size, field_alignment = TYPE_LAYOUT[base]
      else:
        size, field_alignment = layouts[base]["size"], layouts[base]["alignment"]
      offset = align_to(offset, field_alignment)
      field = {"name": field_name, "type": field_type, "offset": offset}
      if extent is not None:
        field.update(extent=extent, stride=size)
      fields.append(field)
      offset += size * (extent if extent is not None else 1)
      alignment = max(alignment, field_alignment)
    layouts[name] = {"size": align_to(offset, alignment), "alignment": alignment, "fields": fields}
  return layouts


def ctypes_type_name(field_type: type) -> str:
  """Keep record identity and array extents in the normalized ctypes type."""
  if issubclass(field_type, ctypes.Array):
    assert field_type._length_ > 0, "unbounded or empty ctypes array"
    return f"{ctypes_type_name(field_type._type_)}[{field_type._length_}]"
  if issubclass(field_type, ctypes.Structure):
    name = field_type.__name__
    assert _binding.STRUCT_TYPES.get(name) is field_type, f"unregistered ctypes record: {name}"
    return name
  assert field_type in CTYPE_NAMES, f"unsupported ctypes type: {field_type}"
  return CTYPE_NAMES[field_type]


def parse_chart_codes(header: str) -> dict[str, int]:
  """Read the complete fixed-width chart selector/identity/presence code set."""
  clean = re.sub(r"\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'|/\*.*?\*/|//[^\n]*", " ", header, flags=re.DOTALL)
  assert clean.isascii() and "\\" not in clean and "##" not in clean and "%:" not in clean, (
    "non-ASCII, backslash or token paste in chart code text"
  )
  tokens = set(re.findall(r"CHART_\w*", clean))
  codes = {}
  for declaration in re.findall(r"^\s*#define\s+(CHART_.*)$", clean, flags=re.MULTILINE):
    match = re.fullmatch(r"(CHART_\w+)\s+UINT32_C\(([0-9]+)\)", declaration.strip())
    assert match is not None, f"unsupported chart code declaration: {declaration}"
    name, value = match.groups()
    assert name not in codes, f"duplicate chart code: {name}"
    codes[name] = int(value)
  assert codes, "missing chart codes"
  assert tokens == set(codes), f"unsupported chart code tokens: {sorted(tokens - set(codes))}"
  return codes


def signature_types(signature: str) -> tuple[str, list[str]]:
  """Extract the normalized return and parameter C types from a manifest signature."""
  match = re.fullmatch(r"(.+?)([A-Za-z_]\w*)\((.*)\)", canonical(signature))
  assert match is not None, f"cannot parse signature: {signature}"
  return_type, _, raw_parameters = match.groups()
  return_type = return_type.strip()
  if raw_parameters == "void":
    return return_type, []
  parameter_types = []
  for parameter in raw_parameters.split(", "):
    parameter_match = re.fullmatch(r"(.+?)([A-Za-z_]\w*)", parameter)
    assert parameter_match is not None, f"cannot parse parameter: {parameter}"
    parameter_types.append(parameter_match.group(1).strip())
  return return_type, parameter_types


def ctypes_types(name: str) -> tuple[str, list[str]]:
  """Return normalized C types from one ctypes declaration."""
  argtypes, restype = _binding.BINDING_SPECS[name]
  return ctypes_type_name(restype), [ctypes_type_name(argtype) for argtype in argtypes]


def function_body(sources: str, name: str) -> str:
  """Extract one C ABI implementation body for the recording-writer check."""
  start_match = re.search(rf"auto\s+{re.escape(name)}\s*\(", sources)
  assert start_match is not None, f"missing implementation for {name}"
  opening = sources.index("{", start_match.start())
  depth = 0
  for index in range(opening, len(sources)):
    if sources[index] == "{":
      depth += 1
    elif sources[index] == "}":
      depth -= 1
      if depth == 0:
        return sources[opening : index + 1]
  raise AssertionError(f"unterminated implementation for {name}")


def parse_wrapper_recording(source: str) -> dict[str, bool]:
  """Map every native export used by a public wrapper to its static recording policy."""
  module = ast.parse(source)
  delta_t_exports = set()
  for statement in module.body:
    if not isinstance(statement, ast.Assign):
      continue
    if not any(isinstance(target, ast.Name) and target.id == "_DELTA_T_EXPORT" for target in statement.targets):
      continue
    assert isinstance(statement.value, ast.Dict)
    delta_t_exports = {
      value.value
      for value in statement.value.values
      if isinstance(value, ast.Constant) and isinstance(value.value, str)
    }
  assert len(delta_t_exports) == 6, "cannot parse _DELTA_T_EXPORT"

  policies = {}
  for function in (node for node in module.body if isinstance(node, ast.FunctionDef) and not node.name.startswith("_")):
    exports = set()
    recording_values = []
    for call in (node for node in ast.walk(function) if isinstance(node, ast.Call)):
      if (
        isinstance(call.func, ast.Attribute)
        and isinstance(call.func.value, ast.Name)
        and call.func.value.id == "_binding"
        and call.func.attr == "call"
      ):
        assert call.args, f"missing binding name in {function.name}"
        binding_name = call.args[0]
        if isinstance(binding_name, ast.Constant) and isinstance(binding_name.value, str):
          exports.add(binding_name.value)
        else:
          assert (
            isinstance(binding_name, ast.Subscript)
            and isinstance(binding_name.value, ast.Name)
            and binding_name.value.id == "_DELTA_T_EXPORT"
          ), f"dynamic binding name in {function.name}"
          exports.update(delta_t_exports)

      if isinstance(call.func, ast.Name) and call.func.id in {"_valid", "_failure"}:
        keyword = next((keyword for keyword in call.keywords if keyword.arg == "recording"), None)
        if keyword is None:
          assert call.func.id == "_valid", f"missing recording policy in {function.name}"
          recording_values.append(True)
        else:
          assert isinstance(keyword.value, ast.Constant) and isinstance(keyword.value.value, bool)
          recording_values.append(keyword.value.value)

    if not exports:
      continue
    assert recording_values and len(set(recording_values)) == 1, f"ambiguous recording policy in {function.name}"
    for name in exports:
      assert name not in policies, f"native export wrapped twice: {name}"
      policies[name] = recording_values[0]
  return policies


def verify_wrapper_recording(source: str, expected_exports: set[str], expected_recording: set[str]) -> None:
  """Require public wrappers to cover every export with the documented recording policy."""
  wrapper_recording = parse_wrapper_recording(source)
  wrapper_exports = set(wrapper_recording)
  wrapper_export_difference = sorted(wrapper_exports ^ expected_exports)
  assert wrapper_exports == expected_exports, f"wrapper exports: {wrapper_export_difference}"
  wrapper_writers = {name for name, recording in wrapper_recording.items() if recording}
  wrapper_recording_difference = sorted(wrapper_writers ^ expected_recording)
  assert wrapper_writers == expected_recording, f"wrapper recording policy: {wrapper_recording_difference}"


def run_wrapper_mutation_self_test(source: str, expected_exports: set[str], expected_recording: set[str]) -> None:
  """Prove wrapper reconciliation rejects one directed recording-policy defect."""
  mutated = source.replace("recording=True", "recording=False", 1)
  assert mutated != source, "cannot inject wrapper recording mutation"
  try:
    verify_wrapper_recording(mutated, expected_exports, expected_recording)
  except AssertionError:
    return
  raise AssertionError("wrapper gate accepted recording mutation")


def verify_record_layouts(manifest_layouts: dict[str, object], header_layouts: dict[str, object]) -> None:
  """Compare the frozen manifest with the header schema and actual ctypes storage."""
  layout_names = unique(list(manifest_layouts), EXPECTED_LAYOUT_COUNT, "manifest layouts")
  assert layout_names == set(header_layouts) == set(_binding.STRUCT_TYPES)
  assert header_layouts == manifest_layouts, "header/manifest field layouts"
  for name, layout in manifest_layouts.items():
    structure = _binding.STRUCT_TYPES[name]
    ctypes_fields = [
      {"name": field_name, "type": ctypes_type_name(field_type)} for field_name, field_type in structure._fields_
    ]
    manifest_fields = [{"name": field["name"], "type": field["type"]} for field in layout["fields"]]
    assert ctypes_fields == manifest_fields, f"ctypes field schema: {name}"
    assert ctypes.sizeof(structure) == layout["size"], f"ctypes size: {name}"
    assert ctypes.alignment(structure) == layout["alignment"], f"ctypes alignment: {name}"
    for field in layout["fields"]:
      assert getattr(structure, field["name"]).offset == field["offset"], f"ctypes offset: {name}.{field['name']}"
      field_type = dict(structure._fields_)[field["name"]]
      if "extent" in field:
        assert issubclass(field_type, ctypes.Array), f"ctypes array: {name}.{field['name']}"
        assert field_type._length_ == field["extent"], f"ctypes extent: {name}.{field['name']}"
        assert ctypes.sizeof(field_type._type_) == field["stride"], f"ctypes stride: {name}.{field['name']}"
        assert ctypes.sizeof(field_type) == field["extent"] * field["stride"], (
          f"ctypes array size: {name}.{field['name']}"
        )


def verify_binding_compatibility() -> None:
  """Require retained private names to alias the actual declaration owner."""
  for name in LEGACY_BINDING_NAMES:
    assert getattr(_binding, name, None) is getattr(_abi, name), f"compatibility declaration identity: {name}"
  for name, primitive in (("P_U32", ctypes.c_uint32), ("P_DOUBLE", ctypes.c_double), ("P_CHAR", ctypes.c_char)):
    assert getattr(_binding, name, None) is ctypes.POINTER(primitive), f"compatibility pointer identity: {name}"


# ctypes shapes alone cannot tell a fixed-capacity companion array from a requested count,
# nor a companion-only count from a plain struct, so those three stay explicit.
AMBIGUOUS_PROTOCOL_KINDS = {
  "solar_lon_root_discriminant": "companion-count",
  "solar_lon_roots": "companion-fill",
  "new_moons_after_jde": "requested-fill",
  "celestial_capability_text": "caller-string",
}


def expected_protocol_kind(name: str) -> str:
  """Derive the manifest protocol kind from the ctypes binding shape."""
  if name in AMBIGUOUS_PROTOCOL_KINDS:
    return AMBIGUOUS_PROTOCOL_KINDS[name]
  argtypes, restype = _binding.BINDING_SPECS[name]
  if isinstance(restype, type) and issubclass(restype, ctypes.Structure):
    return "struct"
  if restype is ctypes.c_char_p:
    return "borrowed-string"
  if restype is ctypes.c_bool:
    return "caller-string" if ctypes.POINTER(ctypes.c_char) in argtypes else "primitive"
  if restype is ctypes.c_uint32:
    assert ctypes.POINTER(ctypes.c_uint32) in argtypes, f"{name} has an unclassified fill shape"
    return "count-fill"
  raise AssertionError(f"{name} has no protocol-kind derivation")


def verify_manifest(
  manifest: dict[str, object],
  header_exports: dict[str, str],
  header_layouts: dict[str, dict[str, object]],
  documented_recording: set[str],
  header_codes: dict[str, int],
) -> None:
  """Require one manifest to agree with the header and loaded ctypes binding."""
  manifest_exports = {entry["name"]: entry for entry in manifest["exports"]}
  header_names = unique(list(header_exports), EXPECTED_EXPORT_COUNT, "celestial.h exports")
  manifest_names = unique([entry["name"] for entry in manifest["exports"]], EXPECTED_EXPORT_COUNT, "manifest exports")
  binding_names = unique(list(_binding.BINDING_SPECS), EXPECTED_EXPORT_COUNT, "ctypes bindings")
  function_names = unique(list(_binding.FUNCTIONS), EXPECTED_EXPORT_COUNT, "loaded functions")
  assert header_names == manifest_names == binding_names == function_names
  verify_binding_compatibility()

  for name, entry in manifest_exports.items():
    assert canonical(entry["signature"]) == header_exports[name], f"header signature: {name}"
    assert signature_types(entry["signature"]) == ctypes_types(name), f"ctypes signature: {name}"
    assert entry["protocol"]["kind"] == expected_protocol_kind(name), f"fill protocol {name}"
    function = getattr(_binding.LIB, name)
    argtypes, restype = _binding.BINDING_SPECS[name]
    assert function.restype is restype, f"loaded return-record identity: {name}"
    assert tuple(function.argtypes) == argtypes, f"loaded argument types: {name}"

  expected_widths = {
    "bool": 8,
    "uint8_t": 8,
    "uint16_t": 16,
    "int32_t": 32,
    "uint32_t": 32,
    "pointer": 64,
    "double": 64,
  }
  assert manifest["native_width_bits"] == expected_widths

  verify_record_layouts(manifest["layouts"], header_layouts)
  assert manifest["chart_v1_codes"] == header_codes, "chart selector/identity/presence codes"

  manifest_recording = {entry["name"] for entry in manifest["exports"] if entry["recording"]}
  assert manifest_recording == documented_recording


def installed_chart_codes() -> dict[str, int]:
  """Name the installed package's native chart codes as celestial.h does."""
  chart = celestial_calendar.chart
  tables = (
    ("CHART_CIVIL_SCALE", chart.CivilScale, chart._CIVIL_SCALE_CODE),
    ("CHART_HOUSE_SYSTEM", chart.HouseSystem, chart._HOUSE_SYSTEM_CODE),
    ("CHART_DELTA_T_MODEL", celestial_calendar.DeltaTModel, celestial_calendar._CHART_DELTA_T_MODEL),
    ("CHART_TARGET", chart.ChartTarget, chart._CHART_TARGET_CODE),
  )
  codes = {}
  for prefix, enum_type, table in tables:
    assert set(table) == set(enum_type), f"{prefix} table covers {enum_type.__name__}"
    codes.update({f"{prefix}_{member.name}": code for member, code in table.items()})
  codes["CHART_PRESENT_LATITUDE"] = chart._PRESENT_LATITUDE
  codes["CHART_PRESENT_DISTANCE"] = chart._PRESENT_DISTANCE
  return codes


def verify_python_codes(manifest: dict[str, object], python_codes: dict[str, int]) -> None:
  """Require the installed package to send and decode the manifest's chart codes."""
  assert python_codes == manifest["chart_v1_codes"], "installed chart code tables"


def installed_event_codes() -> dict[str, int]:
  """Name the installed EventTarget enum as celestial.h's EVENT_TARGET_* codes."""
  return {f"EVENT_TARGET_{member.name}": member.value for member in celestial_calendar.EventTarget}


def verify_event_codes(manifest: dict[str, object], event_codes: dict[str, int]) -> None:
  """Require the event targets to keep the chart identities (#303 pins them equal in C)."""
  chart_targets = {name: code for name, code in manifest["chart_v1_codes"].items() if name.startswith("CHART_TARGET_")}
  expected = {name.replace("CHART_TARGET_", "EVENT_TARGET_", 1): code for name, code in chart_targets.items()}
  assert set(event_codes) == set(expected), "event target names"
  assert event_codes == expected, "installed event target codes"


def parse_event_wire_codes(header_text: str) -> dict[str, int]:
  """Read the EVENT_DIRECTION_* / EVENT_MOTION_* integer codes from celestial.h."""
  codes = {}
  for match in re.finditer(r"^#define\s+(EVENT_(?:DIRECTION|MOTION)_\w+)\s+UINT32_C\((\d+)\)\s*$", header_text, re.M):
    codes[match.group(1)] = int(match.group(2))
  return codes


def verify_event_wire_codes(header_text: str) -> None:
  """Require the installed direction/motion decode tables to keep the header's codes and names."""
  codes = parse_event_wire_codes(header_text)
  directions = {name: code for name, code in codes.items() if name.startswith("EVENT_DIRECTION_")}
  motions = {name: code for name, code in codes.items() if name.startswith("EVENT_MOTION_")}
  assert len(directions) == 3 and set(directions.values()) == set(range(3)), "event direction codes"
  assert len(motions) == 2 and set(motions.values()) == set(range(2)), "station motion codes"
  decode = celestial_calendar.events
  for name, code in directions.items():
    assert decode.decode_direction(code) is getattr(
      celestial_calendar.EventDirection, name.removeprefix("EVENT_DIRECTION_")
    ), f"direction code {name}"
  for name, code in motions.items():
    assert decode.decode_motion(code) is getattr(
      celestial_calendar.StationMotion, name.removeprefix("EVENT_MOTION_")
    ), f"motion code {name}"


def run_event_wire_code_self_test(header_text: str) -> int:
  """Prove that rewriting one direction code in the header text is rejected."""
  swapped = re.sub(
    r"(#define\s+EVENT_DIRECTION_INCREASING\s+UINT32_C\()0(\))",
    r"\g<1>1\g<2>",
    header_text,
    count=1,
  )
  if swapped == header_text:
    raise AssertionError("direction code swap injection did not apply")
  try:
    verify_event_wire_codes(swapped)
  except AssertionError:
    return 1
  raise AssertionError("event wire code gate accepted a rewritten direction code")


def run_event_code_self_test(manifest: dict[str, object], event_codes: dict[str, int]) -> int:
  """Prove that swapping two event target identities is rejected."""
  swapped = dict(event_codes)
  swapped["EVENT_TARGET_MERCURY"], swapped["EVENT_TARGET_VENUS"] = (
    event_codes["EVENT_TARGET_VENUS"],
    event_codes["EVENT_TARGET_MERCURY"],
  )
  try:
    verify_event_codes(manifest, swapped)
  except AssertionError:
    return 1
  raise AssertionError("event code gate accepted swapped target identities")


def run_python_code_self_test(manifest: dict[str, object], python_codes: dict[str, int]) -> int:
  """Prove that swapping two selector codes is rejected."""
  swapped = dict(python_codes)
  swapped["CHART_CIVIL_SCALE_UTC"], swapped["CHART_CIVIL_SCALE_UT1"] = (
    python_codes["CHART_CIVIL_SCALE_UT1"],
    python_codes["CHART_CIVIL_SCALE_UTC"],
  )
  try:
    verify_python_codes(manifest, swapped)
  except AssertionError:
    return 1
  raise AssertionError("chart code gate accepted swapped selector codes")


def run_mutation_self_tests(
  manifest: dict[str, object],
  header_exports: dict[str, str],
  header_layouts: dict[str, dict[str, object]],
  documented_recording: set[str],
  header_codes: dict[str, int],
) -> int:
  """Prove each ABI identity dimension rejects one directed defect."""
  mutations = {}

  missing_export = copy.deepcopy(manifest)
  missing_export["exports"].pop()
  mutations["missing export"] = missing_export

  wrong_signature = copy.deepcopy(manifest)
  wrong_signature["exports"][-1]["signature"] = "DeltaT delta_t(int32_t year)"
  mutations["wrong signature"] = wrong_signature

  wrong_field_type = copy.deepcopy(manifest)
  wrong_field_type["layouts"]["LunarDate"]["fields"][3]["type"] = "uint8_t"
  mutations["same-width field type"] = wrong_field_type

  wrong_event_field_type = copy.deepcopy(manifest)
  wrong_event_field_type["layouts"]["StationEventRecord"]["fields"][3]["type"] = "int32_t"
  mutations["same-width event field type"] = wrong_event_field_type

  swapped_event_fill = copy.deepcopy(manifest)
  event_entry = next(e for e in swapped_event_fill["exports"] if e["name"] == "longitude_events")
  companion = next(e for e in swapped_event_fill["exports"] if e["name"] == "solar_lon_roots")
  [event_entry["protocol"], companion["protocol"]] = [companion["protocol"], event_entry["protocol"]]
  mutations["swapped event fill protocol"] = swapped_event_fill

  wrong_offset = copy.deepcopy(manifest)
  wrong_offset["layouts"]["JieqiMomentQuery"]["fields"][1]["offset"] = 2
  mutations["wrong offset"] = wrong_offset

  wrong_recording = copy.deepcopy(manifest)
  recording_entry = next(entry for entry in wrong_recording["exports"] if entry["name"] == "moon_illumination")
  recording_entry["recording"] = False
  mutations["missing recording marker"] = wrong_recording

  recording_reader = copy.deepcopy(manifest)
  last_error_entry = next(entry for entry in recording_reader["exports"] if entry["name"] == "last_error")
  last_error_entry["recording"] = True
  mutations["recording last_error"] = recording_reader

  for label, record, index, key, value in (
    ("missing field", "ChartBodyV1", 0, None, None),
    ("unknown field", "ChartBodyV1", 0, "name", "unknown"),
    ("identity signedness", "ChartBodyV1", 0, "type", "int32_t"),
    ("presence signedness", "ChartBodyV1", 1, "type", "int32_t"),
    ("array type extent", "ChartHousesV1", 4, "type", "double[11]"),
    ("array extent", "ChartSnapshotV1", 3, "extent", 13),
    ("array stride", "ChartSnapshotV1", 3, "stride", 32),
    ("nested type", "ChartSnapshotV1", 4, "type", "ChartBodyV1"),
  ):
    mutated = copy.deepcopy(manifest)
    fields = mutated["layouts"][record]["fields"]
    if key is None:
      fields.pop(index)
    else:
      fields[index][key] = value
    mutations[label] = mutated

  wrong_chart_recording = copy.deepcopy(manifest)
  next(entry for entry in wrong_chart_recording["exports"] if entry["name"] == "chart_snapshot_v1")["recording"] = False
  mutations["chart recording policy"] = wrong_chart_recording

  wrong_identity = copy.deepcopy(manifest)
  wrong_identity["chart_v1_codes"]["CHART_TARGET_PLUTO"] = 10
  mutations["chart identity code"] = wrong_identity

  wrong_presence = copy.deepcopy(manifest)
  wrong_presence["chart_v1_codes"]["CHART_PRESENT_DISTANCE"] = 4
  mutations["chart presence bit"] = wrong_presence

  missing_code = copy.deepcopy(manifest)
  del missing_code["chart_v1_codes"]["CHART_PRESENT_DISTANCE"]
  mutations["missing chart code"] = missing_code

  extra_code = copy.deepcopy(manifest)
  extra_code["chart_v1_codes"]["CHART_TARGET_CERES"] = 14
  mutations["extra chart code"] = extra_code

  assert len(mutations) == 21, "independent ABI defect-case count"
  for label, mutated in mutations.items():
    try:
      verify_manifest(mutated, header_exports, header_layouts, documented_recording, header_codes)
    except AssertionError:
      continue
    raise AssertionError(f"ABI gate accepted mutation: {label}")
  return len(mutations)


def main() -> None:
  """Run every ABI identity check."""
  assert ctypes.sizeof(ctypes.c_void_p) == 8, "Python native wheels support only 64-bit targets"
  manifest = json.loads((HERE / "manifest.json").read_text(encoding="utf-8"))
  header = HEADER.read_text(encoding="utf-8")
  header_exports = parse_header_exports(header)
  header_names = set(header_exports)
  header_layouts = parse_header_layouts(header)
  header_codes = parse_chart_codes(header)

  assert "Every exported function except `last_error` writes and clears the message" in header
  documented_recording = header_names - {"last_error"}
  sources = "\n".join(path.read_text(encoding="utf-8") for path in sorted(SOURCE_DIR.glob("lib*.cpp")))
  implementation_writers = {name for name in header_names if "lib::wrap_export(" in function_body(sources, name)}
  wrapper_source = PYTHON_WRAPPERS.read_text(encoding="utf-8")
  assert len(documented_recording) == EXPECTED_RECORDING_COUNT
  assert documented_recording == implementation_writers == set(_binding.RECORDING_EXPORTS)
  expected_wrapper_exports = header_names - {"last_error"}
  verify_wrapper_recording(wrapper_source, expected_wrapper_exports, documented_recording)

  verify_manifest(manifest, header_exports, header_layouts, documented_recording, header_codes)
  python_codes = installed_chart_codes()
  verify_python_codes(manifest, python_codes)
  code_mutation_count = run_python_code_self_test(manifest, python_codes)
  assert code_mutation_count == 1, "chart code self-test ran"
  event_codes = installed_event_codes()
  verify_event_codes(manifest, event_codes)
  event_code_mutation_count = run_event_code_self_test(manifest, event_codes)
  assert event_code_mutation_count == 1, "event code self-test ran"
  header_text = HEADER.read_text(encoding="utf-8")
  verify_event_wire_codes(header_text)
  wire_code_mutation_count = run_event_wire_code_self_test(header_text)
  assert wire_code_mutation_count == 1, "event wire code self-test ran"
  mutation_count = run_mutation_self_tests(manifest, header_exports, header_layouts, documented_recording, header_codes)
  run_wrapper_mutation_self_test(wrapper_source, expected_wrapper_exports, documented_recording)

  print(f"PASS exports header=manifest=ctypes=loaded {len(header_exports)}")
  print(f"PASS layouts header=manifest=ctypes {len(header_layouts)}; nested records and array extents/strides")
  recording_count, wrapper_count = len(documented_recording), len(expected_wrapper_exports)
  print(
    f"PASS recording policies {recording_count}/{recording_count}; wrapper exports {wrapper_count}/{wrapper_count}; "
    "docs=writers=manifest=ctypes=wrappers"
  )
  print(
    "PASS chart selector/identity/presence codes; installed tables=manifest, "
    f"swapped selectors rejected {code_mutation_count}/1"
  )
  print(
    "PASS event target identities = chart identities; installed enum=manifest, "
    f"swapped targets rejected {event_code_mutation_count}/1"
  )
  print(
    "PASS event direction/motion wire codes = header codes, installed tables decode them, "
    f"rewritten codes rejected {wire_code_mutation_count}/1"
  )
  print(f"PASS ABI mutations rejected manifest={mutation_count}/{mutation_count} wrapper=1/1")


if __name__ == "__main__":
  try:
    main()
  except AssertionError as error:
    print(f"FAIL: {error}", file=sys.stderr)
    raise
