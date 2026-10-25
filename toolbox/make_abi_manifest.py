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

"""Render native64 and wasm32 ABI manifests from celestial.h and its protocol contracts."""

import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))
from automation.c_abi_schema import Header, parse_header, record_layouts, split_field_type  # noqa: E402
from automation.c_abi_semantics import EXPORT_SEMANTICS  # noqa: E402

VIEWS = {
  "python": {"schema": "celestial-calendar/python-native-abi@2", "width_key": "native_width_bits", "pointer": 64},
  "javascript": {"schema": "celestial-calendar/javascript-wasm-abi@2", "width_key": "wasm_width_bits", "pointer": 32},
}
MANIFEST_PATHS = {
  "python": Path("bindings/python/test/abi/manifest.json"),
  "javascript": Path("bindings/javascript/test/abi/manifest.json"),
}
PYTHON_ONLY_KEYS = frozenset({"ownership", "presence", "extent", "stride"})


def declaration_source() -> Header:
  header = parse_header(REPO_ROOT / "src/shared_lib/celestial.h")
  validate_semantics(header)
  return header


def validate_semantics(header: Header) -> None:
  """Every export needs one protocol row; declaration facts remain in the header."""
  missing = header.functions.keys() - EXPORT_SEMANTICS.keys()
  stale = EXPORT_SEMANTICS.keys() - header.functions.keys()
  if missing or stale:
    raise RuntimeError(f"Export semantics differ: missing={sorted(missing)}, stale={sorted(stale)}")
  for name, function in header.functions.items():
    if not function.exported:
      raise RuntimeError(f"Missing CELESTIAL_API: {name}")
    semantics = EXPORT_SEMANTICS[name]
    if set(semantics) != {"protocol", "recording"} or type(semantics["recording"]) is not bool:
      raise RuntimeError(f"Unsupported export semantics: {name}")
    protocol = semantics["protocol"]
    if protocol.keys() - {
      "kind",
      "error_reader",
      "lifetime",
      "ownership",
      "identity",
      "count_parameter",
      "slots_parameter",
      "capacity_parameter",
      "legal_empty",
      "total_must_match",
      "written_policy",
      "null_slots_requires_zero_capacity",
      "count_export",
      "buffer_parameter",
      "size_includes_nul",
      "null_requires_zero_capacity",
      "truncates",
      "terminates",
      "operations",
      "subject_policy",
      "nontarget_operations",
      "selection_policy",
    }:
      raise RuntimeError(f"Declaration facts do not belong in export semantics: {name}")
    if semantics["protocol"].get("kind") not in {
      None,
      "borrowed-string",
      "sized-string",
      "count-fill",
      "companion-count",
      "companion-fill",
      "requested-fill",
    }:
      raise RuntimeError(f"Unsupported protocol kind: {name}")
    parameters = {p.name for p in function.parameters}
    types = {p.name: p.type for p in function.parameters}
    for key, value in protocol.items():
      if key.endswith("_parameter") and value not in parameters:
        raise RuntimeError(f"Unknown protocol parameter: {name}.{value}")
    for key, spelling in (
      ("count_parameter", "uint32_t *"),
      ("capacity_parameter", "uint32_t"),
      ("buffer_parameter", "char *"),
    ):
      if key in protocol and types[protocol[key]].spelling() != spelling:
        raise RuntimeError(f"Incompatible protocol pointer role: {name}.{key}")
    if "slots_parameter" in protocol:
      slots = types[protocol["slots_parameter"]]
      if not slots.pointer or slots.const:
        raise RuntimeError(f"Incompatible protocol pointer role: {name}.slots_parameter")
    for key in ("operations", "nontarget_operations"):
      if set(protocol.get(key, [])) - header.constants.keys():
        raise RuntimeError(f"Unknown protocol operation: {name}.{key}")
    if "count_export" in protocol and protocol["count_export"] not in header.functions:
      raise RuntimeError(f"Unknown companion count export: {name}")
    if protocol.get("kind") == "count-fill":
      if type(protocol.get("legal_empty")) is not bool or protocol.get("total_must_match") is not True:
        raise RuntimeError(f"Missing count/fill policy: {name}")
      if (
        protocol.get("written_policy") != "min-total-capacity"
        or protocol.get("null_slots_requires_zero_capacity") is not True
      ):
        raise RuntimeError(f"Missing count/fill policy: {name}")
      if not {"count_parameter", "slots_parameter", "capacity_parameter"} <= protocol.keys():
        raise RuntimeError(f"Missing count/fill pointer roles: {name}")
    if protocol.get("kind") == "sized-string":
      if not {"buffer_parameter", "capacity_parameter"} <= protocol.keys():
        raise RuntimeError(f"Missing sized-string pointer roles: {name}")
      if any(
        protocol.get(key) is not True
        for key in ("size_includes_nul", "null_requires_zero_capacity", "truncates", "terminates")
      ):
        raise RuntimeError(f"Missing sized-string policy: {name}")


def _protocol_text(node: object, result: str, header: Header) -> object:
  """Resolve array extents mentioned by ownership prose from the result record."""
  if isinstance(node, dict):
    return {key: _protocol_text(value, result, header) for key, value in node.items()}
  if not isinstance(node, str):
    return node

  def extent(match: re.Match) -> str:
    record = result
    count = None
    for field in match[1].split("."):
      try:
        record, count = split_field_type(dict(header.structs[record])[field])
      except KeyError as error:
        raise RuntimeError(f"Unknown ownership field: {result}.{match[1]}") from error
    if count is None:
      raise RuntimeError(f"Ownership extent is not an array: {result}.{match[1]}")
    return str(count)

  return re.sub(r"\{([A-Za-z_][\w.]*)\.extent\}", extent, node)


def export_entries(header: Header) -> list[dict]:
  validate_semantics(header)
  entries = []
  for name, function in header.functions.items():
    semantics = EXPORT_SEMANTICS[name]
    kind = "struct" if function.result.base in header.structs and not function.result.pointer else "primitive"
    protocol = {
      "kind": kind,
      **_protocol_text(semantics["protocol"], function.result.base, header),
      "error_channel": "preserve" if not semantics["recording"] else "kind",
    }
    if function.result.pointer:
      if protocol["kind"] != "borrowed-string" or function.result.spelling() != "const char *":
        raise RuntimeError(f"Unsupported return-pointer ownership: {name}")
      result = {"kind": "borrowed-string", "pointer_width_bits": 64}
    elif kind == "struct":
      result = {"kind": kind, "layout": function.result.base}
    else:
      result = {"kind": kind, "type": function.result.spelling()}
    entries.append(
      {
        "name": name,
        "signature": function.signature(),
        "return": result,
        "protocol": protocol,
        "recording": semantics["recording"],
      }
    )
  return entries


def _resolve_views(node: object, view: str) -> object:
  if isinstance(node, dict):
    if set(node) == {"python", "javascript"}:
      return node[view]
    return {key: _resolve_views(value, view) for key, value in node.items()}
  if isinstance(node, list):
    return [_resolve_views(item, view) for item in node]
  return node


def _apply_view(node: object, view: str, *, pointer_width_bits: int) -> object:
  if isinstance(node, dict):
    out = {}
    for key, value in node.items():
      if view != "python" and key in PYTHON_ONLY_KEYS:
        continue
      if key == "kind" and value == "struct" and view != "python":
        out[key] = "sret"
      elif key == "pointer_width_bits":
        out[key] = pointer_width_bits
      else:
        out[key] = _apply_view(value, view, pointer_width_bits=pointer_width_bits)
    return out
  if isinstance(node, list):
    return [_apply_view(item, view, pointer_width_bits=pointer_width_bits) for item in node]
  return node


def build_manifest(view: str, header: Header | None = None) -> dict:
  header = declaration_source() if header is None else header
  spec = VIEWS[view]
  widths = {
    "bool": 8,
    "uint8_t": 8,
    "uint16_t": 16,
    "int32_t": 32,
    "uint32_t": 32,
    "pointer": spec["pointer"],
    "double": 64,
  }
  body = _apply_view(
    _resolve_views(
      {
        "layouts": record_layouts(header.structs, view),
        "codes": header.constants,
        "exports": export_entries(header),
      },
      view,
    ),
    view,
    pointer_width_bits=spec["pointer"],
  )
  return {"schema": spec["schema"], "source_header": "src/shared_lib/celestial.h", spec["width_key"]: widths, **body}


def _inline(node: dict) -> str:
  return (
    "{" + ", ".join(f"{json.dumps(key)}: {json.dumps(value, ensure_ascii=False)}" for key, value in node.items()) + "}"
  )


def dump(node: object, level: int, parent_key: str | None = None) -> str:
  indent, child = "  " * level, "  " * (level + 1)
  if isinstance(node, dict):
    if not node:
      return "{}"
    if all(not isinstance(value, (dict, list)) for value in node.values()) and (
      parent_key in {"return", "protocol"} or "offset" in node
    ):
      return _inline(node)
    items = [f"{child}{json.dumps(key)}: {dump(value, level + 1, key)}" for key, value in node.items()]
    return "{\n" + ",\n".join(items) + f"\n{indent}}}"
  if isinstance(node, list):
    if not node:
      return "[]"
    if all(isinstance(item, dict) and "name" in item and "offset" in item for item in node):
      return "[\n" + ",\n".join(f"{child}{_inline(item)}" for item in node) + f"\n{indent}]"
    return "[\n" + ",\n".join(f"{child}{dump(item, level + 1, parent_key)}" for item in node) + f"\n{indent}]"
  return json.dumps(node, ensure_ascii=False)


def render(view: str) -> str:
  return dump(build_manifest(view), 0) + "\n"
