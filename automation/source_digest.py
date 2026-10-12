#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import ast
import hashlib
import json
import re

from pathlib import Path


RETAINED_REGISTRY = Path("src/test/provenance/batch-a-closeout/retained_host_blocks.json")
RETAINED_DATA_SHA256 = "07782553edbc70d9de2bb382c18291ed8adf0d0d052be4d35081fbf8a2ae76a8"
# Add new value-bearing type/member spellings here; implementation names and test macros stay free.
REFERENCE_VALUE_PATTERN = re.compile(
  r'"(?:\\.|[^"\\])*"|(?<![\w.])(?:0x[0-9a-fA-F]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?|\.\d+)'
  r"(?:[a-zA-Z_]\w*)?|\b(?:true|false)\b|[+*/-]|\b[A-Za-z_]\w*(?:::\w+)+"
  r"|\b(?:Angle|AngleDeg|AngleRad|DistanceKm|DistanceAu|GeoLocation|Datetime|Params|TraditionalMonth|RAD|DEG|AU|KM)\b"
  r"|\.(?:latitude|longitude|lat_deg|lon_deg|λ|β|α|δ|r|model|deg|Mp|A1|leap_month|angle_deg|"
  r"jd_ut1|jde_tt|ymd|fraction)\b"
)


def canonical_cpp(text: str) -> str:
  without_comments = re.sub(r"//[^\n]*|/\*.*?\*/", " ", text, flags=re.DOTALL)
  return " ".join(without_comments.split())


def retained_slice(text: str, start: str, end: str) -> str:
  """Retained data markers bind whole blocks; reference inputs/values markers bind value atoms."""
  if text.count(start) != 1:
    raise RuntimeError(f"retained data start is missing or ambiguous: {start}")
  begin = text.index(start)
  finish = text.find(end, begin + len(start))
  if finish < 0:
    raise RuntimeError(f"retained data end is missing: {end}")
  return text[begin:finish]


def retained_digest(path: Path, method: str, start: str = "", end: str = "", input_facts: tuple[str, ...] = ()) -> str:
  if method == "raw_sha256":
    data = path.read_bytes()
  elif method == "notebook_values":
    notebook = json.loads(path.read_text(encoding="utf-8"))
    # Execution counters and presentation metadata do not identify computed values.
    values = [
      {key: value for key, value in output.items() if key not in {"metadata", "execution_count"}}
      for cell in notebook["cells"]
      for output in cell.get("outputs", [])
    ]
    inputs = []
    for cell in notebook["cells"]:
      if cell["cell_type"] != "code":
        continue
      source = "".join(cell["source"])
      try:
        tree = ast.parse(source)
      except SyntaxError as error:
        if re.search(r"\bJieqi\s*\.", source):
          raise RuntimeError("retained notebook input-bearing cell cannot be parsed") from error
        continue
      for node in ast.walk(tree):
        if not isinstance(node, ast.List) or not node.elts:
          continue
        pairs = [
          item
          for item in node.elts
          if isinstance(item, ast.Call)
          and len(item.args) == 2
          and isinstance(item.args[0], ast.Constant)
          and type(item.args[0].value) is int
          and isinstance(item.args[1], ast.Attribute)
          and isinstance(item.args[1].value, ast.Name)
          and item.args[1].value.id == "Jieqi"
        ]
        if len(pairs) == len(node.elts):
          require_facts(source, input_facts, "notebook retained input source")
          inputs.append([(item.args[0].value, item.args[1].attr) for item in pairs])
    data = json.dumps(
      {"outputs": values, "directed_jieqi_inputs": inputs}, sort_keys=True, separators=(",", ":")
    ).encode()
  elif method in {"canonical_cpp", "cpp_reference_values"}:
    text = path.read_text(encoding="utf-8")
    retained = canonical_cpp(retained_slice(text, start, end))
    if method == "cpp_reference_values":
      # Printed examples carry values in calls; units and coordinate labels identify their inputs too.
      retained = " ".join(REFERENCE_VALUE_PATTERN.findall(retained))
    data = retained.encode()
  else:
    raise RuntimeError(f"unknown retained-digest method: {method}")
  return hashlib.sha256(data).hexdigest()


def json_digest(value: object) -> str:
  return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def verify_retained_data(repo_root: Path, ids: tuple[str, ...] | None = None) -> int:
  blocks = json.loads((repo_root / RETAINED_REGISTRY).read_text(encoding="utf-8"))["data_blocks"]
  if json_digest(blocks) != RETAINED_DATA_SHA256:
    raise RuntimeError("retained data identity record differs")
  selected = tuple(blocks) if ids is None else ids
  for block_id in selected:
    block = blocks[block_id]
    path = Path(block["path"])
    if path.is_absolute() or ".." in path.parts:
      raise RuntimeError(f"{block_id} retained path must be repository-relative")
    digest = retained_digest(
      repo_root / path,
      block["method"],
      block.get("start", ""),
      block.get("end", ""),
      tuple(block.get("input_facts", ())),
    )
    if digest != block["sha256"]:
      raise RuntimeError(f"{block_id} retained data hash differs: {digest}")
  return len(selected)


def require_facts(text: str, facts: tuple[str, ...] | list[str], label: str) -> None:
  normalized = " ".join(re.sub(r"(?m)^\s*(?://|#|\*)\s?", "", text).split())
  for fact in facts:
    if fact not in normalized:
      raise RuntimeError(f"{label} required fact is missing: {fact}")


def attribution_digest(text: str) -> str:
  """Identify cited URLs, literal identities and quantities, independently of project prose."""
  atoms = re.findall(
    r"https?://[^\s`)>]+|`[^`]+`|(?<![\w])(?:[0-9a-f]{40,64}|\d+(?:[.,]\d+)*(?:[eE][+-]?\d+)?)(?![\w])",
    text,
  )
  return json_digest(sorted(atoms))
