# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Fault controls for closed ABI grammars, with compiled C as the layout witness."""

import ast
import copy
import ctypes
import importlib.util
import json
import sys
from pathlib import Path
from types import ModuleType

import pytest

from automation import abi_layout, export_surface
from automation.c_abi_schema import parse_c_structs_text


ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "src/shared_lib/celestial.h"
MIRROR = ROOT / "statistics/common.py"
ABI = ROOT / "bindings/python/test/abi"
BINDING = ROOT / "bindings/python/src/celestial_calendar/_binding.py"

C_CONTROL = """
typedef struct Body { uint32_t code; double longitude; } Body;
typedef struct Alias { uint32_t code; double longitude; } Alias;
typedef struct Snapshot {
  bool valid; Body bodies[14]; // fixed roster
  double cusps[12]; /* one-dimensional */ Alias nested;
} Snapshot;
"""
PY_CONTROL = """
class _Body(Structure):
  _fields_ = [("code", c_uint32), ("longitude", c_double)]
class _Alias(ctypes.Structure):
  _fields_ = [("code", ctypes.c_uint32), ("longitude", ctypes.c_double)]
class _Snapshot(Structure):
  _fields_ = [("valid", c_bool), ("bodies", _Body * 14), ("cusps", c_double * 12), ("nested", _Alias)]
"""


def mirror(tmp_path, source):
  path = tmp_path / "mirror.py"
  path.write_text(source, encoding="utf-8")
  return abi_layout.parse_py_structs(path)


def test_closed_grammars_accept_comments_inline_fields_and_earlier_records(tmp_path):
  expected = {
    "Body": [("code", "uint32_t"), ("longitude", "double")],
    "Alias": [("code", "uint32_t"), ("longitude", "double")],
    "Snapshot": [("valid", "bool"), ("bodies", "Body[14]"), ("cusps", "double[12]"), ("nested", "Alias")],
  }
  assert parse_c_structs_text(C_CONTROL) == expected
  assert mirror(tmp_path, PY_CONTROL) == expected


@pytest.mark.parametrize(
  "declaration",
  [
    "float extra;",
    "Unknown extra;",
    "Future extra;",
    "Body *extra;",
    "const double extra;",
    "signed int extra;",
    "double extra[0];",
    "double extra[-1];",
    "double extra[];",
    "double extra[COUNT];",
    "double extra[1 + 2];",
    "double extra[0xC];",
    "double extra[014];",
    "double extra[12u];",
    "double extra[12][2];",
    "uint32_t extra : 1;",
    "double x, extra;",
    "double extra",
    "double extra; stray",
    "double longitude;",
    "#if OMIT\ndouble extra;\n#endif",
    "struct { double value; } extra;",
    ";",
  ],
)
def test_c_member_faults_cannot_disappear(declaration):
  # A known-valid member precedes the fault: the old findall parser silently lost it.
  header = f"typedef struct Body {{ double longitude; {declaration} }} Body;"
  with pytest.raises(RuntimeError):
    parse_c_structs_text(header)


@pytest.mark.parametrize(
  "header",
  [
    "typedef struct { double x; } Body;",
    "typedef struct Body { double x; } Other;",
    "typedef struct Body { } Body;",
    "typedef struct Body { Body self; } Body;",
    "typedef struct Body { Future nested; } Body; typedef struct Future { double x; } Future;",
    "typedef struct Body { double x; } Body; typedef struct Body { double y; } Body;",
    "typedef struct double { uint32_t x; } double;",
    "/* typedef struct Fake { double x; } Fake; */",
  ],
)
def test_c_record_faults_are_loud(header):
  with pytest.raises(RuntimeError):
    parse_c_structs_text(header)


@pytest.mark.parametrize(
  "field_type",
  [
    "c_float",
    "Unknown",
    "_Future",
    "POINTER(c_double)",
    "ctypes.c_void_p",
    "other.c_double",
    "c_double * 0",
    "c_double * -1",
    "c_double * count",
    "c_double * True",
    "c_double * 12.0",
    "c_double * (6 + 6)",
    "12 * c_double",
    "(c_double * 12) * 2",
    "_Body * 0",
  ],
)
def test_mirror_type_and_extent_faults_are_loud(tmp_path, field_type):
  source = PY_CONTROL.replace("_Body * 14", field_type)
  with pytest.raises(RuntimeError):
    mirror(tmp_path, source)


@pytest.mark.parametrize(
  "body",
  [
    "pass",
    "_fields_ = fields",
    "_fields_ = []",
    "_fields_ = [('x', c_double, 1)]",
    "_fields_ = [(1, c_double)]",
    "_fields_ = [('x', c_double), ('x', c_double)]",
    "_fields_ = [('bad.name', c_double)]",
    "_fields_ = [('x', c_double)]; _fields_ = [('y', c_double)]",
    "_fields_ = [('x', c_double)]; _pack_ = 1",
    "_fields_ = [('x', c_double)]; _align_ = 16",
    "_fields_ = [('x', c_double)]; _layout_ = 'ms'",
    "_fields_ = [('x', c_double)]; _anonymous_ = ('x',)",
    "_fields_ = [('x', c_double)]; _fields_ += [('y', c_double)]",
    "_fields_ = [('x', c_double)]; _fields_.append(('y', c_double))",
    "_fields_: list = [('x', c_double)]",
    "_fields_ = other = [('x', c_double)]",
  ],
)
def test_mirror_declaration_faults_are_loud(tmp_path, body):
  with pytest.raises(RuntimeError):
    mirror(tmp_path, f"class Broken(Structure):\n  {body}\n")


def test_mirror_unknown_base_late_fields_and_normalized_name_collisions(tmp_path):
  for source in (
    "class Broken(other.Structure):\n  _fields_ = [('x', c_double)]",
    "class Broken(Structure):\n  pass\nBroken._fields_ = [('x', c_double)]",
    "class Broken(Structure):\n  _fields_ = [('x', c_double)]\nBroken._pack_ = 1",
    "class Broken(Structure):\n  _fields_ = [('x', c_double)]\n  if True:\n    _fields_ = [('y', c_double)]",
    PY_CONTROL + "\nclass Body(Structure):\n  _fields_ = [('x', c_double)]",
  ):
    with pytest.raises(RuntimeError):
      mirror(tmp_path, source)


@pytest.mark.parametrize(
  "mutation",
  [
    lambda source: source.replace("class _Body(Structure):", "@(lambda cls: _Alias)\nclass _Body(Structure):"),
    lambda source: source.replace("class _Body(Structure):", "class _Body(Structure, metaclass=type):"),
    lambda source: source + "\nif True:\n  class _Body(Structure):\n    _fields_ = [('code', c_int32)]\n",
    lambda source: source + "\n_Body = _Alias\n",
    lambda source: source + "\nc_uint32 = c_int32\n",
    lambda source: source.replace("class _Alias(ctypes.Structure):", "  del _fields_\nclass _Alias(ctypes.Structure):"),
    lambda source: source.replace(
      "class _Alias(ctypes.Structure):", "  (_fields_ := [('code', c_int32)])\nclass _Alias(ctypes.Structure):"
    ),
    lambda source: source + "\nexec('_Body = _Alias')\n",
    lambda source: source + "\nsetattr(_Body, '_fields_', [])\n",
    lambda source: source + "\nfor _Body in []:\n  pass\n",
    lambda source: source + "\nfrom other import c_uint32\n",
    lambda source: source + "\nctypes.c_uint32 = ctypes.c_int32\n",
  ],
)
def test_mirror_namespace_and_computed_class_faults_are_loud(tmp_path, mutation):
  expected = mirror(tmp_path, PY_CONTROL)
  with pytest.raises(RuntimeError):
    mirror(tmp_path, mutation(PY_CONTROL))
  assert mirror(tmp_path, PY_CONTROL) == expected


@pytest.fixture(scope="module")
def compiled_layout(tmp_path_factory):
  work = tmp_path_factory.mktemp("native-layout")
  structs = abi_layout.parse_c_structs(HEADER)
  layout = abi_layout.c_layout(structs, HEADER, work)
  assert layout is not None, "fresh C probe must compile and run before layout acceptance"
  return structs, layout


def test_actual_header_manifest_and_statistics_mirror_against_compiled_c(compiled_layout):
  structs, layout = compiled_layout
  parsed_mirror = abi_layout.parse_py_structs(MIRROR)
  assert len(structs) == len(parsed_mirror) == 19
  assert not abi_layout.compare_layouts(structs, parsed_mirror, layout)
  manifest = json.loads((ABI / "manifest.json").read_text(encoding="utf-8"))
  for name, record in manifest["layouts"].items():
    assert record["size"] == layout[name]
    assert record["alignment"] == layout[f"{name}.alignment"]
    assert [(field["name"], field["type"]) for field in record["fields"]] == structs[name]
    for field in record["fields"]:
      key = f"{name}.{field['name']}"
      assert field["offset"] == layout[key]
      if "extent" in field:
        assert field["extent"] == layout[key + ".extent"]
        assert field["stride"] == layout[key + ".stride"]


@pytest.mark.parametrize(
  "old,new",
  [
    ('("target_code", c_uint32)', '("target_code", c_int32)'),
    ('("present_fields", c_uint32)', '("present_fields", c_int32)'),
    ('("valid", c_bool)', '("valid", c_uint8)'),
    ('("distance_au", c_double),', ""),
    ('("distance_au", c_double)', '("unknown", c_double)'),
    ("c_double * 12", "c_double * 11"),
    ("_ChartBodyV1 * 14", "_ChartBodyV1 * 13"),
    ('("houses", _ChartHousesV1)', '("houses", _ChartBodyV1)'),
    ('("houses", _ChartHousesV1)', '("houses", _SunCoordinate)'),
    ('("lon", c_double),\n    ("lat", c_double)', '("lat", c_double),\n    ("lon", c_double)'),
  ],
)
def test_statistics_faults_detect_same_width_types_order_arrays_and_nesting(tmp_path, compiled_layout, old, new):
  structs, ground_truth = compiled_layout
  source = MIRROR.read_text(encoding="utf-8")
  assert old in source, "mutation anchor must exist"
  mutated = mirror(tmp_path, source.replace(old, new, 1))
  assert abi_layout.compare_layouts(structs, mutated, ground_truth)
  assert not abi_layout.compare_layouts(structs, abi_layout.parse_py_structs(MIRROR), ground_truth)


def test_statistics_record_set_and_array_stride_faults(compiled_layout):
  structs, ground_truth = compiled_layout
  parsed = abi_layout.parse_py_structs(MIRROR)
  missing = copy.deepcopy(parsed)
  del missing["DeltaT"]
  assert abi_layout.compare_layouts(structs, missing, ground_truth)
  extra = copy.deepcopy(parsed)
  extra["Extra"] = [("value", "double")]
  assert abi_layout.compare_layouts(structs, extra, ground_truth)
  wrong_stride = dict(ground_truth, **{"ChartSnapshotV1.bodies.stride": 32})
  assert any("bodies.stride" in finding for finding in abi_layout.compare_layouts(structs, parsed, wrong_stride))


def test_equal_layout_record_substitution_still_breaks_identity(tmp_path):
  header = tmp_path / "celestial.h"
  header.write_text("#include <stdbool.h>\n#include <stdint.h>\n" + C_CONTROL, encoding="utf-8")
  structs = abi_layout.parse_c_structs(header)
  ground_truth = abi_layout.c_layout(structs, header, tmp_path)
  assert ground_truth is not None
  pristine = mirror(tmp_path, PY_CONTROL)
  mutated = mirror(tmp_path, PY_CONTROL.replace("_Body * 14", "_Alias * 14"))
  assert abi_layout.py_layout(pristine) == abi_layout.py_layout(mutated)
  assert not abi_layout.compare_layouts(structs, pristine, ground_truth)
  assert abi_layout.compare_layouts(structs, mutated, ground_truth)


@pytest.fixture
def native_verifier(monkeypatch):
  # Execute only source declarations, never the loader or a source-tree package import.
  source = ast.parse(BINDING.read_text(encoding="utf-8"))
  names = {"P_U32", "P_DOUBLE", "P_CHAR", "BINDING_SPECS", "STRUCT_TYPES", "RECORDING_EXPORTS"}
  declarations = [
    node
    for node in source.body
    if isinstance(node, ast.ClassDef)
    or isinstance(node, ast.Assign)
    and any(isinstance(t, ast.Name) and t.id in names for t in node.targets)
  ]
  binding = ModuleType("source_declarations_only")
  binding.__file__ = str(BINDING)
  binding.ctypes = ctypes
  exec(compile(ast.Module(body=declarations, type_ignores=[]), str(BINDING), "exec"), binding.__dict__)
  package = ModuleType("celestial_calendar")
  package._binding = binding
  monkeypatch.setitem(sys.modules, "celestial_calendar", package)
  old_path = list(sys.path)
  spec = importlib.util.spec_from_file_location("native_abi_verifier", ABI / "verify.py")
  verifier = importlib.util.module_from_spec(spec)
  try:
    spec.loader.exec_module(verifier)
  finally:
    sys.path[:] = old_path
  return verifier


def test_native_static_witnesses_and_raw_inventory(native_verifier):
  verifier = native_verifier
  header = HEADER.read_text(encoding="utf-8")
  manifest = json.loads((ABI / "manifest.json").read_text(encoding="utf-8"))
  verifier.verify_record_layouts(manifest["layouts"], verifier.parse_header_layouts(header))
  exports = verifier.parse_header_exports(header)
  assert len(exports) == 30 and len(manifest["exports"]) == 30
  assert not export_surface.self_test_parser()
  assert export_surface.parse_declarations(header) == [(name, True) for name in exports]
  for entry in manifest["exports"]:
    assert verifier.canonical(entry["signature"]) == exports[entry["name"]]
    assert verifier.signature_types(entry["signature"]) == verifier.ctypes_types(entry["name"])
  assert manifest["chart_v1_codes"] == verifier.parse_chart_codes(header)
  source = (ROOT / "bindings/python/src/celestial_calendar/__init__.py").read_text(encoding="utf-8")
  writers = set(exports) - {"last_error"}
  assert len(writers) == 29
  verifier.verify_wrapper_recording(source, writers, writers)
  verifier.run_wrapper_mutation_self_test(source, writers, writers)
  raw = ast.parse((ABI / "raw_protocol.py").read_text(encoding="utf-8"))
  raw_names = {node.name for node in raw.body if isinstance(node, ast.FunctionDef)}
  for name in exports:
    assert f"happy_{name}" in raw_names and f"edge_{name}" in raw_names


@pytest.mark.parametrize(
  "declaration",
  [
    "# define CHART_TARGET_CERES UINT32_C(14)",
    "enum { CHART_TARGET_CERES = 14 };",
    "const uint32_t CHART_TARGET_CERES = 14;",
    "#define CHART_TARGET_CERES INT32_C(14)",
  ],
)
def test_chart_code_declarations_cannot_hide_from_inventory(native_verifier, declaration):
  header = HEADER.read_text(encoding="utf-8")
  expected = native_verifier.parse_chart_codes(header)
  with pytest.raises(AssertionError):
    native_verifier.parse_chart_codes(header + "\n" + declaration + "\n")
  assert native_verifier.parse_chart_codes(header) == expected


@pytest.mark.parametrize("declaration", ["double lost[];", "double lost[0];", "Unknown lost;", "double *lost;"])
def test_native_parser_cannot_silently_skip_members(native_verifier, declaration):
  with pytest.raises(RuntimeError):
    native_verifier.parse_header_layouts(f"typedef struct Broken {{ bool valid; {declaration} }} Broken;")


@pytest.mark.parametrize(
  "record,index,key,value",
  [
    ("ChartBodyV1", 0, "type", "int32_t"),
    ("ChartBodyV1", 1, "type", "int32_t"),
    ("ChartBodyV1", 0, "name", "unknown"),
    ("ChartBodyV1", 0, None, None),
    ("ChartSnapshotV1", 3, "type", "ChartBodyV1[13]"),
    ("ChartSnapshotV1", 3, "extent", 13),
    ("ChartSnapshotV1", 3, "stride", 32),
    ("ChartSnapshotV1", 4, "type", "ChartBodyV1"),
    ("ChartHousesV1", 4, "type", "double[11]"),
    ("ChartHousesV1", 4, "stride", 4),
  ],
)
def test_native_ctypes_witness_rejects_colluding_header_manifest_faults(native_verifier, record, index, key, value):
  pristine = json.loads((ABI / "manifest.json").read_text(encoding="utf-8"))["layouts"]
  mutated = copy.deepcopy(pristine)
  if key is None:
    mutated[record]["fields"].pop(index)
  else:
    mutated[record]["fields"][index][key] = value
  # Let header and manifest agree on the defect; the independent ctypes declaration must object.
  with pytest.raises(AssertionError):
    native_verifier.verify_record_layouts(mutated, mutated)
  native_verifier.verify_record_layouts(
    pristine, native_verifier.parse_header_layouts(HEADER.read_text(encoding="utf-8"))
  )


def test_ctypes_type_identity_and_array_extent_are_not_erased(native_verifier):
  assert native_verifier.ctypes_type_name(ctypes.c_double * 12) == "double[12]"
  body = native_verifier._binding.STRUCT_TYPES["ChartBodyV1"]
  assert native_verifier.ctypes_type_name(body * 14) == "ChartBodyV1[14]"
  with pytest.raises(AssertionError):
    native_verifier.ctypes_type_name(ctypes.c_double * 0)
  with pytest.raises(AssertionError):
    native_verifier.ctypes_type_name(type("ChartBodyV1", (ctypes.Structure,), {"_fields_": body._fields_}))
