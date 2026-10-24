# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Declaration changes may regenerate mirrors, but cannot refresh the independent C baseline."""

import ctypes
import importlib.util
import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest

from automation.abi_layout import compare_layouts, runtime_layout
from automation.c_abi_schema import parse_header, parse_header_text, record_layouts
from automation.c_header_pins import verify_c_header_pins
from toolbox import make_abi_manifest, make_bindings

ROOT = Path(__file__).resolve().parents[2]
HEADER = Path("src/shared_lib/celestial.h")
PINS = Path("src/shared_lib/c_header_check.c")


def test_full_declaration_source_and_pointer_qualifiers():
  header = parse_header(ROOT / HEADER)
  assert len(header.structs) == 28 and len(header.functions) == 36 and len(header.constants) == 63
  selection = header.functions["celestial_positions"].parameters[0]
  assert selection.name == "target_codes" and selection.type.spelling() == "const uint32_t *"
  control = parse_header_text("CELESTIAL_API bool probe(const uint32_t * const input, double *output);")
  assert control.functions["probe"].parameters[0].type.pointer_const
  assert control.functions["probe"].signature() == "bool probe(const uint32_t * const input, double *output)"
  constants = parse_header_text("#define FIRST UINT32_C(0xFFFFFFFF)\n#define SECOND FIRST\n")
  assert constants.constants == {"FIRST": 0xFFFFFFFF, "SECOND": 0xFFFFFFFF}
  assert constants.aliases == {"SECOND": "FIRST"}


@pytest.mark.parametrize(
  "declaration",
  [
    "extern double global;",
    "typedef uint32_t Code;",
    "enum Code { FIRST = 0 };",
    "CELESTIAL_API double probe();",
    "CELESTIAL_API double probe(double);",
    "CELESTIAL_API double probe(float x);",
    "CELESTIAL_API double probe(double x, ...);",
    "CELESTIAL_API double probe(void (*callback)(void));",
    "CELESTIAL_API double probe(double x[2]);",
    "CELESTIAL_API double **probe(void);",
    "CELESTIAL_API double probe(volatile double *x);",
    "CELESTIAL_API double probe(double x, double x);",
    "CELESTIAL_API double probe(void) noexcept;",
    "CELESTIAL_API double probe(void) {}",
    "CELESTIAL_API Missing probe(void);",
    "#define FIRST (1 << 2)",
    "#define FIRST UINT32_C(4294967296)",
    "#define FIRST UINT32_C(-1)",
    "#define FIRST UINT32_C(01)",
    "#define FIRST UNKNOWN",
    "#define FIRST(x) x",
    "#if FUTURE\nCELESTIAL_API double future(void);\n#endif",
    "typedef struct Broken { bool valid;\n#include <stdint.h>\n} Broken;",
    "#include <other.h>",
    '"/*"; CELESTIAL_API double hidden(void); "*/";',
    "CELESTIAL_API double probe(void); stray",
  ],
)
def test_unconsumed_or_unsupported_declarations_fail(declaration):
  with pytest.raises(RuntimeError):
    parse_header_text(declaration + "\n")


def test_semantic_table_rejects_missing_stale_and_declaration_facts(monkeypatch):
  header = make_abi_manifest.declaration_source()
  semantics = make_abi_manifest.EXPORT_SEMANTICS
  with monkeypatch.context() as patch:
    patch.delitem(semantics, "celestial_delta_t")
    with pytest.raises(RuntimeError, match="missing="):
      make_abi_manifest.build_manifest("python", header)
  with monkeypatch.context() as patch:
    patch.setitem(semantics, "extra_export", semantics["celestial_delta_t"])
    with pytest.raises(RuntimeError, match="stale="):
      make_abi_manifest.build_manifest("python", header)
  with monkeypatch.context() as patch:
    patch.setitem(
      semantics["celestial_delta_t"], "signature", "CelestialDeltaT celestial_delta_t(double year, uint32_t model_code)"
    )
    with pytest.raises(RuntimeError, match="Unsupported export semantics"):
      make_abi_manifest.build_manifest("python", header)


@pytest.fixture
def isolated_generator(tmp_path, monkeypatch):
  for relative in (HEADER, PINS, make_bindings.PYTHON_PATH, make_bindings.JAVASCRIPT_PATH):
    path = tmp_path / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / relative, path)
  for relative in (*make_abi_manifest.MANIFEST_PATHS.values(), make_bindings.EXPORT_PATH):
    (tmp_path / relative).parent.mkdir(parents=True, exist_ok=True)
  monkeypatch.setattr(make_abi_manifest, "REPO_ROOT", tmp_path)
  monkeypatch.setattr(make_bindings, "ROOT", tmp_path)
  return tmp_path


def test_generation_check_covers_every_output_and_never_writes_pins(isolated_generator):
  root = isolated_generator
  original = (root / PINS).read_bytes()
  assert make_bindings.main([]) == 0
  assert make_bindings.main(["--check"]) == 0
  for relative, expected in make_bindings.generated_outputs().items():
    path = root / relative
    path.write_text(expected + "\n", encoding="utf-8")
    assert make_bindings.main(["--check"]) == 1
    path.write_text(expected, encoding="utf-8")
    assert make_bindings.main(["--check"]) == 0
  assert (root / PINS).read_bytes() == original


def test_header_checkout_line_endings_do_not_change_generated_outputs(isolated_generator):
  root = isolated_generator
  assert make_bindings.main([]) == 0
  path = root / HEADER
  path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
  assert make_bindings.main(["--check"]) == 0


def compile_pins(root: Path, label: str) -> subprocess.CompletedProcess:
  compiler = os.environ.get("CC", "clang")
  assert shutil.which(compiler), f"C11 baseline compiler is required: {compiler}"
  result = subprocess.run(
    [
      compiler,
      "-std=c11",
      "-Wall",
      "-Wextra",
      "-Werror",
      "-pedantic",
      "-fsyntax-only",
      str(root / PINS),
    ],
    capture_output=True,
    text=True,
    check=False,
  )
  (root / f"{label}-compile.log").write_text(
    f"command={result.args!r}\nexit={result.returncode}\n{result.stdout}{result.stderr}", encoding="utf-8"
  )
  return result


def test_actual_compiler_accepts_the_independent_baseline(tmp_path):
  shared = tmp_path / "src/shared_lib"
  shared.mkdir(parents=True)
  for relative in (HEADER, PINS):
    shutil.copyfile(ROOT / relative, tmp_path / relative)
  verify_c_header_pins(tmp_path)
  result = compile_pins(tmp_path, "positive")
  assert result.returncode == 0, result.stderr


MUTATIONS = [
  ("reorder", "uint32_t month;    /* The month. */\n  uint32_t day;", "uint32_t day;\n  uint32_t month;", True),
  ("legacy-same-width", "uint32_t month;    /* The month. */", "int32_t month;", True),
  ("padding-field", "uint8_t day;     /* The day of the lunar month. */", "uint8_t day;\n  uint8_t padding;", False),
  ("array-extent", "double cusps_deg[12];", "double cusps_deg[11];", True),
  (
    "array-stride",
    "double longitude_rate_deg_per_tt_day; /* Published seven-point signed rate",
    "double extra;\n  double longitude_rate_deg_per_tt_day; /* Published seven-point signed rate",
    True,
  ),
  ("array-element", "CelestialChartBody bodies[14];", "ChartBodyAlias bodies[14];", True),
  ("nested-record", "CelestialChartHouses houses;", "HouseAlias houses;", True),
  (
    "signature",
    "CelestialDeltaT celestial_delta_t(double year, uint32_t model_code)",
    "CelestialDeltaT celestial_delta_t(int32_t year, uint32_t model_code)",
    True,
  ),
  (
    "pointee-const",
    "celestial_positions(const uint32_t *target_codes",
    "celestial_positions(uint32_t *target_codes",
    True,
  ),
  (
    "equatorial-pointee-const",
    "celestial_equatorial_positions(const uint32_t *target_codes",
    "celestial_equatorial_positions(uint32_t *target_codes",
    True,
  ),
  ("code-value", "CELESTIAL_TARGET_MERCURY         UINT32_C(2)", "CELESTIAL_TARGET_MERCURY         UINT32_C(3)", True),
  (
    "default-alias",
    "CELESTIAL_DELTA_T_MODEL_DEFAULT UINT32_C(0)",
    "CELESTIAL_DELTA_T_MODEL_DEFAULT CELESTIAL_DELTA_T_MODEL_ALGO5",
    True,
  ),
]


@pytest.mark.parametrize("label,old,new,compiler_rejects", MUTATIONS, ids=[m[0] for m in MUTATIONS])
def test_colluding_generated_views_cannot_update_baseline(isolated_generator, label, old, new, compiler_rejects):
  root = isolated_generator
  positive = compile_pins(root, "positive")
  assert positive.returncode == 0, positive.stderr
  path = root / HEADER
  original = path.read_text(encoding="utf-8")
  assert original.count(old) >= 1, label
  changed = original.replace(old, new, 1)
  if label == "code-value":
    changed = changed.replace(
      "CELESTIAL_TARGET_VENUS           UINT32_C(3)", "CELESTIAL_TARGET_VENUS           UINT32_C(2)"
    )
  if label == "nested-record":
    start = changed.index("typedef struct CelestialChartHouses {")
    end = changed.index("} CelestialChartHouses;", start) + len("} CelestialChartHouses;")
    alias = changed[start:end].replace("CelestialChartHouses", "HouseAlias")
    changed = changed[:end] + "\n" + alias + changed[end:]
  # Keep each substituted type declared before its first use.
  if label == "array-element":
    start = changed.index("typedef struct CelestialChartBody {")
    end = changed.index("} CelestialChartBody;", start) + len("} CelestialChartBody;")
    alias = changed[start:end].replace("CelestialChartBody", "ChartBodyAlias")
    changed = changed[:end] + "\n" + alias + changed[end:]
  if label == "default-alias":
    definition = "#define CELESTIAL_DELTA_T_MODEL_ALGO5   UINT32_C(5)"
    changed = changed.replace(definition, "")
    changed = changed.replace(
      "#define CELESTIAL_DELTA_T_MODEL_DEFAULT", definition + "\n#define CELESTIAL_DELTA_T_MODEL_DEFAULT"
    )
  path.write_text(changed, encoding="utf-8")
  pin_bytes = (root / PINS).read_bytes()
  assert make_bindings.main([]) == 0
  assert make_bindings.main(["--check"]) == 0
  assert (root / PINS).read_bytes() == pin_bytes

  parsed = parse_header(path)
  spec = importlib.util.spec_from_file_location("generated_control", root / make_bindings.PYTHON_PATH)
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  fields, measured = runtime_layout(module)
  layouts = record_layouts(parsed.structs, "python")
  expected = {}
  for name, record in layouts.items():
    expected[name] = record["size"]
    expected[name + ".alignment"] = record["alignment"]
    for field in record["fields"]:
      expected[name + "." + field["name"]] = field["offset"]
  assert not compare_layouts(parsed.structs, fields, expected, measured=measured)
  manifest = json.loads((root / make_abi_manifest.MANIFEST_PATHS["python"]).read_text(encoding="utf-8"))
  assert manifest["layouts"] == layouts
  if label == "padding-field":
    assert ctypes.sizeof(module.CelestialLunarDate) == 12
    with pytest.raises(RuntimeError, match="field type/offset"):
      verify_c_header_pins(root)
  result = compile_pins(root, label)
  assert (result.returncode != 0) == compiler_rejects, result.stderr


def test_every_field_pin_is_required_and_stale_fields_fail(isolated_generator):
  root = isolated_generator
  parsed = parse_header(root / HEADER)
  path = root / PINS
  original = path.read_text(encoding="utf-8")
  for record, fields in parsed.structs.items():
    for name, kind in fields:
      macro = "ABI_ARRAY" if "[" in kind else "ABI_FIELD"
      line = next(line for line in original.splitlines() if line.startswith(f"{macro}({record}, {name},"))
      path.write_text(original.replace(line, ""), encoding="utf-8")
      with pytest.raises(RuntimeError, match="field type/offset|structs without a layout pin"):
        verify_c_header_pins(root)
      path.write_text(original.replace(line, line.replace(f", {name},", ", stale_field,")), encoding="utf-8")
      with pytest.raises(RuntimeError, match="field type/offset"):
        verify_c_header_pins(root)
  path.write_text(original, encoding="utf-8")
  verify_c_header_pins(root)


def test_every_code_pin_is_required_and_stale_codes_fail(isolated_generator):
  root = isolated_generator
  path = root / PINS
  original = path.read_text(encoding="utf-8")
  for name in parse_header(root / HEADER).constants:
    line = next(line for line in original.splitlines() if line.startswith(f"ABI_CODE({name},"))
    path.write_text(original.replace(line, ""), encoding="utf-8")
    with pytest.raises(RuntimeError, match="numeric code"):
      verify_c_header_pins(root)
    path.write_text(original.replace(line, line.replace(name, "STALE_CODE")), encoding="utf-8")
    with pytest.raises(RuntimeError, match="numeric code"):
      verify_c_header_pins(root)
  path.write_text(original, encoding="utf-8")
  verify_c_header_pins(root)


def test_macro_reordering_preserves_named_codes(isolated_generator):
  root = isolated_generator
  path = root / HEADER
  original = path.read_text(encoding="utf-8")
  first = "#define CELESTIAL_TARGET_MERCURY         UINT32_C(2)"
  second = "#define CELESTIAL_TARGET_VENUS           UINT32_C(3)"
  changed = original.replace(first + "\n" + second, second + "\n" + first)
  assert changed != original
  path.write_text(changed, encoding="utf-8")
  before = parse_header_text(original).constants
  assert parse_header(path).constants == before
  assert make_bindings.main([]) == make_bindings.main(["--check"]) == 0
  verify_c_header_pins(root)
  assert compile_pins(root, "reordered-codes").returncode == 0
