# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""The C ABI layout pins in c_header_check.c must name every celestial.h struct and export.

c_header_check.c compiles only what it mentions, so a new struct or export without a pin
would turn no gate red. This module reconciles the sets in both directions, after stripping
C comments so a commented-out pin does not count:

- layout coverage: every celestial.h struct carries an offsetof pin (a sizeof pin alone
  does not pin field order, and _Alignof pins alignment only);
- stale layout pins: every offsetof/_Alignof name is a celestial.h struct (stale sizeof
  names are left to the C compiler, which rejects the undeclared identifier);
- signature coverage and stale pins: every celestial.h export carries an active _Generic
  pin, and every _Generic pin names a celestial.h export. "Active" means the
  address-of-function-name idiom (`&name`, optionally parenthesized, with whitespace,
  comments, or line splices between tokens); a pin written in any other shape is not
  recognized, so its export fails coverage loudly instead of passing silently, and the
  C compile backstops anything the idiom does not cover.
"""

import re

from dataclasses import dataclass
from pathlib import Path
from typing import Final

HEADER_RELATIVE: Final[Path] = Path("src/shared_lib/celestial.h")
PINS_RELATIVE: Final[Path] = Path("src/shared_lib/c_header_check.c")

_STRUCT_PATTERN: Final[re.Pattern[str]] = re.compile(r"^\s*typedef struct (\w+) \{", re.M)
_EXPORT_PATTERN: Final[re.Pattern[str]] = re.compile(r"^\s*CELESTIAL_API [\w *]+?(\w+)\(", re.M)
_OFFSETOF_PATTERN: Final[re.Pattern[str]] = re.compile(r"offsetof\((\w+),")
_ALIGNOF_PATTERN: Final[re.Pattern[str]] = re.compile(r"_Alignof\((\w+)\)")
_SIZEOF_PATTERN: Final[re.Pattern[str]] = re.compile(r"sizeof\((\w+)\)")
# The pin idiom is address-of-function-name: `_Generic(&name,`, with optional
# parentheses around the address and any whitespace, comments, or line splices
# between tokens. A field-address type check such as `&((Record*)0)->field` is
# not a signature pin; its type name must not be collected. The C compile is
# the backstop for pin styles outside this idiom.
_PINNED_EXPORT_PATTERN: Final[re.Pattern[str]] = re.compile(
  r"_Generic\s*\(\s*(?:\(\s*)*&\s*(?:\(\s*)*(\w+)\s*(?:\)\s*)*,"
)

_COMMENT_PATTERN: Final[re.Pattern[str]] = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
_SPLICE_PATTERN: Final[re.Pattern[str]] = re.compile(r"\\\r?\n")


def _strip_comments(text: str) -> str:
  """Drop C comments and line splices so neither hides nor fakes a pin."""
  return _COMMENT_PATTERN.sub("", _SPLICE_PATTERN.sub("", text))


@dataclass(frozen=True)
class PinSets:
  structs: frozenset[str]
  exports: frozenset[str]
  layout_pins: frozenset[str]
  pinned_exports: frozenset[str]
  struct_only_pins: frozenset[str]


def collect_pin_sets(repo_root: Path) -> PinSets:
  header = _strip_comments((repo_root / HEADER_RELATIVE).read_text(encoding="utf-8"))
  pins = _strip_comments((repo_root / PINS_RELATIVE).read_text(encoding="utf-8"))
  struct_pins = _OFFSETOF_PATTERN.findall(pins) + _ALIGNOF_PATTERN.findall(pins)
  return PinSets(
    structs=frozenset(_STRUCT_PATTERN.findall(header)),
    exports=frozenset(_EXPORT_PATTERN.findall(header)),
    layout_pins=frozenset(_OFFSETOF_PATTERN.findall(pins)),
    pinned_exports=frozenset(_PINNED_EXPORT_PATTERN.findall(pins)),
    struct_only_pins=frozenset(struct_pins),
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

  return sets
