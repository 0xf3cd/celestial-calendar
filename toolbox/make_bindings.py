#!/usr/bin/env python3
#
# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Generate private binding declarations and manifests; never update independent C pins."""

import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from toolbox.make_abi_manifest import (  # noqa: E402
  MANIFEST_PATHS,
  _dump,
  build_manifest,
  declaration_source,
  export_entries,
)
from c_abi_schema import C_TO_CTYPES, CType, Header, split_field_type  # noqa: E402
from c_abi_semantics import EXPORT_SEMANTICS  # noqa: E402

# Private compatibility names used by existing consumers; these specify spelling only.
POINTER_NAMES = {
  "uint32_t": "P_U32",
  "double": "P_DOUBLE",
  "char": "P_CHAR",
  "CelestialLongitudeEventRecord": "P_LONGITUDE_EVENT",
  "CelestialStationEventRecord": "P_STATION_EVENT",
  "CelestialLongitudeDifferenceEventRecord": "P_LONGITUDE_DIFFERENCE_EVENT",
  "CelestialCapabilityRecord": "P_CAPABILITY",
  "CelestialModelIdentityRecord": "P_MODEL_IDENTITY",
  "CelestialPositionRecord": "P_POSITION",
  "CelestialEquatorialPositionRecord": "P_EQUATORIAL_POSITION",
}
PYTHON_PATH = Path("bindings/python/src/celestial_calendar/_abi.py")
JAVASCRIPT_PATH = Path("bindings/javascript/src/bindings.mjs")
EXPORT_PATH = Path("bindings/javascript/src/wasm_exports.json")
BANNER = """CelestialCalendar:
  A C++23-style library that performs astronomical calculations and date conversions between
  Gregorian and Chinese Lunar calendars.

Copyright (C) 2026 Ningqi Wang (0xf3cd)
Email: nq.maigre@gmail.com
Repo : https://github.com/0xf3cd/celestial-calendar

SPDX-License-Identifier: MIT"""


def _python_type(kind: CType, *, result: bool = False) -> str:
  if kind.pointer:
    if result and kind.spelling() == "const char *":
      return "ctypes.c_char_p"
    return POINTER_NAMES.get(kind.base, f"ctypes.POINTER({_python_type(CType(kind.base))})")
  if kind.base == "void":
    return "None"
  return (
    f"ctypes.{C_TO_CTYPES[kind.base]}"
    if kind.base in C_TO_CTYPES
    else ("ctypes.c_char" if kind.base == "char" else kind.base)
  )


def _python_literal(value: object, level: int = 0) -> str:
  indent, child = "  " * level, "  " * (level + 1)
  if isinstance(value, dict):
    rows = [f"{child}{json.dumps(key)}: {_python_literal(item, level + 1)}," for key, item in value.items()]
    return "{\n" + "\n".join(rows) + "\n" + indent + "}" if rows else "{}"
  if isinstance(value, list):
    rows = [child + _python_literal(item, level + 1) + "," for item in value]
    return "[\n" + "\n".join(rows) + "\n" + indent + "]" if rows else "[]"
  return repr(value) if isinstance(value, bool) else json.dumps(value)


def render_python(header: Header) -> str:
  banner = "\n".join("# " + line if line else "#" for line in BANNER.splitlines())
  lines = [
    banner.rstrip(),
    "",
    '"""Private declarations. Regenerate with python3 toolbox/make_bindings.py."""',
    "",
    "from __future__ import annotations",
    "",
    "import ctypes",
  ]
  for name, fields in header.structs.items():
    lines += ["", "", f"class {name}(ctypes.Structure):", "  _fields_ = ["]
    for field_name, field_type in fields:
      base, extent = split_field_type(field_type)
      kind = _python_type(CType(base)) + (f" * {extent}" if extent else "")
      lines.append(f'    ("{field_name}", {kind}),')
    lines.append("  ]")
  lines += ["", ""]
  used = {p.type.base for f in header.functions.values() for p in f.parameters if p.type.pointer}
  for base, name in POINTER_NAMES.items():
    if base in used:
      lines.append(f"{name} = ctypes.POINTER({_python_type(CType(base))})")
  lines += ["", "BINDING_SPECS = {"]
  for name, function in header.functions.items():
    arguments = [_python_type(p.type) for p in function.parameters]
    result = _python_type(function.result, result=True)
    short = f'  "{name}": (({", ".join(arguments)}{"," if len(arguments) == 1 else ""}), {result}),'
    if len(short) <= 120:
      lines.append(short)
    else:
      lines += [f'  "{name}": (', "    (", *[f"      {arg}," for arg in arguments], "    ),", f"    {result},", "  ),"]
  lines += [
    "}",
    "",
    "STRUCT_TYPES = {",
    *[f'  "{name}": {name},' for name in header.structs],
    "}",
    "",
    "RECORDING_EXPORTS = frozenset(",
    "  (",
  ]
  lines += [f'    "{name}",' for name in header.functions if EXPORT_SEMANTICS[name]["recording"]]
  lines += [
    "  )",
    ")",
    "",
    "# ctypes erases pointer qualifiers; retain the complete C spelling for verification.",
    "C_SIGNATURES = {",
  ]
  for name, function in header.functions.items():
    parameters = [p.type.spelling() for p in function.parameters]
    lines += [f'  "{name}": (', f'    "{function.result.spelling()}",']
    if len(parameters) > 1:
      lines += ["    (", *[f'      "{p}",' for p in parameters], "    ),"]
    else:
      lines.append(f'    ("{parameters[0]}",),' if parameters else "    (),")
    lines.append("  ),")
  lines += ["}", "", "C_CODES = {", *[f'  "{name}": {value},' for name, value in header.constants.items()], "}"]
  protocols = {entry["name"]: entry["protocol"] for entry in export_entries(header)}
  lines += ["", "PROTOCOLS = " + _python_literal(protocols)]
  return "\n".join(lines) + "\n"


def _chart_codes(header: Header) -> dict:
  groups = {}
  for group, prefix in (
    ("scales", "CELESTIAL_CIVIL_SCALE_"),
    ("models", "CELESTIAL_DELTA_T_MODEL_"),
    ("systems", "CELESTIAL_HOUSE_SYSTEM_"),
    ("present", "CELESTIAL_PRESENT_"),
    ("targets", "CELESTIAL_TARGET_"),
  ):
    groups[group] = {}
    for name, value in header.constants.items():
      if name.startswith(prefix) and name != "CELESTIAL_TARGET_ALL":
        first, *rest = name[len(prefix) :].lower().split("_")
        groups[group][first + "".join(word.title() for word in rest)] = value
  return groups


def render_javascript(header: Header) -> str:
  banner = "/*\n" + "\n".join(" * " + line if line else " *" for line in BANNER.splitlines()) + "\n */"
  lines = [
    banner,
    "",
    "// Regenerate with python3 toolbox/make_bindings.py.",
    "const deepFreeze = (value) => {",
    "  for (const child of Object.values(value)) {",
    '    if (child !== null && typeof child === "object") deepFreeze(child);',
    "  }",
    "  return Object.freeze(value);",
    "};",
    "",
  ]
  manifest = build_manifest("javascript", header)
  bindings, functions, wasm = [], {}, {}
  for entry in manifest["exports"]:
    name = entry["name"]
    function = header.functions[name]
    is_record = entry["return"]["kind"] == "sret"
    result = (
      "sret:" + entry["return"]["layout"]
      if is_record
      else (
        entry["return"].get("type", entry["return"]["kind"])
        if entry["protocol"]["kind"] == "primitive"
        else entry["protocol"]["kind"]
      )
    )
    bindings.append({"cName": name, "wasmName": "_" + name, "result": result, "readsLastError": entry["recording"]})
    functions[name] = {
      "signature": function.signature(),
      "result": function.result.spelling(),
      "parameters": [{"name": p.name, "type": p.type.spelling()} for p in function.parameters],
    }

    def lowered(kind: CType) -> str:
      if kind.pointer:
        return "i32"
      if kind.base in {"double", "void"}:
        return "f64" if kind.base == "double" else "void"
      if kind.base not in C_TO_CTYPES:
        raise RuntimeError(f"Unsupported WASM value parameter: {kind.spelling()}")
      return "i32"

    wasm[name] = {
      "result": "void" if is_record else lowered(function.result),
      "parameters": (["i32"] if is_record else []) + [lowered(p.type) for p in function.parameters],
    }
  declarations = {
    "source_sha256": hashlib.sha256(
      (ROOT / "src/shared_lib/celestial.h").read_text(encoding="utf-8").encode("utf-8")
    ).hexdigest(),
    "functions": functions,
    "constants": header.constants,
    "aliases": header.aliases,
  }
  for name, value in (
    ("CHART_CODES", _chart_codes(header)),
    ("LAYOUTS", manifest["layouts"]),
    ("BINDINGS", bindings),
    ("C_DECLARATIONS", declarations),
    ("WASM_SIGNATURES", wasm),
    ("PROTOCOLS", {entry["name"]: entry["protocol"] for entry in manifest["exports"]}),
  ):
    lines += [f"export const {name} = deepFreeze({json.dumps(value, indent=2)});", ""]
  return "\n".join(lines)


def generated_outputs() -> dict[Path, str]:
  header = declaration_source()
  outputs = {
    PYTHON_PATH: render_python(header),
    JAVASCRIPT_PATH: render_javascript(header),
    EXPORT_PATH: json.dumps([*header.functions, "malloc", "free"], indent=2) + "\n",
  }
  outputs.update({path: _dump(build_manifest(view, header), 0) + "\n" for view, path in MANIFEST_PATHS.items()})
  return outputs


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--check", action="store_true", help="reject stale checked-in declarations")
  args = parser.parse_args(argv)
  failures = []
  for relative, content in generated_outputs().items():
    path = ROOT / relative
    if args.check:
      if not path.is_file() or path.read_text(encoding="utf-8") != content:
        failures.append(str(relative))
    else:
      path.write_text(content, encoding="utf-8")
      print(f"wrote {relative}")
  if failures:
    print("Stale binding declarations: " + ", ".join(failures), file=sys.stderr)
    print("Run python3 toolbox/make_bindings.py", file=sys.stderr)
  return int(bool(failures))


if __name__ == "__main__":
  sys.exit(main())
