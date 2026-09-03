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
import json
import re

from dataclasses import dataclass
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
from typing import Final

if __package__:
  from .astrotime_delta_t_provenance import (
    ASTROTIME_ALGO5_RECORD_SHA256,
    ASTROTIME_ALGO5_ROOT_RELATIVE,
  )
  from .source_digest import canonical_cpp
else:
  from astrotime_delta_t_provenance import (
    ASTROTIME_ALGO5_RECORD_SHA256,
    ASTROTIME_ALGO5_ROOT_RELATIVE,
  )
  from source_digest import canonical_cpp


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
NASA_ROOT: Final[Path] = REPO_ROOT / "src" / "test" / "provenance" / "nasa" / "tp-2006-214141"
NASA_RECORD: Final[Path] = NASA_ROOT / "delta_t.json"
NASA_ACKNOWLEDGMENT: Final[Path] = NASA_ROOT / "ACKNOWLEDGMENT.txt"
NASA_RECORD_SHA256: Final[str] = "db26c77ea3d92e8663541d2aa83b9693f3ecc04846cc152c832a88a399b4da5b"
NASA_ACKNOWLEDGMENT_SHA256: Final[str] = "2d90c4731996cd9b8586c055eb4c29535ebab66abe426b53ae944d15a4887881"
NASA_NOTICE_APPLICABILITY: Final[str] = (
  "the NASA/TP-2006-214141 Delta-T polynomial material in src/astro/delta_t.hpp, the 398 non-HKO lunar-year "
  "table values in src/calendar/lunar/algo3.hpp preserving their NASA-backed historical generation relation, and the "
  "NASA-sourced historical Delta-T validation values in src/test"
)


@dataclass(frozen=True)
class ProvenanceCounts:
  runtime_branches: int
  nasa_v25_rows: int
  usno_v25_rows: int
  bulletin_a_v25_rows: int
  v27_relations: int


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _normalized(text: str) -> str:
  return " ".join(text.split())


def _read_pinned(path: Path, expected_sha256: str, label: str) -> bytes:
  data = path.read_bytes()
  digest = hashlib.sha256(data).hexdigest()
  _require(digest == expected_sha256, f"{label} hash mismatch: {digest}")
  return data


def _delta_t_rows(text: str) -> dict[int, Decimal]:
  start = text.index("const inline DatasetType ACCURATE_DELTA_T_TABLE")
  end = text.index("};", start)
  rows = re.findall(r"\{\s*(\d{4})\.0,\s*(\d+\.\d+)\s*\}", text[start:end])
  return {int(year): Decimal(value) for year, value in rows}


def _usno_delta_t_rows(path: Path) -> dict[int, Decimal]:
  rows = {}
  for line in path.read_text(encoding="ascii").splitlines():
    year, month, day, value = line.split()
    if month == "1" and day == "1":
      rows[int(year)] = Decimal(value)
  return rows


def _rounded(value: str, places: int) -> Decimal:
  quantum = Decimal(1).scaleb(-places)
  return Decimal(value).quantize(quantum, rounding=ROUND_HALF_UP)


def _lunar_data_values(text: str) -> tuple[int, ...]:
  match = re.search(r"LUNAR_DATA\s*=\s*\{(.*?)\n\};", text, re.DOTALL)
  _require(match is not None, "NASA lunar-table data differs")
  values = tuple(int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", match.group(1)))
  _require(len(values) == 600, "NASA lunar-table data differs")
  return values


def _cpp_block(text: str, declaration: str) -> str:
  start = text.index(declaration)
  opening = text.index("{", start)
  depth = 0
  for index in range(opening, len(text)):
    if text[index] == "{":
      depth += 1
    elif text[index] == "}":
      depth -= 1
      if depth == 0:
        return text[start : index + 1]
  raise RuntimeError(f"C++ block is incomplete: {declaration}")


def verify_nasa_delta_t_provenance(
  repo_root: Path = REPO_ROOT,
  nasa_root: Path = NASA_ROOT,
  record_sha256: str = NASA_RECORD_SHA256,
  acknowledgment_sha256: str = NASA_ACKNOWLEDGMENT_SHA256,
  notice_applicability: str = NASA_NOTICE_APPLICABILITY,
  algo5_root: Path | None = None,
  algo5_record_sha256: str = ASTROTIME_ALGO5_RECORD_SHA256,
) -> ProvenanceCounts:
  record_path = nasa_root / "delta_t.json"
  acknowledgment_path = nasa_root / "ACKNOWLEDGMENT.txt"
  record = json.loads(_read_pinned(record_path, record_sha256, "NASA provenance record"))
  acknowledgment = _read_pinned(acknowledgment_path, acknowledgment_sha256, "NASA acknowledgment")
  if algo5_root is None:
    algo5_root = repo_root / ASTROTIME_ALGO5_ROOT_RELATIVE
  algo5_record = json.loads(
    _read_pinned(algo5_root / "record.json", algo5_record_sha256, "AstroTime algo5 provenance record")
  )

  publication = record["publication"]
  _require(record["schema"] == 1, "NASA provenance schema differs")
  _require(publication["identifier"] == "NASA/TP-2006-214141", "NASA publication identifier differs")
  _require(publication["ntrs_document"] == "20070003587", "NASA NTRS document differs")
  _require(publication["published"] == "2006-10", "NASA publication date differs")
  _require(publication["upstream_publication_bytes_retained"] is False, "NASA publication bytes were not retained")
  _require(publication["upstream_byte_identity_claim"] is False, "NASA record must not claim upstream-byte identity")
  _require(b"NASA/TP-2006-214141" in acknowledgment, "NASA acknowledgment omits the publication identifier")
  _require(b"Fred Espenak and Jean Meeus" in acknowledgment, "NASA acknowledgment omits the authors")
  _require(
    notice_applicability
    == "the NASA/TP-2006-214141 Delta-T polynomial material in src/astro/delta_t.hpp, the 398 non-HKO lunar-year "
    "table values in src/calendar/lunar/algo3.hpp preserving their NASA-backed historical generation relation, and the "
    "NASA-sourced historical Delta-T validation values in src/test",
    "NASA notice applicability differs",
  )

  runtime = record["runtime_relation"]
  _require(
    runtime["source_location"] == "Section 2.7, equations (11)-(25), printed pages 14-16",
    "NASA locator differs",
  )
  branches = runtime["branches"]
  _require([branch["equation"] for branch in branches] == list(range(11, 26)), "NASA equation inventory differs")
  _require(
    runtime["canonical_function"]
    == {
      "canonicalization": "remove C/C++ comments, then replace each whitespace run with one ASCII space",
      "sha256": "ae0fd9457995ccb48cc61593f832ed2f5be91dfdde3314a60580f285e1806b9f",
    },
    "NASA canonical function record differs",
  )
  expected_intervals = (
    "year < -500",
    "-500 <= year < 500",
    "500 <= year < 1600",
    "1600 <= year < 1700",
    "1700 <= year < 1800",
    "1800 <= year < 1860",
    "1860 <= year < 1900",
    "1900 <= year < 1920",
    "1920 <= year < 1941",
    "1941 <= year < 1961",
    "1961 <= year < 1986",
    "1986 <= year < 2005",
    "2005 <= year < 2050",
    "2050 <= year < 2150",
    "year >= 2150",
  )
  _require(tuple(branch["interval"] for branch in branches) == expected_intervals, "NASA interval inventory differs")

  delta_t = (repo_root / runtime["repository_path"]).read_text(encoding="utf-8")
  algo2_source = delta_t[delta_t.index("namespace algo2 {") : delta_t.index("} // namespace algo2")]
  algo2 = _normalized(algo2_source)
  for branch in branches:
    snippet = branch["cpp"]
    _require(algo2.count(snippet) == 1, f"NASA equation ({branch['equation']}) repository relation differs")
  _require(
    "NASA/TP-2006-214141, Section 2.7, equations (11)-(25)" in algo2,
    "NASA runtime citation differs",
  )
  function_source = _cpp_block(
    algo2_source,
    "[[nodiscard]] constexpr auto compute(const double year) noexcept -> double",
  )
  canonical_function = canonical_cpp(function_source)
  _require(
    hashlib.sha256(canonical_function.encode("utf-8")).hexdigest() == runtime["canonical_function"]["sha256"],
    "NASA runtime canonical function differs",
  )

  downstream = record["downstream_runtime_relation"]
  _require(
    downstream["repository_path"] == "src/calendar/lunar/algo3.hpp"
    and downstream["repository_symbol"] == "calendar::lunar::algo3::LUNAR_DATA",
    "NASA lunar-table repository relation differs",
  )
  _require(
    downstream["historical_generation"]
    == {
      "origin_commit": "0bf19c3fb77d74ea8f17fc1f4d5f4962de48151c",
      "generator": "calendar::lunar::algo2::calc_lunar_year",
      "year_ranges": ["1600-1900", "2100-2199"],
      "entries": 401,
      "source_relation": "the then-default Delta-T route used astro::delta_t::algo2::compute",
    },
    "NASA lunar-table historical generation differs",
  )
  _require(
    downstream["current_retention"]
    == {
      "all_current_values": {
        "entries": 401,
        "canonicalization": (
          "ASCII lines YEAR:VALUE\\n in ascending year order; VALUE is eight lowercase hexadecimal digits without 0x"
        ),
        "sha256": "0c110ba78423f7babededdf3629f349d6bcf2906af71594bdb5688998f8ea359",
      },
      "entries_from_origin": 398,
      "retained_origin_values": {
        "canonicalization": (
          "ASCII lines YEAR:VALUE\\n in ascending year order; VALUE is eight lowercase hexadecimal digits without 0x"
        ),
        "sha256": "66c5892f0de175f4aee35bd41912f8290bda5b1a4b239d784dcc89f4a589f9a4",
      },
      "rebake": {
        "commit": "8194ffbf38657d66158d2314bdf66294c7a8d001",
        "generator": "calendar::lunar::algo2::calc_lunar_year",
        "years": [2133, 2165, 2172],
        "values": {
          "canonicalization": (
            "ASCII lines YEAR:VALUE\\n in ascending year order; VALUE is eight lowercase hexadecimal digits without 0x"
          ),
          "sha256": "8a9e82b389f66578387f3bc098600f555f0f39b460fc40751e789cb5ffda71a4",
        },
      },
    },
    "NASA lunar-table current retention differs",
  )
  _require(
    downstream["historical_environment"]
    == {
      "status": "unrecovered",
      "unavailable": [
        "compiler and version",
        "compiler flags",
        "standard library and libm versions",
        "operating system and architecture",
        "dependency versions",
      ],
      "claim": ("The 2024 numerical environment was not recovered; no bit-for-bit historical regeneration is claimed."),
    },
    "NASA lunar-table historical environment differs",
  )
  current_regeneration = downstream["current_regeneration"]
  _require(
    current_regeneration
    == {
      "matching_entries": 401,
      "repository_test": "src/test/lunar/algo3_test.cpp",
      "test": "LunarAlgo3.BakedMatchesLiveAlgo2",
      "canonical_test": {
        "canonicalization": "remove C/C++ comments, then replace each whitespace run with one ASCII space",
        "sha256": "236ca2928c891d2ea9d5bb0a3030f830f1af54496319d5534ac92ef4d33672e8",
      },
    },
    "NASA lunar-table current regeneration differs",
  )
  lunar_table = (repo_root / downstream["repository_path"]).read_text(encoding="utf-8")
  lunar_values = _lunar_data_values(lunar_table)
  all_years = (*range(1600, 1901), *range(2100, 2200))
  retained_years = tuple(year for year in (*range(1600, 1901), *range(2100, 2200)) if year not in {2133, 2165, 2172})
  _require(
    len(retained_years) == downstream["current_retention"]["entries_from_origin"],
    "NASA lunar-table retained count differs",
  )
  retained_bytes = "".join(f"{year}:{lunar_values[year - 1600]:08x}\n" for year in retained_years).encode("ascii")
  _require(
    hashlib.sha256(retained_bytes).hexdigest() == downstream["current_retention"]["retained_origin_values"]["sha256"],
    "NASA lunar-table retained origin values differ",
  )
  rebake_years = downstream["current_retention"]["rebake"]["years"]
  rebake_bytes = "".join(f"{year}:{lunar_values[year - 1600]:08x}\n" for year in rebake_years).encode("ascii")
  _require(
    hashlib.sha256(rebake_bytes).hexdigest() == downstream["current_retention"]["rebake"]["values"]["sha256"],
    "NASA lunar-table rebake values differ",
  )
  all_current_bytes = "".join(f"{year}:{lunar_values[year - 1600]:08x}\n" for year in all_years).encode("ascii")
  _require(
    hashlib.sha256(all_current_bytes).hexdigest() == downstream["current_retention"]["all_current_values"]["sha256"],
    "NASA lunar-table all current values differ",
  )
  _require(
    "NASA/TP-2006-214141, Section 2.7, equations (11)-(25); historical source for 398\n"
    " *      retained non-HKO entries." in lunar_table,
    "NASA lunar-table citation differs",
  )
  lunar_test = (repo_root / current_regeneration["repository_test"]).read_text(encoding="utf-8")
  _require(
    f"TEST({current_regeneration['test'].replace('.', ', ')})" in lunar_test
    and "  ASSERT_EQ(401, checked);\n" in lunar_test,
    "NASA lunar-table full-regeneration gate differs",
  )
  test_source = _cpp_block(lunar_test, f"TEST({current_regeneration['test'].replace('.', ', ')})")
  _require(
    hashlib.sha256(canonical_cpp(test_source).encode("utf-8")).hexdigest()
    == current_regeneration["canonical_test"]["sha256"],
    "NASA lunar-table canonical regeneration test differs",
  )

  validation = record["validation_relations"]
  v25 = validation["v25"]
  partitions = v25["partitions"]
  _require(len(partitions) == 3, "V25 source partition count differs")
  nasa_partition, usno_partition, bulletin_a_partition = partitions
  _require(nasa_partition["source"] == "NASA/TP-2006-214141 Table 2-2", "V25 NASA source differs")
  _require(usno_partition["source"] == "USNO deltat.data", "V25 USNO source differs")
  _require(nasa_partition["years"] == list(range(1955, 2010, 5)), "V25 NASA year partition differs")
  _require(usno_partition["years"] == [2010, 2014], "V25 USNO year partition differs")
  algo5_v25 = algo5_record["validation_relations"]["v25_2015_2026"]
  _require(
    bulletin_a_partition
    == {
      "source": "IERS Bulletin A via the retained AstroTime algo5 record",
      "source_record": (ASTROTIME_ALGO5_ROOT_RELATIVE / "record.json").as_posix(),
      "source_record_sha256": ASTROTIME_ALGO5_RECORD_SHA256,
      "source_commit": algo5_v25["source_commit"],
      "source_tree": algo5_v25["source_tree"],
      "years": algo5_v25["years"],
      "window_observations": algo5_v25["window_observations"],
      "source_medians": algo5_v25["source_medians"],
      "repository_values": algo5_v25["repository_values"],
      "relation": (
        "median of 31 observations within 15 days inclusive of each January 1 boundary, rounded to two decimal places"
      ),
    },
    "V25 Bulletin A relation differs",
  )

  helper_text = (repo_root / v25["repository_path"]).read_text(encoding="utf-8")
  repository_rows = _delta_t_rows(helper_text)
  covered_years = set(nasa_partition["years"]) | set(usno_partition["years"]) | set(bulletin_a_partition["years"])
  _require(set(repository_rows) == covered_years, "V25 dataset rows are not covered by the recorded source partitions")
  for year, value in zip(nasa_partition["years"], nasa_partition["repository_values"], strict=True):
    _require(repository_rows[year] == Decimal(value), f"V25 NASA value differs for {year}")
  usno_rows = _usno_delta_t_rows(repo_root / "statistics/usno_data.txt")
  for year, source, target in zip(
    usno_partition["years"],
    usno_partition["source_values"],
    usno_partition["repository_values"],
    strict=True,
  ):
    _require(usno_rows[year] == Decimal(source), f"V25 USNO source value differs for {year}")
    _require(_rounded(source, 1) == Decimal(target), f"V25 USNO rounding relation differs for {year}")
    _require(repository_rows[year] == Decimal(target), f"V25 repository value differs for {year}")
  for year, source, target in zip(
    bulletin_a_partition["years"],
    bulletin_a_partition["source_medians"],
    bulletin_a_partition["repository_values"],
    strict=True,
  ):
    _require(_rounded(source, 2) == Decimal(target), f"V25 Bulletin A rounding relation differs for {year}")
    _require(repository_rows[year] == Decimal(target), f"V25 Bulletin A repository value differs for {year}")
  _require("NASA/TP-2006-214141 Table 2-2" in helper_text, "V25 NASA citation differs")
  _require("USNO deltat.data" in helper_text, "V25 USNO citation differs")
  _require(
    "2015-2026: IERS Bulletin A final values at AstroTime-Analysis ddf3be1" in helper_text
    and "record ed1cdc2f" in helper_text
    and "share algo5's fit input" in helper_text
    and "not independent accuracy" in helper_text,
    "V25 Bulletin A citation differs",
  )

  v27 = validation["v27"]
  year_500 = v27["year_500"]
  year_2000 = v27["year_2000"]
  _require(year_500["delta_t_seconds"] == 5710, "V27 year-500 value differs")
  _require(year_500["source_standard_error_seconds"] == 140, "V27 year-500 140 s locator differs")
  _require(
    year_500["source_location"] == "Table 2-1; scientific lineage in Section 2.6",
    "V27 year-500 locator differs",
  )
  _require(
    _rounded(year_2000["source_value"], 2) == Decimal(year_2000["repository_value"]),
    "V27 year-2000 rounding differs",
  )
  _require(usno_rows[2000] == Decimal(year_2000["source_value"]), "V27 year-2000 USNO source value differs")
  julian_day = (repo_root / v27["repository_path"]).read_text(encoding="utf-8")
  _require("63.8285 s" in julian_day and "63.83 s" in julian_day, "V27 year-2000 citation relation differs")
  _require("NASA/TP-2006-214141 Table 2-1" in julian_day, "V27 year-500 NASA citation differs")
  _require("5710 s" in julian_day and "140 s" in julian_day, "V27 year-500 value or locator differs")
  _require("23h + 58min + 56s + 170ms" in julian_day, "V27 year-2000 repository anchor differs")
  _require("22h + 24min + 50s" in julian_day, "V27 year-500 repository anchor differs")

  return ProvenanceCounts(
    runtime_branches=len(branches),
    nasa_v25_rows=len(nasa_partition["years"]),
    usno_v25_rows=len(usno_partition["years"]),
    bulletin_a_v25_rows=len(bulletin_a_partition["years"]),
    v27_relations=2,
  )


if __name__ == "__main__":
  print(verify_nasa_delta_t_provenance())
