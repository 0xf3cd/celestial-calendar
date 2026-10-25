# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Fault controls for the shared grammar, generated ctypes records and ABI verifier."""

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
typedef struct Snapshot { bool valid; Body bodies[14]; double cusps[12]; Alias nested; } Snapshot;
"""
PY_CONTROL = """
import ctypes
class Body(ctypes.Structure):
  _fields_ = [("code", ctypes.c_uint32), ("longitude", ctypes.c_double)]
class Alias(ctypes.Structure):
  _fields_ = [("code", ctypes.c_uint32), ("longitude", ctypes.c_double)]
class Snapshot(ctypes.Structure):
  _fields_ = [("valid", ctypes.c_bool), ("bodies", Body * 14), ("cusps", ctypes.c_double * 12), ("nested", Alias)]
"""


def mirror(source):
  module = ModuleType("abi_fixture")
  exec(compile(source, "abi_fixture", "exec"), module.__dict__)
  return abi_layout.runtime_layout(module)


def test_closed_grammar_and_runtime_records():
  expected = {
    "Body": [("code", "uint32_t"), ("longitude", "double")],
    "Alias": [("code", "uint32_t"), ("longitude", "double")],
    "Snapshot": [("valid", "bool"), ("bodies", "Body[14]"), ("cusps", "double[12]"), ("nested", "Alias")],
  }
  assert parse_c_structs_text(C_CONTROL) == mirror(PY_CONTROL)[0] == expected


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
  with pytest.raises(RuntimeError):
    parse_c_structs_text(f"typedef struct Body {{ double longitude; {declaration} }} Body;")


@pytest.mark.parametrize(
  "header",
  [
    "typedef struct { double x; } Body;",
    "typedef struct Body { double x; } Other;",
    "typedef struct Body { } Body;",
    "typedef struct Body { Body self; } Body;",
    "typedef struct Body { Future nested; } Body;",
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
  structs = abi_layout.parse_c_structs(HEADER)
  measured = abi_layout.c_layout(structs, HEADER, tmp_path_factory.mktemp("native-layout"))
  assert measured is not None, "fresh C probe must compile and run"
  return structs, measured


def test_current_header_manifest_and_generated_records_agree_with_actual_compiler(compiled_layout):
  structs, ground = compiled_layout
  schema, measured = abi_layout.runtime_layout(abi_layout.load_abi())
  assert len(structs) == len(schema) == 28
  assert not abi_layout.compare_layouts(structs, schema, ground, measured=measured)
  manifest = json.loads((ABI / "manifest.json").read_text())
  for name, record in manifest["layouts"].items():
    assert record["size"] == ground[name] and record["alignment"] == ground[name + ".alignment"]
    for field in record["fields"]:
      key = name + "." + field["name"]
      assert field["offset"] == ground[key]
      if "extent" in field:
        assert field["extent"] == ground[key + ".extent"] and field["stride"] == ground[key + ".stride"]


def test_declarations_do_not_load_a_library(monkeypatch):
  monkeypatch.setattr(ctypes, "CDLL", lambda *args, **kwargs: pytest.fail("declarations loaded a native library"))
  assert len(abi_layout.load_abi().BINDING_SPECS) == 36


def test_foreign_return_record_is_not_accepted_as_a_matching_layout():
  module, foreign = abi_layout.load_abi(), abi_layout.load_abi()
  args, _ = module.BINDING_SPECS["celestial_delta_t"]
  module.BINDING_SPECS["celestial_delta_t"] = (args, foreign.CelestialDeltaT)
  with pytest.raises(RuntimeError, match="Unmeasured declared return record"):
    abi_layout.runtime_layout(module)


@pytest.mark.parametrize("defect", ["restype", "argtypes", "foreign-return"])
def test_loaded_metadata_cannot_drift_from_declarations(defect):
  module, library = abi_layout.load_abi(), ModuleType("library")
  for name, (args, result) in module.BINDING_SPECS.items():
    function = ModuleType(name)
    function.argtypes, function.restype = args, result
    setattr(library, name, function)
  function = library.celestial_delta_t
  if defect == "restype":
    function.restype = module.CelestialJulianDay
  elif defect == "argtypes":
    function.argtypes = (ctypes.c_int32, ctypes.c_uint32)
  else:
    function.restype = type("CelestialDeltaT", (ctypes.Structure,), {"_fields_": module.CelestialDeltaT._fields_})
  with pytest.raises(RuntimeError, match="Loaded signature differs"):
    abi_layout.runtime_layout(module, library=library)


@pytest.mark.parametrize(
  "old,new",
  [
    ('("target_code", ctypes.c_uint32)', '("target_code", ctypes.c_int32)'),
    ('("present_fields", ctypes.c_uint32)', '("present_fields", ctypes.c_int32)'),
    ('("valid", ctypes.c_bool)', '("valid", ctypes.c_uint8)'),
    ('("distance_au", ctypes.c_double),', ""),
    ('("distance_au", ctypes.c_double)', '("unknown", ctypes.c_double)'),
    ("ctypes.c_double * 12", "ctypes.c_double * 11"),
    ("CelestialChartBody * 14", "CelestialChartBody * 13"),
    ('("houses", CelestialChartHouses)', '("houses", CelestialChartBody)'),
    ('("houses", CelestialChartHouses)', '("houses", CelestialSunCoordinate)'),
    (
      '("lon", ctypes.c_double),\n    ("lat", ctypes.c_double)',
      '("lat", ctypes.c_double),\n    ("lon", ctypes.c_double)',
    ),
  ],
)
def test_generated_declaration_faults_are_detected(compiled_layout, old, new):
  structs, ground = compiled_layout
  source = DECLARATIONS.read_text()
  assert old in source
  mutated, measured = mirror(source.replace(old, new, 1))
  assert abi_layout.compare_layouts(structs, mutated, ground, measured=measured)


@pytest.mark.parametrize(
  "suffix",
  [
    "\nCelestialChartHouses = CelestialSunCoordinate\n",
    "\nglobals()['CelestialChartHouses'] = CelestialSunCoordinate\n",
    "\nexec('CelestialChartHouses = CelestialSunCoordinate')\n",
  ],
)
def test_runtime_measurement_reads_bound_classes(compiled_layout, suffix):
  structs, ground = compiled_layout
  with pytest.raises(RuntimeError):
    schema, measured = mirror(DECLARATIONS.read_text() + suffix)
    if abi_layout.compare_layouts(structs, schema, ground, measured=measured):
      raise RuntimeError("runtime layout differs")


def test_equal_storage_does_not_erase_nested_record_identity():
  pristine, measured = mirror(PY_CONTROL)
  changed, altered = mirror(PY_CONTROL.replace("Body * 14", "Alias * 14"))
  assert measured == altered and pristine != changed


@pytest.fixture
def native_verifier(monkeypatch):
  abi = abi_layout.load_abi()
  package = ModuleType("celestial_calendar")
  package._abi = package._binding = abi
  monkeypatch.setitem(sys.modules, "celestial_calendar", package)
  old_path = list(sys.path)
  spec = importlib.util.spec_from_file_location("native_abi_verifier", ABI / "verify.py")
  verifier = importlib.util.module_from_spec(spec)
  try:
    spec.loader.exec_module(verifier)
  finally:
    sys.path[:] = old_path
  return verifier


def test_declaration_seams_keep_owner_identity(native_verifier, monkeypatch):
  native_verifier.verify_binding_compatibility()
  for name in native_verifier.DECLARATION_SEAMS:
    changed = ModuleType("changed")
    for seam in native_verifier.DECLARATION_SEAMS:
      setattr(changed, seam, getattr(native_verifier._abi, seam))
    setattr(changed, name, object())
    monkeypatch.setattr(native_verifier, "_binding", changed)
    with pytest.raises(AssertionError, match="declaration owner identity"):
      native_verifier.verify_binding_compatibility()


def test_shared_parser_routes_and_exact_metadata(native_verifier):
  header = HEADER.read_text()
  exports = native_verifier.parse_header_exports(header)
  assert len(exports) == 36 and not export_surface.self_test_parser()
  assert export_surface.parse_declarations(header) == [(name, True) for name in exports]
  manifest = json.loads((ABI / "manifest.json").read_text())
  native_verifier.verify_record_layouts(manifest["layouts"], native_verifier.parse_header_layouts(header))
  native_verifier.verify_protocol_contracts(native_verifier._abi.PROTOCOLS)
  for entry in manifest["exports"]:
    assert native_verifier.canonical(entry["signature"]) == exports[entry["name"]]
    assert native_verifier.signature_types(entry["signature"]) == native_verifier.ctypes_types(entry["name"])


@pytest.mark.parametrize(
  "declaration",
  [
    "# define CELESTIAL_TARGET_CERES UINT32_C(14)",
    "enum { CELESTIAL_TARGET_CERES = 14 };",
    "const uint32_t CELESTIAL_TARGET_CERES = 14;",
    "#define CELESTIAL_TARGET_CERES INT32_C(14)",
    "enum { CELESTIAL_TARGET_Ceres = 14 };",
    "enum { CELESTIAL_TARGET_Çeres = 14 };",
    "enum { CELESTIAL_TARGET_PLUTO\\u00c7 = 14 };",
    "enum { CELESTIAL_TARGET_PLUTO\\\nX = 14 };",
    "enum { MAX_CELESTIAL_BODIES = 14 };",
    "// open /*\nenum { CELESTIAL_TARGET_PLUTOX = 14 };\n/* close */",
    '#define OPEN "/*"\nenum { CELESTIAL_TARGET_PLUTOX = 14 };\n#define CLOSE "*/"',
    "#define CAT(a, b) a##b\nenum { CAT(CELESTIAL, _TARGET_PLUTOX) = 14 };",
    "#define CAT(a, b) a %:%: b\nenum { CAT(CELESTIAL, _TARGET_PLUTOX) = 14 };",
  ],
)
def test_code_declarations_cannot_hide_from_shared_inventory(native_verifier, declaration):
  with pytest.raises(RuntimeError):
    native_verifier.parse_codes(HEADER.read_text() + "\n" + declaration + "\n")


@pytest.mark.parametrize("record", ["CelestialChartBody", "CelestialChartSnapshot"])
def test_packed_runtime_records_do_not_pass_storage_checks(compiled_layout, record):
  structs, ground = compiled_layout
  source = DECLARATIONS.read_text()
  anchor = f"class {record}(ctypes.Structure):"
  assert anchor in source
  schema, measured = mirror(source.replace(anchor, anchor + "\n  _pack_ = 1", 1))
  assert abi_layout.compare_layouts(structs, schema, ground, measured=measured)


@pytest.mark.parametrize(
  "alias,primitive,export",
  [
    ("P_U32", ctypes.c_int32, "celestial_new_moons_in_year"),
    ("P_DOUBLE", ctypes.c_float, "celestial_new_moons_after_jde"),
    ("P_CHAR", ctypes.c_uint8, "celestial_get_jieqi_name"),
  ],
)
def test_pointer_alias_mutation_cannot_redefine_expected_pointee(
  native_verifier, monkeypatch, alias, primitive, export
):
  binding = native_verifier._binding
  original, wrong = getattr(binding, alias), ctypes.POINTER(primitive)
  monkeypatch.setattr(binding, alias, wrong)
  args, result = binding.BINDING_SPECS[export]
  monkeypatch.setitem(binding.BINDING_SPECS, export, (tuple(wrong if k is original else k for k in args), result))
  with pytest.raises(AssertionError, match="unsupported ctypes type"):
    native_verifier.ctypes_types(export)


def test_mutation_inventory_remains_independent(native_verifier, monkeypatch):
  manifest = json.loads((ABI / "manifest.json").read_text())
  checked = []

  def rejects(changed, *args):
    checked.append(changed)
    raise AssertionError("controlled rejection")

  monkeypatch.setattr(native_verifier, "verify_manifest", rejects)
  assert native_verifier.run_mutation_self_tests(manifest, {}, {}, set(), {}) == len(checked) == 21


def test_verifier_actually_rejects_all_manifest_mutations(native_verifier, monkeypatch):
  verifier = native_verifier
  abi = verifier._abi
  library = ModuleType("fake_native")
  functions = {}
  for name, (args, result) in abi.BINDING_SPECS.items():
    function = ModuleType(name)
    function.argtypes, function.restype = args, result
    setattr(library, name, function)
    functions[name] = function
  monkeypatch.setattr(abi, "FUNCTIONS", functions, raising=False)
  monkeypatch.setattr(abi, "LIB", library, raising=False)
  text = HEADER.read_text()
  args = (
    verifier.parse_header_exports(text),
    verifier.parse_header_layouts(text),
    set(abi.RECORDING_EXPORTS),
    verifier.parse_codes(text),
  )
  manifest = json.loads((ABI / "manifest.json").read_text())
  verifier.verify_manifest(manifest, *args)
  assert verifier.run_mutation_self_tests(manifest, *args) == 21


def test_raw_protocol_inventory_requires_every_final_export(native_verifier):
  names = set(native_verifier.parse_header_exports(HEADER.read_text()))
  source = "\n".join(f"def {kind}_{name}(): pass" for name in names for kind in ("happy", "edge"))
  native_verifier.verify_raw_inventory(source, names)
  with pytest.raises(AssertionError, match="raw-protocol coverage"):
    native_verifier.verify_raw_inventory(source.replace("def happy_celestial_delta_t()", "def omitted()"), names)
