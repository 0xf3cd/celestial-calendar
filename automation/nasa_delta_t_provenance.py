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


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
NASA_ROOT: Final[Path] = REPO_ROOT / "src" / "test" / "provenance" / "nasa" / "tp-2006-214141"
NASA_RECORD: Final[Path] = NASA_ROOT / "delta_t.json"
NASA_ACKNOWLEDGMENT: Final[Path] = NASA_ROOT / "ACKNOWLEDGMENT.txt"
NASA_RECORD_SHA256: Final[str] = "9539cfe87e0abc622b18b502e4838a5e6e1e1b163cb84825e7373431cf53bc13"
NASA_ACKNOWLEDGMENT_SHA256: Final[str] = "1f772efcfc102cc2e2bccdefa3720cbbba1c0a7835173c0af60c4efdb9d31670"
NASA_NOTICE_APPLICABILITY: Final[str] = (
  "the NASA/TP-2006-214141 Delta-T polynomial material in src/astro/delta_t.hpp and the "
  "NASA-sourced historical Delta-T validation values in src/test"
)


@dataclass(frozen=True)
class ProvenanceCounts:
  runtime_branches: int
  nasa_v25_rows: int
  usno_v25_rows: int
  inherited_v25_rows: int
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


def verify_nasa_delta_t_provenance(
  repo_root: Path = REPO_ROOT,
  nasa_root: Path = NASA_ROOT,
  record_sha256: str = NASA_RECORD_SHA256,
  acknowledgment_sha256: str = NASA_ACKNOWLEDGMENT_SHA256,
  notice_applicability: str = NASA_NOTICE_APPLICABILITY,
) -> ProvenanceCounts:
  record_path = nasa_root / "delta_t.json"
  acknowledgment_path = nasa_root / "ACKNOWLEDGMENT.txt"
  record = json.loads(_read_pinned(record_path, record_sha256, "NASA provenance record"))
  acknowledgment = _read_pinned(acknowledgment_path, acknowledgment_sha256, "NASA acknowledgment")

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
    notice_applicability == "the NASA/TP-2006-214141 Delta-T polynomial material in src/astro/delta_t.hpp and the "
    "NASA-sourced historical Delta-T validation values in src/test",
    "NASA notice applicability was broadened",
  )

  runtime = record["runtime_relation"]
  _require(
    runtime["source_location"] == "Section 2.7, equations (11)-(25), printed pages 14-16",
    "NASA locator differs",
  )
  branches = runtime["branches"]
  _require([branch["equation"] for branch in branches] == list(range(11, 26)), "NASA equation inventory differs")
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

  delta_t = _normalized((repo_root / runtime["repository_path"]).read_text(encoding="utf-8"))
  algo2 = delta_t[delta_t.index("namespace algo2 {") : delta_t.index("} // namespace algo2")]
  for branch in branches:
    snippet = branch["cpp"]
    _require(algo2.count(snippet) == 1, f"NASA equation ({branch['equation']}) repository relation differs")
  _require(
    "NASA/TP-2006-214141, Section 2.7, equations (11)-(25)" in algo2,
    "NASA runtime citation differs",
  )

  retired_relations = record["retired_runtime_relations"]
  _require(len(retired_relations) == 1, "NASA retired runtime relation inventory differs")
  retired_lunar = retired_relations[0]
  _require(
    retired_lunar["former_source_url"] == "https://eclipse.gsfc.nasa.gov/SEcat5/deltatpoly.html",
    "NASA retired lunar-table source differs",
  )
  _require(retired_lunar["regenerated_entries"] == 401, "NASA retired lunar-table regeneration count differs")
  _require(retired_lunar["changed_entries"] == 0, "NASA retired lunar-table regeneration was not byte-identical")
  _require(
    [(item["years"], item["entries"]) for item in retired_lunar["replacement_relations"]]
    == [("1600-1900", 301), ("2100-2199", 100)],
    "NASA retired lunar-table replacement partition differs",
  )
  lunar_table = (repo_root / retired_lunar["repository_path"]).read_text(encoding="utf-8")
  _require(retired_lunar["former_source_url"] not in lunar_table, "NASA retired lunar-table citation returned")
  _require(
    "all 401 non-HKO entries equal a full live-algo2 regeneration" in lunar_table,
    "NASA retired lunar-table replacement relation differs",
  )
  lunar_test = (repo_root / retired_lunar["verification"]["repository_test"]).read_text(encoding="utf-8")
  _require(
    "TEST(LunarAlgo3, BakedMatchesLiveAlgo2)" in lunar_test and "ASSERT_EQ(401, checked)" in lunar_test,
    "NASA retired lunar-table full-regeneration gate differs",
  )

  validation = record["validation_relations"]
  v25 = validation["v25"]
  partitions = v25["partitions"]
  _require(len(partitions) == 3, "V25 source partition count differs")
  nasa_partition, usno_partition, inherited_partition = partitions
  _require(nasa_partition["source"] == "NASA/TP-2006-214141 Table 2-2", "V25 NASA source differs")
  _require(usno_partition["source"] == "USNO deltat.data", "V25 USNO source differs")
  _require(inherited_partition["source"] == "inherited from unresolved R09", "V25 inherited source differs")
  _require(inherited_partition["relation"] == "not closed by A3.5", "V25 inherited condition was closed")
  _require(nasa_partition["years"] == list(range(1955, 2010, 5)), "V25 NASA year partition differs")
  _require(usno_partition["years"] == [2010, 2014], "V25 USNO year partition differs")
  _require(inherited_partition["years"] == list(range(2015, 2027)), "V25 inherited year partition differs")

  helper_text = (repo_root / v25["repository_path"]).read_text(encoding="utf-8")
  repository_rows = _delta_t_rows(helper_text)
  covered_years = set(nasa_partition["years"]) | set(usno_partition["years"]) | set(inherited_partition["years"])
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
  _require("NASA/TP-2006-214141 Table 2-2" in helper_text, "V25 NASA citation differs")
  _require("USNO deltat.data" in helper_text, "V25 USNO citation differs")
  _require("2015+ entries" in helper_text and "R09" in helper_text, "V25 inherited R09 boundary differs")

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
    inherited_v25_rows=len(inherited_partition["years"]),
    v27_relations=2,
  )


if __name__ == "__main__":
  print(verify_nasa_delta_t_provenance())
