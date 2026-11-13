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

"""Read the decimal coefficient initializers in the eight VSOP87D headers."""

import math
import re
import sys
from fractions import Fraction
from pathlib import Path


Term = tuple[Fraction, Fraction, Fraction]
Tables = dict[str, tuple[Term, ...]]

PLANETS = {
  "earth": "EAR",
  "mercury": "MER",
  "venus": "VEN",
  "mars": "MAR",
  "jupiter": "JUP",
  "saturn": "SAT",
  "uranus": "URA",
  "neptune": "NEP",
}

_COMMENTS = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
_SOURCE = re.compile(
  r'\s*#pragma\s+once\s*#include\s*<array>\s*#include\s*"defines\.hpp"\s*'
  r"namespace\s+astro::vsop87d::(?P<planet>[a-z]+)_coeff\s*\{(?P<body>.*?)"
  r"\}\s*namespace\s+astro::vsop87d\s*\{\s*template\s*<\s*>\s*"
  r"struct\s+PlanetTables\s*<\s*Planet::(?P<code>[A-Z]+)\s*>\s*\{(?P<exports>.*?)"
  r"\}\s*;\s*\}\s*",
  re.DOTALL,
)
_ALIAS = re.compile(r"using\s+(Coefficients|Vsop87dTable|Vsop87dTables)\s*=\s*astro::vsop87d::\1\s*;")
_RAW = re.compile(
  r"inline\s+constexpr\s+std::array\s*<\s*Coefficients\s*,\s*(0|[1-9][0-9]*)\s*>\s*"
  r"([LBR](?:0|[1-9][0-9]*))\s*\{\s*\{(.*?)\}\s*\}\s*;",
  re.DOTALL,
)
_MAPPING = re.compile(
  r"inline\s+constexpr\s+std::array\s*<\s*Vsop87dTable\s*,\s*(0|[1-9][0-9]*)\s*>\s*"
  r"([LBR]_(?:array|ARRAY))\s*\{([^{}]*)\}\s*;",
)
_FAMILY = re.compile(r"inline\s+constexpr\s+Vsop87dTables\s+([LBR])\s*\{\s*([LBR]_(?:array|ARRAY))\s*\}\s*;")
_NUMBER = r"[+-]?(?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)"
_ROW = re.compile(r"\{\s*(" + _NUMBER + r")\s*,\s*(" + _NUMBER + r")\s*,\s*(" + _NUMBER + r")\s*,?\s*\}")
_NAME_LIST = re.compile(r"\s*[LBR](?:0|[1-9][0-9]*)(?:\s*,\s*[LBR](?:0|[1-9][0-9]*))*\s*,?\s*")


def parse_coefficients(text: str) -> Tables:
  """Return power names and (amplitude, phase, frequency) tuples as exact binary64 rationals.

  Validate all initializers and L/B/R mappings before returning any tables.
  Raise ValueError for unsupported syntax, incomplete coverage, or inconsistent counts.
  """
  if (sys.float_info.radix, sys.float_info.mant_dig, sys.float_info.max_exp) != (2, 53, 1024):
    raise ValueError("Python float must use binary64")
  source = _SOURCE.fullmatch(_COMMENTS.sub(" ", text))
  if source is None:
    raise ValueError("Unsupported header envelope or PlanetTables specialization")
  planet = source["planet"]
  if planet not in PLANETS or source["code"] != PLANETS[planet]:
    raise ValueError("Unknown planet or mismatched PlanetTables specialization")
  exports = "".join(
    rf"\s*static\s+const\s+inline\s+Vsop87dTables\s*&\s*{family}\s*=\s*"
    rf"(?:vsop87d::)?{planet}_coeff::{family}\s*;"
    for family in "LBR"
  )
  if re.fullmatch(exports + r"\s*", source["exports"]) is None:
    raise ValueError("Unsupported or incomplete PlanetTables exports")

  body = re.sub(r"(?m)^\s*#pragma (?:region [LBR][0-5]-[LBR][0-5]|endregion)[ \t]*$", " ", source["body"])
  tables: Tables = {}
  aliases: set[str] = set()
  mappings: dict[str, tuple[str, ...]] = {}
  families: dict[str, str] = {}
  position = 0
  while position < len(body):
    if body[position].isspace():
      position += 1
      continue
    alias = _ALIAS.match(body, position)
    raw = _RAW.match(body, position)
    mapping = _MAPPING.match(body, position)
    family = _FAMILY.match(body, position)
    if alias is not None:
      name = alias[1]
      if name in aliases:
        raise ValueError(f"Duplicate alias {name}")
      aliases.add(name)
      position = alias.end()
    elif raw is not None:
      count, name, initializer = raw.groups()
      if name in tables:
        raise ValueError(f"Duplicate raw array {name}")
      terms: list[Term] = []
      row_position = 0
      while row_position < len(initializer):
        if initializer[row_position].isspace():
          row_position += 1
          continue
        row = _ROW.match(initializer, row_position)
        if row is None:
          raise ValueError(f"{name}: malformed row at initializer offset {row_position}")
        values = tuple(float(literal) for literal in row.groups())
        if not all(math.isfinite(value) for value in values):
          raise ValueError(f"{name}: non-finite binary64 coefficient")
        amplitude, phase, frequency = (Fraction.from_float(value) for value in values)
        terms.append((amplitude, phase, frequency))
        row_position = row.end()
        while row_position < len(initializer) and initializer[row_position].isspace():
          row_position += 1
        if row_position < len(initializer):
          if initializer[row_position] != ",":
            raise ValueError(f"{name}: expected row separator at initializer offset {row_position}")
          row_position += 1
      if len(terms) != int(count):
        raise ValueError(f"{name}: declared {count} rows, read {len(terms)}")
      tables[name] = tuple(terms)
      position = raw.end()
    elif mapping is not None:
      count, name, initializer = mapping.groups()
      if name in mappings:
        raise ValueError(f"Duplicate table mapping {name}")
      if _NAME_LIST.fullmatch(initializer) is None:
        raise ValueError(f"{name}: malformed table mapping")
      names = tuple(part.strip() for part in initializer.strip().rstrip(",").split(","))
      if len(names) != int(count):
        raise ValueError(f"{name}: declared {count} tables, read {len(names)}")
      expected = tuple(f"{name[0]}{power}" for power in range(len(names)))
      if names != expected:
        raise ValueError(f"{name}: mapping must list exactly {expected}")
      mappings[name] = names
      position = mapping.end()
    elif family is not None:
      name, mapping_name = family.groups()
      if name in families:
        raise ValueError(f"Duplicate coordinate family {name}")
      families[name] = mapping_name
      position = family.end()
    else:
      raise ValueError(f"Unconsumed namespace token at offset {position}: {body[position : position + 60]!r}")

  if aliases != {"Coefficients", "Vsop87dTable", "Vsop87dTables"}:
    raise ValueError("Missing expected type aliases")
  if set(families) != set("LBR") or len(mappings) != 3:
    raise ValueError("Missing expected L/B/R coordinate families or mappings")
  ordered_names: list[str] = []
  for name in "LBR":
    mapping_name = families[name]
    if mapping_name not in mappings or mapping_name[0] != name:
      raise ValueError(f"{name}: incorrect or missing table mapping {mapping_name}")
    ordered_names.extend(mappings[mapping_name])
  if set(ordered_names) != set(tables):
    raise ValueError("Raw arrays and coordinate mappings do not cover exactly the same tables")
  return {name: tables[name] for name in ordered_names}


def read_coefficients(path: Path) -> Tables:
  """Read one UTF-8 header; attach its path to any parsing error."""
  try:
    return parse_coefficients(path.read_text(encoding="utf-8"))
  except ValueError as error:
    raise ValueError(f"{path}: {error}") from error
