# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Fault controls for the C grammar and real ctypes records, measured against compiled C."""

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
ABI = ROOT / "bindings/python/test/abi"
DECLARATIONS = ROOT / "bindings/python/src/celestial_calendar/_abi.py"

C_CONTROL = """
typedef struct Body { uint32_t code; double longitude; } Body;
typedef struct Alias { uint32_t code; double longitude; } Alias;
typedef struct Snapshot {
  bool valid; Body bodies[14]; // fixed roster
  double cusps[12]; /* one-dimensional */ Alias nested;
} Snapshot;
"""
PY_CONTROL = """
import ctypes
from ctypes import Structure, c_bool, c_uint32, c_double
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
  spec = importlib.util.spec_from_file_location("abi_fixture", path)
  module = importlib.util.module_from_spec(spec)
  exec(compile(source, str(path), "exec"), module.__dict__)
  return abi_layout.runtime_layout(module)


def test_closed_grammars_accept_comments_inline_fields_and_earlier_records(tmp_path):
  expected = {
    "Body": [("code", "uint32_t"), ("longitude", "double")],
    "Alias": [("code", "uint32_t"), ("longitude", "double")],
    "Snapshot": [("valid", "bool"), ("bodies", "Body[14]"), ("cusps", "double[12]"), ("nested", "Alias")],
  }
  assert parse_c_structs_text(C_CONTROL) == expected
  schema, _ = mirror(tmp_path, PY_CONTROL)
  assert schema == expected


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


@pytest.fixture(scope="module")
def compiled_layout(tmp_path_factory):
  work = tmp_path_factory.mktemp("native-layout")
  structs = abi_layout.parse_c_structs(HEADER)
  layout = abi_layout.c_layout(structs, HEADER, work)
  assert layout is not None, "fresh C probe must compile and run before layout acceptance"
  return structs, layout


def test_actual_header_manifest_and_declarations_against_compiled_c(compiled_layout):
  structs, layout = compiled_layout
  schema, measured = abi_layout.runtime_layout(abi_layout.load_abi())
  assert len(structs) == len(schema) == 19
  assert not abi_layout.compare_layouts(structs, schema, layout, measured=measured)
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


def test_real_declarations_import_without_a_native_library(monkeypatch):
  def reject_load(*arguments, **keywords):
    pytest.fail("declaration checks must not load a native library")

  monkeypatch.setattr(ctypes, "CDLL", reject_load)
  before = {name for name in sys.modules if name.startswith("celestial_calendar")}
  module = abi_layout.load_abi()
  assert len(module.STRUCT_TYPES) == 19 and len(module.BINDING_SPECS) == 30
  assert {name for name in sys.modules if name.startswith("celestial_calendar")} == before
  abi_layout.runtime_layout(module)


def test_declared_return_identity_is_checked_without_lib():
  module = abi_layout.load_abi()
  foreign = abi_layout.load_abi()
  assert module.DeltaT is not foreign.DeltaT
  arguments, original = module.BINDING_SPECS["delta_t"]
  assert ctypes.sizeof(original) == ctypes.sizeof(foreign.DeltaT)
  module.BINDING_SPECS["delta_t"] = (arguments, foreign.DeltaT)
  with pytest.raises(RuntimeError, match="Unmeasured declared return record"):
    abi_layout.runtime_layout(module)
  module.BINDING_SPECS["delta_t"] = (arguments, original)
  abi_layout.runtime_layout(module)


@pytest.mark.parametrize("defect", ["restype", "argtypes", "foreign-return"])
def test_loaded_function_metadata_cannot_drift_from_its_declarations(defect):
  module = abi_layout.load_abi()
  library = ModuleType("library_fixture")
  for name, (arguments, result) in module.BINDING_SPECS.items():
    function = ModuleType(name)
    function.argtypes, function.restype = arguments, result
    setattr(library, name, function)
  abi_layout.runtime_layout(module, library=library)
  function = library.delta_t
  if defect == "restype":
    function.restype = module.JulianDay
  elif defect == "argtypes":
    function.argtypes = (ctypes.c_int32,)
  else:
    function.restype = type("DeltaT", (ctypes.Structure,), {"_fields_": module.DeltaT._fields_})
  with pytest.raises(RuntimeError, match="Loaded signature differs"):
    abi_layout.runtime_layout(module, library=library)


def test_independent_jieqi_exporter_declaration_against_compiled_c(compiled_layout):
  from toolbox import jieqi_table

  structs, layout = compiled_layout
  schema, measured = abi_layout.runtime_layout(jieqi_table)
  name = "JieqiMomentQuery"
  relevant = {key: value for key, value in layout.items() if key == name or key.startswith(name + ".")}
  assert not abi_layout.compare_layouts({name: structs[name]}, schema, relevant, measured=measured)


@pytest.mark.parametrize(
  "mutation",
  [
    lambda source: source + "\nChartHousesV1 = SunCoordinate\n",
    lambda source: source + "\nmatch SunCoordinate:\n  case ChartHousesV1:\n    pass\n",
    lambda source: source + "\nglobals()['ChartHousesV1'] = SunCoordinate\n",
    lambda source: source + "\nexec('ChartHousesV1 = SunCoordinate')\n",
    lambda source: source.replace(
      "class ChartBodyV1(ctypes.Structure):", "class ChartBodyV1(ctypes.Structure):\n  _pack_ = 1"
    ),
    lambda source: source.replace(
      "class ChartSnapshotV1(ctypes.Structure):", "class ChartSnapshotV1(ctypes.Structure):\n  _pack_ = 1"
    ),
    lambda source: (
      source + "\nBINDING_SPECS['delta_t'] = ((ctypes.c_double,), "
      "type('DeltaT', (ctypes.Structure,), {'_fields_': DeltaT._fields_}))\n"
    ),
  ],
)
def test_runtime_mirror_checks_actual_bound_classes_not_declared_schema(tmp_path, compiled_layout, mutation):
  structs, ground_truth = compiled_layout
  source = DECLARATIONS.read_text(encoding="utf-8")
  runtime_structs, measured = mirror(tmp_path, source)
  assert not abi_layout.compare_layouts(structs, runtime_structs, ground_truth, measured=measured)
  try:
    runtime_structs, measured = mirror(tmp_path, mutation(source))
  except (RuntimeError, AttributeError):
    pass
  else:
    assert abi_layout.compare_layouts(structs, runtime_structs, ground_truth, measured=measured)
  runtime_structs, measured = mirror(tmp_path, source)
  assert not abi_layout.compare_layouts(structs, runtime_structs, ground_truth, measured=measured)


@pytest.mark.parametrize(
  "old,new",
  [
    ('("target_code", ctypes.c_uint32)', '("target_code", ctypes.c_int32)'),
    ('("present_fields", ctypes.c_uint32)', '("present_fields", ctypes.c_int32)'),
    ('("valid", ctypes.c_bool)', '("valid", ctypes.c_uint8)'),
    ('("distance_au", ctypes.c_double),', ""),
    ('("distance_au", ctypes.c_double)', '("unknown", ctypes.c_double)'),
    ("ctypes.c_double * 12", "ctypes.c_double * 11"),
    ("ChartBodyV1 * 14", "ChartBodyV1 * 13"),
    ('("houses", ChartHousesV1)', '("houses", ChartBodyV1)'),
    ('("houses", ChartHousesV1)', '("houses", SunCoordinate)'),
    (
      '("lon", ctypes.c_double),\n    ("lat", ctypes.c_double)',
      '("lat", ctypes.c_double),\n    ("lon", ctypes.c_double)',
    ),
  ],
)
def test_declaration_faults_detect_same_width_types_order_arrays_and_nesting(tmp_path, compiled_layout, old, new):
  structs, ground_truth = compiled_layout
  source = DECLARATIONS.read_text(encoding="utf-8")
  assert old in source, "mutation anchor must exist"
  mutated, measured = mirror(tmp_path, source.replace(old, new, 1))
  assert abi_layout.compare_layouts(structs, mutated, ground_truth, measured=measured)
  pristine, measured = abi_layout.runtime_layout(abi_layout.load_abi())
  assert not abi_layout.compare_layouts(structs, pristine, ground_truth, measured=measured)


def test_statistics_record_set_and_array_stride_faults(compiled_layout):
  structs, ground_truth = compiled_layout
  parsed, measured = abi_layout.runtime_layout(abi_layout.load_abi())
  missing = copy.deepcopy(parsed)
  del missing["DeltaT"]
  assert abi_layout.compare_layouts(structs, missing, ground_truth, measured=measured)
  extra = copy.deepcopy(parsed)
  extra["Extra"] = [("value", "double")]
  assert abi_layout.compare_layouts(structs, extra, ground_truth, measured=measured)
  wrong_stride = dict(ground_truth, **{"ChartSnapshotV1.bodies.stride": 32})
  assert any(
    "bodies.stride" in finding
    for finding in abi_layout.compare_layouts(structs, parsed, wrong_stride, measured=measured)
  )


def test_equal_layout_record_substitution_still_breaks_identity(tmp_path):
  header = tmp_path / "celestial.h"
  header.write_text("#include <stdbool.h>\n#include <stdint.h>\n" + C_CONTROL, encoding="utf-8")
  structs = abi_layout.parse_c_structs(header)
  ground_truth = abi_layout.c_layout(structs, header, tmp_path)
  assert ground_truth is not None
  pristine, measured = mirror(tmp_path, PY_CONTROL)
  mutated, altered = mirror(tmp_path, PY_CONTROL.replace("_Body * 14", "_Alias * 14"))
  assert measured == altered
  assert not abi_layout.compare_layouts(structs, pristine, ground_truth, measured=measured)
  assert abi_layout.compare_layouts(structs, mutated, ground_truth, measured=altered)


@pytest.fixture
def native_verifier(monkeypatch):
  binding = abi_layout.load_abi()
  package = ModuleType("celestial_calendar")
  package._abi = binding
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


def test_legacy_binding_names_alias_the_real_owner_not_a_matching_copy(native_verifier, monkeypatch):
  native_verifier.verify_binding_compatibility()
  copied = abi_layout.load_abi()
  assert copied.JulianDay is not native_verifier._abi.JulianDay
  monkeypatch.setattr(native_verifier, "_binding", copied)
  with pytest.raises(AssertionError, match="compatibility declaration identity"):
    native_verifier.verify_binding_compatibility()


def test_future_declarations_do_not_expand_the_legacy_binding_names(native_verifier, monkeypatch):
  owner = native_verifier._abi
  binding = ModuleType("legacy_binding_fixture")
  for name in native_verifier.LEGACY_BINDING_NAMES:
    setattr(binding, name, getattr(owner, name))
  future = type("ChartSnapshotV2", (ctypes.Structure,), {"_fields_": [("value", ctypes.c_double)]})
  monkeypatch.setattr(owner, "ChartSnapshotV2", future, raising=False)
  monkeypatch.setitem(owner.STRUCT_TYPES, "ChartSnapshotV2", future)
  monkeypatch.setattr(native_verifier, "_binding", binding)
  assert not hasattr(binding, "ChartSnapshotV2")
  native_verifier.verify_binding_compatibility()


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
    "enum { CHART_TARGET_Ceres = 14 };",
    "enum { CHART_TARGET_Çeres = 14 };",
    "enum { CHART_TARGET_PLUTO\\u00c7 = 14 };",
    "enum { CHART_TARGET_PLUTO\\\nX = 14 };",
    "enum { MAX_CHART_BODIES = 14 };",
    "// open /*\nenum { CHART_TARGET_PLUTOX = 14 };\n/* close */",
    '#define OPEN "/*"\nenum { CHART_TARGET_PLUTOX = 14 };\n#define CLOSE "*/"',
    "#define CAT(a, b) a##b\nenum { CAT(CHART, _TARGET_PLUTOX) = 14 };",
    "#define CAT(a, b) a %:%: b\nenum { CAT(CHART, _TARGET_PLUTOX) = 14 };",
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


@pytest.mark.parametrize(
  "alias,primitive,export",
  [
    ("P_U32", ctypes.c_int32, "new_moons_in_year"),
    ("P_DOUBLE", ctypes.c_float, "new_moons_after_jde"),
    ("P_CHAR", ctypes.c_uint8, "get_jieqi_name"),
  ],
)
def test_pointer_pointees_cannot_define_their_own_expected_signature(
  native_verifier, monkeypatch, alias, primitive, export
):
  binding = native_verifier._binding
  original = getattr(binding, alias)
  wrong = ctypes.POINTER(primitive)
  assert wrong is not original
  expected = native_verifier.ctypes_types(export)
  pristine = dict(binding.BINDING_SPECS)
  monkeypatch.setattr(binding, alias, wrong)
  for name, (arguments, result) in tuple(binding.BINDING_SPECS.items()):
    if original in arguments:
      monkeypatch.setitem(
        binding.BINDING_SPECS, name, (tuple(wrong if kind is original else kind for kind in arguments), result)
      )
  spec = importlib.util.spec_from_file_location("pointer_identity_control", ABI / "verify.py")
  checker = importlib.util.module_from_spec(spec)
  old_path = list(sys.path)
  try:
    spec.loader.exec_module(checker)
  finally:
    sys.path[:] = old_path
  with pytest.raises(AssertionError, match="unsupported ctypes type"):
    checker.ctypes_types(export)
  assert wrong not in checker.CTYPE_NAMES and original in checker.CTYPE_NAMES
  binding.BINDING_SPECS.update(pristine)
  monkeypatch.setattr(binding, alias, original)
  assert checker.ctypes_types(export) == expected


def test_manifest_mutation_total_counts_the_cases_actually_checked(native_verifier, monkeypatch):
  manifest = json.loads((ABI / "manifest.json").read_text(encoding="utf-8"))
  checked = []

  def rejects(record, *arguments):
    checked.append(record)
    raise AssertionError("controlled defect rejection")

  monkeypatch.setattr(native_verifier, "verify_manifest", rejects)
  total = native_verifier.run_mutation_self_tests(manifest, {}, {}, set(), {})
  assert total == len(checked) == 19


def test_manifest_mutation_case_omission_cannot_lower_the_accepted_denominator(native_verifier):
  source = (ABI / "verify.py").read_text(encoding="utf-8")
  removed = '  mutations["missing export"] = missing_export'
  assert source.count(removed) == 1
  module = ModuleType("mutation_inventory_control")
  module.__file__ = str(ABI / "verify.py")
  exec(compile(source.replace(removed, ""), module.__file__, "exec"), module.__dict__)
  manifest = json.loads((ABI / "manifest.json").read_text(encoding="utf-8"))
  with pytest.raises(AssertionError, match="independent ABI defect-case count"):
    module.run_mutation_self_tests(manifest, {}, {}, set(), {})
