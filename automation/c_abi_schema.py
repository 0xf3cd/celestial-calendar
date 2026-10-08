# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Closed C record grammar shared by the native ABI acceptance and mirror gates."""

import re
from pathlib import Path
from typing import Final


C_TO_CTYPES: Final[dict[str, str]] = {
  "bool": "c_bool",
  "uint8_t": "c_uint8",
  "uint16_t": "c_uint16",
  "uint32_t": "c_uint32",
  "int32_t": "c_int32",
  "double": "c_double",
}

Fields = list[tuple[str, str]]
STRUCT_RE = re.compile(r"typedef\s+struct\s+([A-Za-z_]\w*)\s*\{([^{}]*)\}\s*\1\s*;", re.DOTALL)
FIELD_RE = re.compile(r"([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*(?:\[\s*([1-9][0-9]*)\s*\])?")


def split_field_type(value: str) -> tuple[str, int | None]:
  """Split a canonical scalar/record type or one positive decimal-literal array."""
  match = re.fullmatch(r"([A-Za-z_]\w*)(?:\[([1-9][0-9]*)\])?", value)
  if match is None:
    raise RuntimeError(f"Unsupported ABI field type: {value!r}")
  base, extent = match.groups()
  return base, None if extent is None else int(extent)


def parse_c_structs_text(header: str) -> dict[str, Fields]:
  """Read every named typedef record; no unknown member or declaration may disappear."""
  header = re.sub(r"/\*.*?\*/|//[^\n]*", " ", header, flags=re.DOTALL)
  structs: dict[str, Fields] = {}
  for start in re.finditer(r"\btypedef\s+struct\b", header):
    match = STRUCT_RE.match(header, start.start())
    if match is None:
      raise RuntimeError(f"Unsupported typedef struct near {header[start.start() : start.start() + 80]!r}")
    name, body = match.groups()
    if name in structs or name in C_TO_CTYPES:
      raise RuntimeError(f"Duplicate or reserved struct name: {name}")
    declarations = body.strip().split(";")
    if declarations[-1].strip():
      raise RuntimeError(f"Unterminated field declaration in {name}")

    fields: Fields = []
    seen: set[str] = set()
    for declaration in declarations[:-1]:
      field = FIELD_RE.fullmatch(declaration.strip())
      if field is None:
        raise RuntimeError(f"Unsupported field declaration in {name}: {declaration.strip()!r}")
      base, field_name, extent = field.groups()
      if base not in C_TO_CTYPES and base not in structs:
        raise RuntimeError(f"Unknown or not previously declared field type in {name}.{field_name}: {base}")
      if field_name in seen:
        raise RuntimeError(f"Duplicate field in {name}: {field_name}")
      seen.add(field_name)
      fields.append((field_name, base if extent is None else f"{base}[{extent}]"))
    if not fields:
      raise RuntimeError(f"No fields parsed from struct {name}")
    structs[name] = fields

  if not structs:
    raise RuntimeError("No typedef struct found")
  return structs


def parse_c_structs(header: Path) -> dict[str, Fields]:
  """Parse the published header without compiling or loading a native library."""
  return parse_c_structs_text(header.read_text(encoding="utf-8"))
