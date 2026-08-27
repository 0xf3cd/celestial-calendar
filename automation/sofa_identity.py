#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

import hashlib
import re

from dataclasses import dataclass
from decimal import Decimal
from pathlib import Path
from typing import Final


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
SOFA_ROOT: Final[Path] = REPO_ROOT / "src" / "test" / "provenance" / "sofa" / "2023-10-11"
SOFA_ARCHIVE_URL: Final[str] = "https://www.iausofa.org/s/sofa_c-20231011tar.gz"
SOFA_ARCHIVE_SHA256: Final[str] = "d9c10833cae8b4d9361a0ffda31ec361fd1262362025bec4d4e51a880150ace2"


@dataclass(frozen=True)
class PinnedInput:
  path: Path
  sha256: str


@dataclass(frozen=True)
class IdentityCounts:
  leap_rows: int
  lunar_rows: int
  iau1980_rows: int
  meeus_rows: int
  meeus_zeroed_obliquity_rows: int


SOFA_INPUTS: Final[tuple[PinnedInput, ...]] = (
  PinnedInput(Path("00READ.ME"), "553a3c1a246c4e8b49ab5fb0ebaf3dfa0fdb99673aa479c911df511c6afa7ae7"),
  PinnedInput(Path("src/dat.c"), "c269b897cba4f204af65fb874ec7769fcf38b284735d9a6b76c03c51a462fab8"),
  PinnedInput(Path("src/moon98.c"), "a28c822bc68d3115bc0649469df4e0ea12dddbf8426f3c5df9decb467d29dcfe"),
  PinnedInput(Path("src/nut80.c"), "e85eb97a1c19b3071ccb1f6b6f8b1f9da7ad47e665472a8c290875bcf358863e"),
  PinnedInput(Path("doc/copyr.lis"), "ffe5460c057a4765e6ca7cf30b50e9f1306e84640e8ec9c05566bbad2c96c994"),
)

# Meeus orders equal-amplitude rows differently from nut80.c. This fixed mapping preserves the
# repository's summation order while tying every row to its one-based SOFA source row.
MEEUS_SOURCE_INDICES: Final[tuple[int, ...]] = (
  1,
  9,
  31,
  2,
  10,
  32,
  11,
  33,
  34,
  12,
  35,
  13,
  36,
  37,
  38,
  40,
  39,
  41,
  14,
  3,
  42,
  45,
  43,
  44,
  46,
  15,
  47,
  16,
  48,
  18,
  17,
  49,
  19,
  4,
  50,
  54,
  52,
  51,
  53,
  58,
  55,
  56,
  59,
  20,
  57,
  61,
  21,
  60,
  62,
  22,
  23,
  64,
  24,
  63,
  65,
  67,
  5,
  6,
  66,
  68,
  69,
  71,
  72,
)

Row = tuple[Decimal, ...]
NUTATION_ROW: Final[str] = (
  r"\{\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*"
  r"(-?\d+(?:\.\d+)?),\s*(-?\d+(?:\.\d+)?),\s*"
  r"(-?\d+(?:\.\d+)?),\s*(-?\d+(?:\.\d+)?)\s*\}"
)
REPOSITORY_NUTATION_ROW: Final[str] = (
  r"\{\s*\{\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\s*\},\s*"
  r"\{\s*(-?\d+(?:\.\d+)?),\s*(-?\d+(?:\.\d+)?)\s*\},\s*"
  r"\{\s*(-?\d+(?:\.\d+)?),\s*(-?\d+(?:\.\d+)?)\s*\}\s*\}"
)


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _block(text: str, start: str, end: str, label: str) -> str:
  start_at = text.find(start)
  _require(start_at >= 0, f"{label}: start marker not found")
  end_at = text.find(end, start_at)
  _require(end_at >= 0, f"{label}: end marker not found")
  return text[start_at : end_at + len(end)]


def _rows(pattern: str, text: str, label: str) -> list[Row]:
  rows = [tuple(Decimal(value) for value in match) for match in re.findall(pattern, text)]
  _require(bool(rows), f"{label}: no rows parsed")
  return rows


def _require_rows_equal(label: str, source: list[Row], target: list[Row]) -> None:
  _require(len(source) == len(target), f"{label}: source rows={len(source)}, target rows={len(target)}")
  for index, (source_row, target_row) in enumerate(zip(source, target, strict=True), 1):
    _require(
      source_row == target_row,
      f"{label} row {index} differs: source={source_row}, target={target_row}",
    )


def _read_pinned_inputs(sofa_root: Path) -> dict[Path, str]:
  inputs: dict[Path, str] = {}
  for source in SOFA_INPUTS:
    data = (sofa_root / source.path).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    _require(digest == source.sha256, f"SOFA input hash mismatch for {source.path}: {digest}")
    inputs[source.path] = data.decode("ascii")
  return inputs


def verify_sofa_identities(
  repo_root: Path = REPO_ROOT,
  sofa_root: Path = SOFA_ROOT,
) -> IdentityCounts:
  inputs = _read_pinned_inputs(sofa_root)
  readme = inputs[Path("00READ.ME")]
  _require("SOFA-Issue: 2023-10-11" in readme, "SOFA issue marker differs")
  _require("issued on 2023-10-11" in readme, "SOFA issue date differs")

  for path in (Path("src/dat.c"), Path("src/moon98.c"), Path("src/nut80.c")):
    _require("SOFA release 2023-10-11" in inputs[path], f"{path}: SOFA release marker differs")

  dat_text = inputs[Path("src/dat.c")]
  changes = _rows(
    r"\{\s*(\d+),\s*(\d+),\s*(-?\d+(?:\.\d+)?)\s*\}",
    _block(dat_text, "} changes[] = {", "   };", "dat.c changes"),
    "dat.c changes",
  )
  _require(len(changes) == 42, f"dat.c changes rows={len(changes)}, expected 42")
  modern_changes = [row for row in changes if row[:2] >= (Decimal(1972), Decimal(1))]
  leap_text = (repo_root / "src" / "astro" / "leap_second.hpp").read_text(encoding="utf-8")
  leap_rows = _rows(
    r"\{\s*util::to_ymd\((\d+),\s*(\d+),\s*1\),\s*(-?\d+(?:\.\d+)?)\s*\}",
    _block(leap_text, "LEAP_SECOND_TABLE {{", "}};", "LEAP_SECOND_TABLE"),
    "LEAP_SECOND_TABLE",
  )
  _require_rows_equal("leap-second", modern_changes, leap_rows)

  moon98_text = inputs[Path("src/moon98.c")]
  source_lr = _rows(
    r"\{\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*"
    r"(-?\d+(?:\.\d+)?),\s*(-?\d+(?:\.\d+)?)\s*\}",
    _block(moon98_text, "static struct termlr tlr[] = {", "}};", "moon98.c tlr"),
    "moon98.c tlr",
  )
  source_b = _rows(
    r"\{\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+(?:\.\d+)?)\s*\}",
    _block(moon98_text, "static struct termb tb[] = {", "}};", "moon98.c tb"),
    "moon98.c tb",
  )
  lunar_text = (repo_root / "src" / "astro" / "elp2000_82b.hpp").read_text(encoding="utf-8")
  target_lr = _rows(
    r"\{\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\s*\}",
    _block(lunar_text, "std::array<LRCoefficients, 60> LR {{", "}};", "LR"),
    "LR",
  )
  target_b = _rows(
    r"\{\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\s*\}",
    _block(lunar_text, "std::array<BCoefficients, 60> B {{", "}};", "B"),
    "B",
  )
  million = Decimal(1_000_000)
  transformed_lr = [(*row[:4], row[4] * million, row[5]) for row in source_lr]
  transformed_b = [(*row[:4], row[4] * million) for row in source_b]
  _require_rows_equal("lunar LR", transformed_lr, target_lr)
  _require_rows_equal("lunar B", transformed_b, target_b)

  nut80_text = inputs[Path("src/nut80.c")]
  source_nutation = _rows(
    NUTATION_ROW,
    _block(nut80_text, "   } x[] = {", "   };", "nut80.c x"),
    "nut80.c x",
  )
  _require(len(source_nutation) == 106, f"nut80.c rows={len(source_nutation)}, expected 106")
  earth_text = (repo_root / "src" / "astro" / "earth.hpp").read_text(encoding="utf-8")
  target_iau1980 = _rows(
    REPOSITORY_NUTATION_ROW,
    _block(
      earth_text,
      "std::array<NutationCoeffs, 106> IAU1980_NUTATION_COEFFS {{",
      "}};",
      "IAU1980_NUTATION_COEFFS",
    ),
    "IAU1980_NUTATION_COEFFS",
  )
  transformed_iau1980 = [
    (nd, nlp, nl, nf, nom, sp, spt, ce, cet) for nl, nlp, nf, nd, nom, sp, spt, ce, cet in source_nutation
  ]
  _require_rows_equal("IAU 1980 nutation", transformed_iau1980, target_iau1980)

  target_meeus = _rows(
    REPOSITORY_NUTATION_ROW,
    _block(
      earth_text,
      "std::array<NutationCoeffs, 63> MEEUS_NUTATION_COEFFS {{",
      "}};",
      "MEEUS_NUTATION_COEFFS",
    ),
    "MEEUS_NUTATION_COEFFS",
  )
  selected_indices = {index for index, row in enumerate(source_nutation, 1) if abs(row[5]) >= Decimal(3)}
  _require(len(MEEUS_SOURCE_INDICES) == len(set(MEEUS_SOURCE_INDICES)), "Meeus source mapping contains duplicates")
  _require(set(MEEUS_SOURCE_INDICES) == selected_indices, "Meeus source mapping is not the nut80.c abs(sp)>=3 subset")
  transformed_meeus: list[Row] = []
  zeroed_obliquity_rows = 0
  for source_index in MEEUS_SOURCE_INDICES:
    nl, nlp, nf, nd, nom, sp, spt, ce, cet = source_nutation[source_index - 1]
    transformed_ce = ce if abs(ce) >= Decimal(3) else Decimal(0)
    zeroed_obliquity_rows += transformed_ce != ce
    transformed_meeus.append((nd, nlp, nl, nf, nom, sp, spt, transformed_ce, cet))
  _require_rows_equal("Meeus nutation", transformed_meeus, target_meeus)
  _require(zeroed_obliquity_rows == 12, f"Meeus zeroed obliquity rows={zeroed_obliquity_rows}, expected 12")

  return IdentityCounts(
    leap_rows=len(leap_rows),
    lunar_rows=len(target_lr) + len(target_b),
    iau1980_rows=len(target_iau1980),
    meeus_rows=len(target_meeus),
    meeus_zeroed_obliquity_rows=zeroed_obliquity_rows,
  )
