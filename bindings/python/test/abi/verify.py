# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

"""Verify the native header, manifest, ctypes declarations, and loaded library as one ABI."""

from __future__ import annotations

import ast
import copy
import ctypes
import json
import re
import sys
from pathlib import Path

from celestial_calendar import _binding


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
HEADER = REPO / "src" / "shared_lib" / "celestial.h"
SOURCE_DIR = REPO / "src" / "shared_lib"
PYTHON_WRAPPERS = REPO / "bindings" / "python" / "src" / "celestial_calendar" / "__init__.py"
EXPECTED_EXPORT_COUNT = 29
EXPECTED_LAYOUT_COUNT = 16
EXPECTED_RECORDING_COUNT = 7

TYPE_LAYOUT = {
  "bool": (1, 1),
  "uint8_t": (1, 1),
  "uint16_t": (2, 2),
  "int32_t": (4, 4),
  "uint32_t": (4, 4),
  "double": (8, 8),
}
CTYPE_NAMES = {
  ctypes.c_bool: "bool",
  ctypes.c_uint8: "uint8_t",
  ctypes.c_uint16: "uint16_t",
  ctypes.c_int32: "int32_t",
  ctypes.c_uint32: "uint32_t",
  ctypes.c_double: "double",
  ctypes.c_char_p: "const char *",
  _binding.P_U32: "uint32_t *",
  _binding.P_DOUBLE: "double *",
  _binding.P_CHAR: "char *",
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
  for match in re.finditer(r"typedef struct (\w+)\s*\{([\s\S]*?)\}\s*\1;", header):
    name, raw_body = match.groups()
    body = re.sub(r"/\*[\s\S]*?\*/", "", raw_body)
    fields = [
      {"name": field_name, "type": field_type}
      for field_type, field_name in re.findall(r"\b(bool|uint8_t|uint16_t|int32_t|uint32_t|double)\s+(\w+)\s*;", body)
    ]
    offset = 0
    alignment = 1
    for field in fields:
      size, field_alignment = TYPE_LAYOUT[field["type"]]
      offset = align_to(offset, field_alignment)
      field["offset"] = offset
      offset += size
      alignment = max(alignment, field_alignment)
    layouts[name] = {"size": align_to(offset, alignment), "alignment": alignment, "fields": fields}
  return layouts


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
  return_type = restype.__name__ if issubclass(restype, ctypes.Structure) else CTYPE_NAMES[restype]
  return return_type, [CTYPE_NAMES[argtype] for argtype in argtypes]


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
          recording_values.append(False)
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


def verify_manifest(
  manifest: dict[str, object],
  header_exports: dict[str, str],
  header_layouts: dict[str, dict[str, object]],
  documented_recording: set[str],
) -> None:
  """Require one manifest to agree with the header and loaded ctypes binding."""
  manifest_exports = {entry["name"]: entry for entry in manifest["exports"]}
  header_names = unique(list(header_exports), EXPECTED_EXPORT_COUNT, "celestial.h exports")
  manifest_names = unique([entry["name"] for entry in manifest["exports"]], EXPECTED_EXPORT_COUNT, "manifest exports")
  binding_names = unique(list(_binding.BINDING_SPECS), EXPECTED_EXPORT_COUNT, "ctypes bindings")
  function_names = unique(list(_binding.FUNCTIONS), EXPECTED_EXPORT_COUNT, "loaded functions")
  assert header_names == manifest_names == binding_names == function_names

  for name, entry in manifest_exports.items():
    assert canonical(entry["signature"]) == header_exports[name], f"header signature: {name}"
    assert signature_types(entry["signature"]) == ctypes_types(name), f"ctypes signature: {name}"
    assert getattr(_binding.LIB, name) is not None, f"loaded symbol: {name}"

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

  manifest_layouts = manifest["layouts"]
  layout_names = unique(list(manifest_layouts), EXPECTED_LAYOUT_COUNT, "manifest layouts")
  assert layout_names == set(header_layouts) == set(_binding.STRUCT_TYPES)
  assert header_layouts == manifest_layouts
  for name, layout in manifest_layouts.items():
    structure = _binding.STRUCT_TYPES[name]
    ctypes_fields = [
      {"name": field_name, "type": CTYPE_NAMES[field_type]} for field_name, field_type in structure._fields_
    ]
    manifest_fields = [{"name": field["name"], "type": field["type"]} for field in layout["fields"]]
    assert ctypes_fields == manifest_fields, f"ctypes field schema: {name}"
    assert ctypes.sizeof(structure) == layout["size"], f"ctypes size: {name}"
    assert ctypes.alignment(structure) == layout["alignment"], f"ctypes alignment: {name}"
    for field in layout["fields"]:
      assert getattr(structure, field["name"]).offset == field["offset"], f"ctypes offset: {name}.{field['name']}"

  manifest_recording = {entry["name"] for entry in manifest["exports"] if entry["recording"]}
  assert manifest_recording == documented_recording


def run_mutation_self_tests(
  manifest: dict[str, object],
  header_exports: dict[str, str],
  header_layouts: dict[str, dict[str, object]],
  documented_recording: set[str],
) -> None:
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

  wrong_offset = copy.deepcopy(manifest)
  wrong_offset["layouts"]["JieqiMomentQuery"]["fields"][1]["offset"] = 2
  mutations["wrong offset"] = wrong_offset

  wrong_recording = copy.deepcopy(manifest)
  recording_entry = next(entry for entry in wrong_recording["exports"] if entry["name"] == "moon_illumination")
  recording_entry["recording"] = False
  mutations["wrong recording marker"] = wrong_recording

  for label, mutated in mutations.items():
    try:
      verify_manifest(mutated, header_exports, header_layouts, documented_recording)
    except AssertionError:
      continue
    raise AssertionError(f"ABI gate accepted mutation: {label}")


def main() -> None:
  """Run every ABI identity check."""
  assert ctypes.sizeof(ctypes.c_void_p) == 8, "Python native wheels support only 64-bit targets"
  manifest = json.loads((HERE / "manifest.json").read_text(encoding="utf-8"))
  header = HEADER.read_text(encoding="utf-8")
  header_exports = parse_header_exports(header)
  header_names = set(header_exports)
  header_layouts = parse_header_layouts(header)

  recording_match = re.search(r"Only the recording functions \(([\s\S]*?)\) write and clear the message", header)
  assert recording_match is not None, "cannot parse celestial.h recording list"
  documented_recording = set(re.findall(r"`([a-z0-9_]+)`", recording_match.group(1)))
  sources = "\n".join(path.read_text(encoding="utf-8") for path in sorted(SOURCE_DIR.glob("lib*.cpp")))
  implementation_writers = {name for name in header_names if "lib::clear_last_error()" in function_body(sources, name)}
  wrapper_recording = parse_wrapper_recording(PYTHON_WRAPPERS.read_text(encoding="utf-8"))
  assert len(documented_recording) == EXPECTED_RECORDING_COUNT
  assert documented_recording == implementation_writers == set(_binding.RECORDING_EXPORTS)
  wrapper_exports = set(wrapper_recording)
  expected_wrapper_exports = header_names - {"last_error"}
  wrapper_export_difference = sorted(wrapper_exports ^ expected_wrapper_exports)
  assert wrapper_exports == expected_wrapper_exports, f"wrapper exports: {wrapper_export_difference}"
  wrapper_writers = {name for name, recording in wrapper_recording.items() if recording}
  wrapper_recording_difference = sorted(wrapper_writers ^ documented_recording)
  assert wrapper_writers == documented_recording, f"wrapper recording policy: {wrapper_recording_difference}"

  verify_manifest(manifest, header_exports, header_layouts, documented_recording)
  run_mutation_self_tests(manifest, header_exports, header_layouts, documented_recording)

  print("PASS exports header=manifest=ctypes=loaded 29")
  print("PASS layouts header=manifest=ctypes 16")
  print("PASS recording docs=writers=manifest=ctypes=wrappers 7/28")
  print("PASS ABI mutations rejected 5/5")


if __name__ == "__main__":
  try:
    main()
  except AssertionError as error:
    print(f"FAIL: {error}", file=sys.stderr)
    raise
