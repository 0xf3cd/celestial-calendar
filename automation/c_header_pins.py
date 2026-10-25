# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Reconcile independent C11 record, field, array, signature and numeric-code pin coverage."""

import re

from dataclasses import dataclass
from pathlib import Path
from typing import Final

from .c_abi_schema import parse_header, split_field_type, strip_comments

HEADER_RELATIVE: Final[Path] = Path("src/shared_lib/celestial.h")
PINS_RELATIVE: Final[Path] = Path("src/shared_lib/c_header_check.c")

_OFFSETOF_PATTERN: Final[re.Pattern[str]] = re.compile(r"offsetof\((\w+),")
_ALIGNOF_PATTERN: Final[re.Pattern[str]] = re.compile(r"_Alignof\((\w+)\)")
# The pin idiom is address-of-function-name: `_Generic(&name,`, with optional
# parentheses around the address and any whitespace, comments, or line splices
# between tokens. A field-address type check such as `&((Record*)0)->field` is
# not a signature pin; its type name must not be collected. The C compile is
# the backstop for pin styles outside this idiom.
_PINNED_EXPORT_PATTERN: Final[re.Pattern[str]] = re.compile(
  r"_Generic\s*\(\s*(?:\(\s*)*&\s*(?:\(\s*)*(\w+)\s*(?:\)\s*)*,"
)

_SPLICE_PATTERN: Final[re.Pattern[str]] = re.compile(r"\\\r?\n")


def _strip_comments(text: str) -> str:
  """Drop C comments and line splices so neither hides nor fakes a pin."""
  text = strip_comments(_SPLICE_PATTERN.sub("", text))
  return re.sub(r'"(?:\\.|[^"\\])*"', '""', text)


@dataclass(frozen=True)
class PinSets:
  structs: frozenset[str]
  exports: frozenset[str]
  layout_pins: frozenset[str]
  pinned_exports: frozenset[str]
  struct_only_pins: frozenset[str]
  records: frozenset[str]
  fields: frozenset[tuple[str, str]]
  field_pins: frozenset[tuple[str, str]]
  arrays: frozenset[tuple[str, str]]
  array_pins: frozenset[tuple[str, str]]
  codes: frozenset[str]
  code_pins: frozenset[str]


def collect_pin_sets(repo_root: Path) -> PinSets:
  header = parse_header(repo_root / HEADER_RELATIVE)
  pins = _strip_comments((repo_root / PINS_RELATIVE).read_text(encoding="utf-8"))
  pins = re.sub(r"^\s*#.*$", "", pins, flags=re.M)
  records = frozenset(re.findall(r"ABI_RECORD\(\s*(\w+)\s*,\s*[0-9]+\s*,\s*[0-9]+\s*\)", pins))
  fields = frozenset(re.findall(r"ABI_FIELD\(\s*(\w+)\s*,\s*(\w+)\s*,\s*\w+\s*,\s*[0-9]+\s*\)", pins))
  arrays = frozenset(
    re.findall(r"ABI_ARRAY\(\s*(\w+)\s*,\s*(\w+)\s*,\s*\w+\s*,\s*[0-9]+\s*,\s*[0-9]+\s*,\s*[0-9]+\s*\)", pins)
  )
  struct_pins = _OFFSETOF_PATTERN.findall(pins) + _ALIGNOF_PATTERN.findall(pins)
  return PinSets(
    structs=frozenset(header.structs),
    exports=frozenset(header.functions),
    layout_pins=frozenset(_OFFSETOF_PATTERN.findall(pins)) | {record for record, _ in fields | arrays},
    pinned_exports=frozenset(_PINNED_EXPORT_PATTERN.findall(pins)),
    struct_only_pins=frozenset(struct_pins) | records | {record for record, _ in fields | arrays},
    records=records,
    fields=frozenset((name, field) for name, members in header.structs.items() for field, _ in members),
    field_pins=fields | arrays,
    arrays=frozenset(
      (name, field)
      for name, members in header.structs.items()
      for field, kind in members
      if split_field_type(kind)[1] is not None
    ),
    array_pins=arrays,
    codes=frozenset(header.constants),
    code_pins=frozenset(re.findall(r"ABI_CODE\(\s*(\w+)\s*,\s*(?:[0-9]+|0x[0-9A-Fa-f]+)\s*\)", pins)),
  )


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def verify_c_header_pins(repo_root: Path) -> PinSets:
  sets = collect_pin_sets(repo_root)

  unpinned_structs = sets.structs - sets.layout_pins
  _require(not unpinned_structs, f"celestial.h structs without a layout pin: {sorted(unpinned_structs)}")
  stale_structs = sets.struct_only_pins - sets.structs
  _require(not stale_structs, f"c_header_check.c pins name no celestial.h struct: {sorted(stale_structs)}")

  unpinned_exports = sets.exports - sets.pinned_exports
  _require(not unpinned_exports, f"celestial.h exports without a signature pin: {sorted(unpinned_exports)}")
  stale_exports = sets.pinned_exports - sets.exports
  _require(not stale_exports, f"c_header_check.c pins name no celestial.h export: {sorted(stale_exports)}")

  for expected, actual, label in (
    (sets.structs, sets.records, "size/alignment"),
    (sets.fields, sets.field_pins, "field type/offset"),
    (sets.arrays, sets.array_pins, "array extent/stride"),
    (sets.codes, sets.code_pins, "numeric code"),
  ):
    missing, stale = expected - actual, actual - expected
    _require(not missing and not stale, f"{label} pins differ: missing={sorted(missing)}, stale={sorted(stale)}")

  return sets
