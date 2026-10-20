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
would turn no gate red (the observation-pool #16 shape). This module reconciles the sets in
both directions: unpinned header entities and stale pins both fail.
"""

import re

from dataclasses import dataclass
from pathlib import Path
from typing import Final

HEADER_RELATIVE: Final[Path] = Path("src/shared_lib/celestial.h")
PINS_RELATIVE: Final[Path] = Path("src/shared_lib/c_header_check.c")

_STRUCT_PATTERN: Final[re.Pattern[str]] = re.compile(r"^typedef struct (\w+) \{", re.M)
_EXPORT_PATTERN: Final[re.Pattern[str]] = re.compile(r"^CELESTIAL_API [\w *]+?(\w+)\(", re.M)
_OFFSETOF_PATTERN: Final[re.Pattern[str]] = re.compile(r"offsetof\((\w+),")
_ALIGNOF_PATTERN: Final[re.Pattern[str]] = re.compile(r"_Alignof\((\w+)\)")
_SIZEOF_PATTERN: Final[re.Pattern[str]] = re.compile(r"sizeof\((\w+)\)")
_PINNED_EXPORT_PATTERN: Final[re.Pattern[str]] = re.compile(r"_Generic\(&(\w+),")


@dataclass(frozen=True)
class PinSets:
  structs: frozenset[str]
  exports: frozenset[str]
  pinned_structs: frozenset[str]
  pinned_exports: frozenset[str]
  struct_only_pins: frozenset[str]


def collect_pin_sets(repo_root: Path) -> PinSets:
  header = (repo_root / HEADER_RELATIVE).read_text(encoding="utf-8")
  pins = (repo_root / PINS_RELATIVE).read_text(encoding="utf-8")
  # offsetof and _Alignof name structs by C semantics; sizeof also names builtins (e.g.
  # sizeof(double) sanity asserts), so it evidences a pin but never a stale one.
  struct_pins = _OFFSETOF_PATTERN.findall(pins) + _ALIGNOF_PATTERN.findall(pins)
  return PinSets(
    structs=frozenset(_STRUCT_PATTERN.findall(header)),
    exports=frozenset(_EXPORT_PATTERN.findall(header)),
    pinned_structs=frozenset(struct_pins) | frozenset(_SIZEOF_PATTERN.findall(pins)),
    pinned_exports=frozenset(_PINNED_EXPORT_PATTERN.findall(pins)),
    struct_only_pins=frozenset(struct_pins),
  )


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def verify_c_header_pins(repo_root: Path) -> PinSets:
  sets = collect_pin_sets(repo_root)

  unpinned_structs = sets.structs - sets.pinned_structs
  _require(not unpinned_structs, f"celestial.h structs without a layout pin: {sorted(unpinned_structs)}")
  stale_structs = sets.struct_only_pins - sets.structs
  _require(not stale_structs, f"c_header_check.c pins name no celestial.h struct: {sorted(stale_structs)}")

  unpinned_exports = sets.exports - sets.pinned_exports
  _require(not unpinned_exports, f"celestial.h exports without a signature pin: {sorted(unpinned_exports)}")
  stale_exports = sets.pinned_exports - sets.exports
  _require(not stale_exports, f"c_header_check.c pins name no celestial.h export: {sorted(stale_exports)}")

  return sets
