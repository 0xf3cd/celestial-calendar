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

"""Verify the retained HKO lunar-table identity record offline.

The raw HKO responses remain outside the repository. The committed record preserves their
hashes, the exact historical extractor, its outputs, and the 199-word runtime relation.
The extractor is historical evidence, not an in-place regeneration tool. Its tracked path is five
levels below the repository root, while its `parents[3]` assumption requires replaying it with the
retained responses from a directory four levels below the root.
Numerical identity does not settle redistribution permission; that question remains open.
"""

import csv
import hashlib
import io
import json
import re

from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Final


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
HKO_ROOT: Final[Path] = REPO_ROOT / "src" / "test" / "provenance" / "hko" / "2026-08-25"
HKO_SOURCE_URL: Final[str] = "https://www.hko.gov.hk/en/gts/time/calendar/text/files/T{year}e.txt"
HKO_EXTRACTOR_SHA256: Final[str] = "bfb8948025e4e5e78cea2f6f39061f7a181588bb67055d7cbfd01d47f855975f"
HKO_ARTIFACT_SHA256: Final[dict[str, str]] = {
  "extract.py": HKO_EXTRACTOR_SHA256,
  "hko-source-sha256.txt": "8f3980c799312490a516725a038162f6c16e6521515bfb196c4fcd6ab0baad1e",
  "hko-comparison.tsv": "aee2f24dc535fe2fd282e7d23e10acdd12dca9d9a8024584e50ac6a1deaa1d79",
  "hko-reconstructed-words.tsv": "ecca33a7bb3f77dfd5fb2dba75fc30391a9451d3e9fb00800488a0ac09064542",
  "summary.json": "5f43d03a904cbafcb95da58b62e0ce75cea9910c8e36591091b30d80867ab998",
}
COMPARISON_FIELDS: Final[list[str]] = [
  "year",
  "hko_word",
  "repository_word",
  "status",
  "first_day",
  "leap_month",
  "month_lengths",
]


@dataclass(frozen=True)
class IdentityCounts:
  source_hashes: int
  reconstructed_words: int
  algo1_words: int
  algo3_words: int
  anomalies: int


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _read_artifacts(hko_root: Path, expected_hashes: dict[str, str]) -> dict[str, str]:
  _require(set(expected_hashes) == set(HKO_ARTIFACT_SHA256), "HKO artifact hash inventory differs")
  # Reject subdirectories too, so nested raw responses cannot bypass the exact inventory.
  inventory = {path.name for path in hko_root.iterdir()}
  _require(inventory == set(expected_hashes), f"HKO artifact inventory differs: {sorted(inventory)}")

  artifacts = {}
  for name, expected in expected_hashes.items():
    data = (hko_root / name).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    _require(digest == expected, f"HKO artifact hash mismatch for {name}: {digest}")
    artifacts[name] = data.decode("utf-8")
  return artifacts


def _source_hashes(text: str) -> list[str]:
  lines = text.splitlines()
  _require(len(lines) == 200, f"HKO source hash rows={len(lines)}, expected 200")
  hashes = []
  for expected_year, line in zip(range(1901, 2101), lines, strict=True):
    match = re.fullmatch(r"([0-9a-f]{64})  (https://\S+)", line)
    _require(match is not None, f"HKO source hash row {expected_year} is malformed")
    _require(match[2] == HKO_SOURCE_URL.format(year=expected_year), f"HKO source year order differs at {expected_year}")
    hashes.append(match[1])
  return hashes


def _recorded_words(text: str) -> list[int]:
  lines = text.splitlines()
  _require(len(lines) == 199, f"HKO reconstructed rows={len(lines)}, expected 199")
  words = []
  for expected_year, line in zip(range(1901, 2100), lines, strict=True):
    match = re.fullmatch(r"(\d{4})\t(0x[0-9a-f]{6})", line)
    _require(match is not None, f"HKO reconstructed row {expected_year} is malformed")
    _require(int(match[1]) == expected_year, f"HKO reconstructed year order differs at {expected_year}")
    words.append(int(match[2], 16))
  return words


def _comparison_words(text: str) -> list[int]:
  reader = csv.DictReader(io.StringIO(text), delimiter="\t")
  _require(reader.fieldnames == COMPARISON_FIELDS, "HKO comparison columns differ")
  rows = list(reader)
  _require(len(rows) == 199, f"HKO comparison rows={len(rows)}, expected 199")

  words = []
  for expected_year, row in zip(range(1901, 2100), rows, strict=True):
    year = int(row["year"])
    _require(year == expected_year, f"HKO comparison year order differs at {expected_year}")
    _require(row["status"] == "MATCH", f"HKO comparison status differs for {year}")
    hko_word = int(row["hko_word"], 16)
    repository_word = int(row["repository_word"], 16)
    _require(hko_word == repository_word, f"HKO comparison word differs for {year}")

    first_day = date.fromisoformat(row["first_day"])
    leap_month = int(row["leap_month"])
    month_lengths = tuple(int(value) for value in row["month_lengths"].split(","))
    _require(0 <= leap_month <= 12, f"HKO leap month differs for {year}")
    _require(len(month_lengths) == 12 + bool(leap_month), f"HKO month count differs for {year}")
    _require(all(length in (29, 30) for length in month_lengths), f"HKO month length differs for {year}")
    days_offset = (first_day - date(year, 1, 1)).days
    month_bits = sum(1 << index for index, length in enumerate(month_lengths) if length == 30)
    reconstructed = (days_offset << 17) | (leap_month << 13) | month_bits
    _require(reconstructed == hko_word, f"HKO comparison encoding differs for {year}")
    words.append(hko_word)
  return words


def _runtime_words(path: Path, expected_count: int, label: str) -> list[int]:
  text = path.read_text(encoding="utf-8")
  match = re.search(r"LUNAR_DATA\s*=\s*\{(?P<body>.*?)\n\};", text, re.DOTALL)
  _require(match is not None, f"{label} LUNAR_DATA was not found")
  words = [int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", match["body"])]
  _require(len(words) == expected_count, f"{label} words={len(words)}, expected {expected_count}")
  return words


def _verify_summary(summary_text: str) -> None:
  expected = {
    "source_url_pattern": HKO_SOURCE_URL,
    "source_files": 200,
    "source_years": [1901, 2100],
    "boundary_only_years": [2100],
    "reconstructed_years": [1901, 2099],
    "reconstructed_count": 199,
    "repository_word_count": 199,
    "agreement_count": 199,
    "mismatches": [],
    "daily_row_anomalies": [
      {
        "year": 2069,
        "missing_dates": ["2069-12-30"],
        "extra_dates": [],
      }
    ],
    "source_manifest_sha256": HKO_ARTIFACT_SHA256["hko-source-sha256.txt"],
    "comparison_sha256": HKO_ARTIFACT_SHA256["hko-comparison.tsv"],
    "reconstructed_words_sha256": HKO_ARTIFACT_SHA256["hko-reconstructed-words.tsv"],
  }
  _require(json.loads(summary_text) == expected, "HKO summary differs")


def _verify_source_markings(repo_root: Path) -> None:
  algo1 = (repo_root / "src/calendar/lunar/algo1.hpp").read_text(encoding="utf-8")
  algo3 = (repo_root / "src/calendar/lunar/algo3.hpp").read_text(encoding="utf-8")
  _require(
    "All 199 words for 1901-2099 reproduce exactly from the retained HKO identity record" in algo1,
    "algo1 HKO identity marking differs",
  )
  _require(
    "Numerical identity does not settle redistribution permission; that question remains open." in algo1,
    "algo1 HKO permission boundary differs",
  )
  _require(
    "Their retained HKO identity record is `src/test/provenance/hko/2026-08-25/`" in algo3,
    "algo3 HKO identity marking differs",
  )
  _require(
    "Exact identity does not settle redistribution permission, which remains open." in algo3,
    "algo3 HKO permission boundary differs",
  )


def verify_hko_identity(
  repo_root: Path = REPO_ROOT,
  hko_root: Path = HKO_ROOT,
  artifact_sha256: dict[str, str] | None = None,
) -> IdentityCounts:
  artifacts = _read_artifacts(hko_root, HKO_ARTIFACT_SHA256 if artifact_sha256 is None else artifact_sha256)
  source_hashes = _source_hashes(artifacts["hko-source-sha256.txt"])
  reconstructed_words = _recorded_words(artifacts["hko-reconstructed-words.tsv"])
  comparison_words = _comparison_words(artifacts["hko-comparison.tsv"])
  _require(comparison_words == reconstructed_words, "HKO comparison and reconstructed words differ")
  _verify_summary(artifacts["summary.json"])

  algo1_words = _runtime_words(repo_root / "src/calendar/lunar/algo1.hpp", 199, "algo1")
  algo3_words = _runtime_words(repo_root / "src/calendar/lunar/algo3.hpp", 600, "algo3")[301:500]
  _require(algo1_words == reconstructed_words, "algo1 HKO words differ")
  _require(algo3_words == reconstructed_words, "algo3 HKO words differ")
  _verify_source_markings(repo_root)

  return IdentityCounts(
    source_hashes=len(source_hashes),
    reconstructed_words=len(reconstructed_words),
    algo1_words=len(algo1_words),
    algo3_words=len(algo3_words),
    anomalies=1,
  )
