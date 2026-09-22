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

import argparse
import hashlib
import re

from collections import Counter
from dataclasses import dataclass
from decimal import Decimal
from pathlib import Path
from typing import Final, Sequence


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
UPSTREAM_ROOT: Final[str] = "https://ftp.imcce.fr/pub/ephem/planets/vsop87"
CHECK_SOURCE_SHA256: Final[str] = "f8fa52449262be05a22a96840c1acbad0b35c8999e00b5c0477ba8a91a67a51a"
CHECK_GENERATED_PATH: Final[Path] = Path("src/test/astro/vsop87d_check_data.hpp")
CHECK_GENERATED_SHA256: Final[str] = "8bdedcccb48e6d0dcbd2d3e18430738d6ef4a45295e9149be53c72b93017308f"
HEADER_PATTERN: Final[re.Pattern[str]] = re.compile(
  r"^ VSOP87 VERSION D4\s+([A-Z]+)\s+VARIABLE ([123]) \(LBR\)\s+\*T\*\*(\d)\s+(\d+) TERMS?\s+"
  r"HELIOCENTRIC DYNAMICAL ECLIPTIC AND EQUINOX OF THE DATE$"
)
CHECK_HEADER_PATTERN: Final[re.Pattern[str]] = re.compile(
  r"^VSOP87D\s+([A-Z]+)\s+JD(\d+\.\d)\s+\d{2}/\d{2}/\d{4}\s+12h TDB$"
)
DECIMAL_PATTERN: Final[re.Pattern[str]] = re.compile(r"[+-]?\d+\.\d{11}")
CHECK_DECIMAL_PATTERN: Final[re.Pattern[str]] = re.compile(r"[+-]?(?:\d+)?\.\d{10}")
CHECK_JDS: Final[tuple[str, ...]] = (
  "2451545.0",
  "2415020.0",
  "2378495.0",
  "2341970.0",
  "2305445.0",
  "2268920.0",
  "2232395.0",
  "2195870.0",
  "2159345.0",
  "2122820.0",
)


@dataclass(frozen=True)
class PlanetSpec:
  key: str
  display_name: str
  enum_name: str
  source_name: str
  source_sha256: str
  tables: tuple[tuple[str, int], ...]
  generated_sha256: str

  @property
  def generated_path(self) -> Path:
    return Path(f"src/astro/vsop87d/{self.key}_coeff.hpp")


@dataclass(frozen=True)
class Coefficient:
  A: str
  B: str
  C: str


@dataclass(frozen=True)
class Series:
  name: str
  terms: tuple[Coefficient, ...]


@dataclass(frozen=True)
class CheckRow:
  planet: str
  jd: str
  λ: str
  β: str
  r: str


@dataclass(frozen=True)
class ImportCounts:
  planets: int
  series: int
  terms: int
  check_rows: int


PLANETS: Final[tuple[PlanetSpec, ...]] = (
  PlanetSpec(
    key="mercury",
    display_name="MERCURY",
    enum_name="MER",
    source_name="VSOP87D.mer",
    source_sha256="f468481b5a05080a943ad4746ff7ea7e0ff6652b71a46d83c9c636cb69485e34",
    tables=(
      ("L0", 1380),
      ("L1", 839),
      ("L2", 395),
      ("L3", 153),
      ("L4", 28),
      ("L5", 13),
      ("B0", 818),
      ("B1", 494),
      ("B2", 230),
      ("B3", 53),
      ("B4", 15),
      ("B5", 10),
      ("R0", 1215),
      ("R1", 711),
      ("R2", 326),
      ("R3", 119),
      ("R4", 18),
      ("R5", 10),
    ),
    generated_sha256="09e745e40b5cf3d9ad68d1715c3952c21153d1fbc8e05f1b4570252448439fcc",
  ),
  PlanetSpec(
    key="venus",
    display_name="VENUS",
    enum_name="VEN",
    source_name="VSOP87D.ven",
    source_sha256="cb2f3a738289ed45f69fec1845e480baf4b32d481eccc21b8629a2d0d10e8261",
    tables=(
      ("L0", 367),
      ("L1", 215),
      ("L2", 70),
      ("L3", 9),
      ("L4", 5),
      ("L5", 5),
      ("B0", 210),
      ("B1", 133),
      ("B2", 59),
      ("B3", 15),
      ("B4", 5),
      ("B5", 4),
      ("R0", 330),
      ("R1", 180),
      ("R2", 63),
      ("R3", 7),
      ("R4", 3),
      ("R5", 2),
    ),
    generated_sha256="acd5bdc121fca49c8aeedec17888207ec7eb341b14bc75102454576089b3e65a",
  ),
  PlanetSpec(
    key="mars",
    display_name="MARS",
    enum_name="MAR",
    source_name="VSOP87D.mar",
    source_sha256="b1184df9553d85ffcf904c16bd437ab668804fa98859f27fe2e7bf6cfa6bc07e",
    tables=(
      ("L0", 1217),
      ("L1", 686),
      ("L2", 310),
      ("L3", 129),
      ("L4", 36),
      ("L5", 15),
      ("B0", 441),
      ("B1", 287),
      ("B2", 130),
      ("B3", 41),
      ("B4", 11),
      ("B5", 5),
      ("R0", 1118),
      ("R1", 596),
      ("R2", 313),
      ("R3", 111),
      ("R4", 28),
      ("R5", 9),
    ),
    generated_sha256="b590c01ecd2f132fb59033fb632e7533ec61624e301544f4c51b2f00faa74754",
  ),
  PlanetSpec(
    key="jupiter",
    display_name="JUPITER",
    enum_name="JUP",
    source_name="VSOP87D.jup",
    source_sha256="3f3dfbc7d117ecad2b2dadf2fc626b260a3cd5efa98e7d4c6b26cd682fc48090",
    tables=(
      ("L0", 760),
      ("L1", 369),
      ("L2", 191),
      ("L3", 109),
      ("L4", 45),
      ("L5", 10),
      ("B0", 249),
      ("B1", 141),
      ("B2", 81),
      ("B3", 42),
      ("B4", 12),
      ("B5", 5),
      ("R0", 745),
      ("R1", 381),
      ("R2", 190),
      ("R3", 98),
      ("R4", 46),
      ("R5", 9),
    ),
    generated_sha256="3aacf1d698d3502bf35b9c4fa4099ac0f255bfbef26d2d14ea0d5ba01b33e14f",
  ),
  PlanetSpec(
    key="saturn",
    display_name="SATURN",
    enum_name="SAT",
    source_name="VSOP87D.sat",
    source_sha256="2e49e19396f24c17298f0b667e7763ee5c28b60d549c89d72a17dfd5f8d46b05",
    tables=(
      ("L0", 1152),
      ("L1", 642),
      ("L2", 321),
      ("L3", 148),
      ("L4", 68),
      ("L5", 27),
      ("B0", 500),
      ("B1", 260),
      ("B2", 111),
      ("B3", 58),
      ("B4", 26),
      ("B5", 11),
      ("R0", 1205),
      ("R1", 639),
      ("R2", 342),
      ("R3", 157),
      ("R4", 64),
      ("R5", 28),
    ),
    generated_sha256="ef0a66af47b52915daa32383e0dcc34ced959308c56024bb557f18d82831a317",
  ),
  PlanetSpec(
    key="uranus",
    display_name="URANUS",
    enum_name="URA",
    source_name="VSOP87D.ura",
    source_sha256="80eb3a778d53f450066d9b13f17e15b979e253437d22f33ec5a4604cdb2872a7",
    tables=(
      ("L0", 947),
      ("L1", 426),
      ("L2", 151),
      ("L3", 46),
      ("L4", 7),
      ("L5", 1),
      ("B0", 283),
      ("B1", 154),
      ("B2", 60),
      ("B3", 16),
      ("B4", 2),
      ("R0", 1124),
      ("R1", 514),
      ("R2", 192),
      ("R3", 55),
      ("R4", 11),
    ),
    generated_sha256="e5a67f411b593a1e139e1f3b35bff4608d1ead90e7f52f8ac1e8a875e1ba7c20",
  ),
  PlanetSpec(
    key="neptune",
    display_name="NEPTUNE",
    enum_name="NEP",
    source_name="VSOP87D.nep",
    source_sha256="3ff65a5cabc04c411f975888f77268b89e336ece0b75d198a3935090fdde3ae6",
    tables=(
      ("L0", 423),
      ("L1", 183),
      ("L2", 57),
      ("L3", 15),
      ("L4", 2),
      ("L5", 1),
      ("B0", 172),
      ("B1", 82),
      ("B2", 25),
      ("B3", 9),
      ("B4", 1),
      ("B5", 1),
      ("R0", 607),
      ("R1", 250),
      ("R2", 72),
      ("R3", 22),
      ("R4", 7),
    ),
    generated_sha256="ec12eadf6b0dbe0bf906d67ca766d8a6daafd17fb2976fa1ed577b5c825df28b",
  ),
)


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _sha256(data: bytes) -> str:
  return hashlib.sha256(data).hexdigest()


def _scaled_amplitude(value: str) -> str:
  scaled = Decimal(value) * Decimal(100_000_000)
  rendered = f"{scaled:.3f}"
  _require(Decimal(rendered) == scaled, f"VSOP87D amplitude cannot be shifted losslessly: {value}")
  return rendered


def parse_source(data: bytes, spec: PlanetSpec) -> tuple[Series, ...]:
  digest = _sha256(data)
  _require(digest == spec.source_sha256, f"{spec.source_name} source hash mismatch: {digest}")
  try:
    lines = data.decode("ascii").splitlines()
  except UnicodeDecodeError as error:
    raise RuntimeError(f"{spec.source_name} is not ASCII") from error

  parsed: list[Series] = []
  current_name: str | None = None
  expected_terms = 0
  terms: list[Coefficient] = []

  def finish_series() -> None:
    nonlocal terms
    if current_name is None:
      return
    _require(
      len(terms) == expected_terms,
      f"{spec.source_name} {current_name} term count differs: {len(terms)} != {expected_terms}",
    )
    parsed.append(Series(current_name, tuple(terms)))
    terms = []

  for line_number, line in enumerate(lines, start=1):
    header = HEADER_PATTERN.fullmatch(line)
    if header is not None:
      finish_series()
      planet, variable, power, count = header.groups()
      _require(planet == spec.display_name, f"{spec.source_name}:{line_number} planet differs: {planet}")
      current_name = f"{'LBR'[int(variable) - 1]}{power}"
      expected_terms = int(count)
      continue

    _require(current_name is not None, f"{spec.source_name}:{line_number} term precedes a series header")
    _require(len(line) == 132, f"{spec.source_name}:{line_number} term width differs: {len(line)}")
    term_index = line[5:10].strip()
    _require(
      term_index == str(len(terms) + 1),
      f"{spec.source_name}:{line_number} term index differs: {term_index}",
    )
    A, B, C = (line[79:97].strip(), line[97:111].strip(), line[111:132].strip())
    _require(
      all(DECIMAL_PATTERN.fullmatch(value) is not None for value in (A, B, C)),
      f"{spec.source_name}:{line_number} coefficient spelling differs",
    )
    terms.append(Coefficient(_scaled_amplitude(A), B, C))

  finish_series()
  inventory = tuple((series.name, len(series.terms)) for series in parsed)
  _require(inventory == spec.tables, f"{spec.source_name} series inventory differs: {inventory}")
  return tuple(parsed)


def _project_banner() -> list[str]:
  return [
    "/*",
    " * CelestialCalendar:",
    " *   A C++23-style library that performs astronomical calculations and date conversions between",
    " *   Gregorian and Chinese Lunar calendars.",
    " *",
    " * Copyright (C) 2026 Ningqi Wang (0xf3cd)",
    " * Email: nq.maigre@gmail.com",
    " * Repo : https://github.com/0xf3cd/celestial-calendar",
    " *",
    " * SPDX-License-Identifier: MIT",
    " */",
  ]


def render_header(spec: PlanetSpec, series: Sequence[Series]) -> bytes:
  all_terms = tuple(term for table in series for term in table.terms)
  widths = (
    max(len(term.A) for term in all_terms),
    max(len(term.B) for term in all_terms),
    max(len(term.C) for term in all_terms),
  )
  lines = [
    *_project_banner(),
    "",
    "#pragma once",
    "",
    "#include <array>",
    "",
    '#include "defines.hpp"',
    "",
    f"namespace astro::vsop87d::{spec.key}_coeff {{",
    "",
    f"// Retained material boundary (R12/#58 extension): these complete {spec.display_name.title()} VSOP87D",
    f"// coefficient tables come from the January 1996 IMCCE distribution, `{spec.source_name}`. They remain",
    "// under their source terms and outside the project MIT grant.",
    "",
    "using Coefficients  = astro::vsop87d::Coefficients;",
    "using Vsop87dTable  = astro::vsop87d::Vsop87dTable;",
    "using Vsop87dTables = astro::vsop87d::Vsop87dTables;",
    "",
    "// Generated by automation/vsop87d_import.py. Do not edit coefficient rows by hand.",
    "// NOLINTBEGIN(modernize-use-designated-initializers)",
  ]

  for table in series:
    lines.extend(("", f"inline constexpr std::array<Coefficients, {len(table.terms)}> {table.name} {{{{"))
    lines.extend(
      f"  {{ {term.A:>{widths[0]}}, {term.B:>{widths[1]}}, {term.C:>{widths[2]}}, }}," for term in table.terms
    )
    lines.append("}};")

  lines.extend(("", "// NOLINTEND(modernize-use-designated-initializers)"))
  for coordinate in "LBR":
    table_names = [table.name for table in series if table.name.startswith(coordinate)]
    joined = ", ".join(table_names)
    lines.extend(
      (
        "",
        f"inline constexpr std::array<Vsop87dTable, {len(table_names)}> {coordinate}_ARRAY {{ {joined}, }};",
        f"inline constexpr Vsop87dTables {coordinate} {{ {coordinate}_ARRAY }};",
      )
    )

  lines.extend(
    (
      "",
      f"}} // namespace astro::vsop87d::{spec.key}_coeff",
      "",
      "namespace astro::vsop87d {",
      "",
      f"/** @brief Specialize `PlanetTables` for `Planet::{spec.enum_name}`. */",
      "template <>",
      f"struct PlanetTables<Planet::{spec.enum_name}> {{",
      f"  static const inline Vsop87dTables& L = {spec.key}_coeff::L;",
      f"  static const inline Vsop87dTables& B = {spec.key}_coeff::B;",
      f"  static const inline Vsop87dTables& R = {spec.key}_coeff::R;",
      "};",
      "",
      "} // namespace astro::vsop87d",
      "",
    )
  )
  return "\n".join(lines).encode("ascii")


def parse_check_source(data: bytes, source_sha256: str = CHECK_SOURCE_SHA256) -> tuple[CheckRow, ...]:
  digest = _sha256(data)
  _require(digest == source_sha256, f"vsop87.chk source hash mismatch: {digest}")
  try:
    lines = data.decode("ascii").splitlines()
  except UnicodeDecodeError as error:
    raise RuntimeError("vsop87.chk is not ASCII") from error

  enum_names = {spec.display_name: spec.enum_name for spec in PLANETS} | {"EARTH": "EAR"}
  rows: list[CheckRow] = []
  for index, line in enumerate(lines):
    header = CHECK_HEADER_PATTERN.fullmatch(line.strip())
    if header is None:
      continue
    planet, jd = header.groups()
    _require(planet in enum_names, f"vsop87.chk planet differs: {planet}")
    _require(index + 1 < len(lines), f"vsop87.chk position row is missing after JD{jd}")
    fields = lines[index + 1].split()
    _require(
      len(fields) == 9 and (fields[0], fields[2], fields[3], fields[5], fields[6], fields[8]) == (
        "l",
        "rad",
        "b",
        "rad",
        "r",
        "au",
      ),
      f"vsop87.chk position row differs after JD{jd}",
    )
    λ, β, radius = fields[1], fields[4], fields[7]
    _require(
      all(CHECK_DECIMAL_PATTERN.fullmatch(value) is not None for value in (λ, β, radius)),
      f"vsop87.chk coordinate spelling differs after JD{jd}",
    )
    rows.append(CheckRow(enum_names[planet], jd, λ, β, radius))

  counts = Counter(row.planet for row in rows)
  _require(counts == {enum_name: 10 for enum_name in enum_names.values()}, f"vsop87.chk block counts differ: {counts}")
  for enum_name in enum_names.values():
    _require(
      tuple(row.jd for row in rows if row.planet == enum_name) == CHECK_JDS,
      f"vsop87.chk JD inventory differs for {enum_name}",
    )
  return tuple(rows)


def render_check_header(rows: Sequence[CheckRow]) -> bytes:
  widths = tuple(
    max(len(getattr(row, field)) for row in rows)
    for field in ("planet", "jd", "λ", "β", "r")
  )
  lines = [
    *_project_banner(),
    "",
    "#pragma once",
    "",
    "#include <array>",
    "",
    '#include "vsop87d/defines.hpp"',
    "",
    "namespace astro::vsop87d::test_data {",
    "",
    "struct OfficialCheckRow {",
    "  Planet planet;",
    "  double jd;",
    "  double λ;",
    "  double β;",
    "  double r;",
    "};",
    "",
    "// Retained material boundary (R12/#58 extension): these position rows are the complete VSOP87D subset",
    "// of the January 1996 IMCCE distribution's `vsop87.chk`. They remain under their source terms and",
    "// outside the project MIT grant.",
    "//",
    "// The source labels the epochs TDB; `vsop87.doc` instructs users to treat the argument as TT. Tests pass",
    "// the printed JD numbers directly and compare only l, b, and r, each printed to ten decimal places.",
    "// Generated by automation/vsop87d_import.py. Do not edit rows by hand.",
    "// NOLINTBEGIN(modernize-use-designated-initializers)",
    f"inline constexpr std::array<OfficialCheckRow, {len(rows)}> OFFICIAL_CHECK_ROWS {{{{",
    "  // Planet       JD            l (rad)        b (rad)        r (AU)",
  ]
  lines.extend(
    f"  {{ Planet::{row.planet:<{widths[0]}}, {row.jd:>{widths[1]}}, {row.λ:>{widths[2]}}, "
    f"{row.β:>{widths[3]}}, {row.r:>{widths[4]}}, }},"
    for row in rows
  )
  lines.extend(
    (
      "}};",
      "// NOLINTEND(modernize-use-designated-initializers)",
      "",
      "} // namespace astro::vsop87d::test_data",
      "",
    )
  )
  return "\n".join(lines).encode("utf-8")


def _counts() -> ImportCounts:
  return ImportCounts(
    planets=len(PLANETS),
    series=sum(len(spec.tables) for spec in PLANETS),
    terms=sum(count for spec in PLANETS for _name, count in spec.tables),
    check_rows=80,
  )


def verify_repository(repo_root: Path = REPO_ROOT, specs: Sequence[PlanetSpec] = PLANETS) -> ImportCounts:
  for spec in specs:
    path = repo_root / spec.generated_path
    digest = _sha256(path.read_bytes())
    _require(digest == spec.generated_sha256, f"{spec.generated_path} generated hash mismatch: {digest}")
  check_digest = _sha256((repo_root / CHECK_GENERATED_PATH).read_bytes())
  _require(
    check_digest == CHECK_GENERATED_SHA256,
    f"{CHECK_GENERATED_PATH} generated hash mismatch: {check_digest}",
  )
  return _counts()


def replay(source_dir: Path, check_file: Path, repo_root: Path = REPO_ROOT, write: bool = False) -> ImportCounts:
  outputs: list[tuple[Path, bytes]] = []
  for spec in PLANETS:
    series = parse_source((source_dir / spec.source_name).read_bytes(), spec)
    outputs.append((spec.generated_path, render_header(spec, series)))
  check_rows = parse_check_source(check_file.read_bytes())
  outputs.append((CHECK_GENERATED_PATH, render_check_header(check_rows)))

  for relative, expected in outputs:
    destination = repo_root / relative
    if write:
      destination.write_bytes(expected)
    else:
      _require(destination.read_bytes() == expected, f"{relative} differs from the deterministic replay")
    print(f"{_sha256(expected)}  {relative}")
  return _counts()


def main() -> None:
  parser = argparse.ArgumentParser(description="Replay or verify the complete retained VSOP87D planetary corpus")
  parser.add_argument("--source-dir", type=Path, help="directory containing the seven official VSOP87D.* files")
  parser.add_argument("--check-file", type=Path, help="official vsop87.chk file")
  parser.add_argument("--write", action="store_true", help="write the deterministic generated headers")
  args = parser.parse_args()

  if (args.source_dir is None) != (args.check_file is None):
    parser.error("--source-dir and --check-file must be supplied together")
  if args.write and args.source_dir is None:
    parser.error("--write requires --source-dir and --check-file")

  counts = (
    verify_repository()
    if args.source_dir is None
    else replay(args.source_dir, args.check_file, write=args.write)
  )
  print(
    f"planets={counts.planets} series={counts.series} terms={counts.terms} "
    f"check_rows={counts.check_rows}"
  )


if __name__ == "__main__":
  main()
