#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from datetime import date
from pathlib import Path


PROBE_DIR = Path(__file__).resolve().parent
REPO_ROOT = PROBE_DIR.parents[3]
SOURCE_URL = "https://www.hko.gov.hk/en/gts/time/calendar/text/files/T{year}e.txt"
MONTH_START = re.compile(
  rb"^(\d{4})/(\d{1,2})/(\d{1,2})\s+(\d{1,2})(?:st|nd|rd|th)\s+Lunar\s+[Mm]onth\b",
  re.MULTILINE,
)


@dataclass(frozen=True)
class MonthStart:
  day: date
  month: int


@dataclass(frozen=True)
class ReconstructedYear:
  first_day: date
  leap_month: int
  month_lengths: tuple[int, ...]
  encoded: int


def sha256(data: bytes) -> str:
  return hashlib.sha256(data).hexdigest()


def source_bytes(year: int) -> bytes:
  return (PROBE_DIR / f"T{year}e.txt").read_bytes()


def parse_month_starts(year: int) -> tuple[MonthStart, ...]:
  starts = tuple(
    MonthStart(
      day=date(int(match[1]), int(match[2]), int(match[3])),
      month=int(match[4]),
    )
    for match in MONTH_START.finditer(source_bytes(year))
  )
  if not starts:
    raise ValueError(f"T{year}e.txt contains no lunar-month starts")
  return starts


def daily_row_anomaly(year: int) -> dict[str, object] | None:
  actual = {
    date(int(match[1]), int(match[2]), int(match[3]))
    for match in re.finditer(rb"^(\d{4})/(\d{1,2})/(\d{1,2})\s+", source_bytes(year), re.MULTILINE)
  }
  first = date(year, 1, 1)
  expected = {date.fromordinal(first.toordinal() + offset) for offset in range((date(year + 1, 1, 1) - first).days)}
  missing = tuple(sorted(expected - actual))
  extra = tuple(sorted(actual - expected))
  if not missing and not extra:
    return None
  return {
    "year": year,
    "missing_dates": [day.isoformat() for day in missing],
    "extra_dates": [day.isoformat() for day in extra],
  }


def reconstruct(year: int, starts_by_year: dict[int, tuple[MonthStart, ...]]) -> ReconstructedYear:
  starts = starts_by_year[year] + starts_by_year[year + 1]
  first_index = next(index for index, start in enumerate(starts) if start.day.year == year and start.month == 1)
  next_index = next(index for index in range(first_index + 1, len(starts)) if starts[index].month == 1)
  lunar_starts = starts[first_index : next_index + 1]
  labels = tuple(start.month for start in lunar_starts[:-1])

  repeated = tuple(labels[index] for index in range(1, len(labels)) if labels[index] == labels[index - 1])
  if len(repeated) > 1:
    raise ValueError(f"{year} has multiple repeated month labels: {labels}")
  leap_month = repeated[0] if repeated else 0
  expected = tuple(range(1, 13)) if leap_month == 0 else tuple(range(1, leap_month + 1)) + tuple(range(leap_month, 13))
  if labels != expected:
    raise ValueError(f"{year} has unexpected month labels: {labels}")

  month_lengths = tuple((right.day - left.day).days for left, right in zip(lunar_starts, lunar_starts[1:]))
  if any(length not in (29, 30) for length in month_lengths):
    raise ValueError(f"{year} has invalid month lengths: {month_lengths}")

  first_day = lunar_starts[0].day
  days_offset = (first_day - date(year, 1, 1)).days
  month_bits = sum(1 << index for index, length in enumerate(month_lengths) if length == 30)
  encoded = (days_offset << 17) | (leap_month << 13) | month_bits
  return ReconstructedYear(first_day, leap_month, month_lengths, encoded)


def repository_words() -> tuple[int, ...]:
  header = (REPO_ROOT / "src/calendar/lunar/algo1.hpp").read_text(encoding="utf-8")
  array_match = re.search(r"LUNAR_DATA\s*=\s*\{(?P<body>.*?)\n\};", header, re.DOTALL)
  if array_match is None:
    raise ValueError("algo1::LUNAR_DATA was not found")
  words = tuple(int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", array_match["body"]))
  if len(words) != 199:
    raise ValueError(f"expected 199 repository words, found {len(words)}")
  return words


def main() -> None:
  source_years = range(1901, 2101)
  missing_files = tuple(year for year in source_years if not (PROBE_DIR / f"T{year}e.txt").is_file())
  if missing_files:
    raise ValueError(f"missing source files: {missing_files}")
  daily_row_anomalies = tuple(
    anomaly for year in source_years if (anomaly := daily_row_anomaly(year)) is not None
  )

  starts_by_year = {year: parse_month_starts(year) for year in source_years}
  reconstructed = {year: reconstruct(year, starts_by_year) for year in range(1901, 2100)}
  repo_words = repository_words()
  mismatches = tuple(
    year for year, repo_word in zip(range(1901, 2100), repo_words) if reconstructed[year].encoded != repo_word
  )

  source_manifest_lines = tuple(
    f"{sha256(source_bytes(year))}  {SOURCE_URL.format(year=year)}" for year in source_years
  )
  source_manifest = "\n".join(source_manifest_lines) + "\n"
  (PROBE_DIR / "hko-source-sha256.txt").write_text(source_manifest, encoding="utf-8")

  comparison_lines = ["year\thko_word\trepository_word\tstatus\tfirst_day\tleap_month\tmonth_lengths"]
  for year, repo_word in zip(range(1901, 2100), repo_words):
    row = reconstructed[year]
    status = "MATCH" if row.encoded == repo_word else "MISMATCH"
    lengths = ",".join(str(length) for length in row.month_lengths)
    comparison_lines.append(
      f"{year}\t0x{row.encoded:06x}\t0x{repo_word:06x}\t{status}\t{row.first_day.isoformat()}\t"
      f"{row.leap_month}\t{lengths}"
    )
  comparison = "\n".join(comparison_lines) + "\n"
  (PROBE_DIR / "hko-comparison.tsv").write_text(comparison, encoding="utf-8")

  reconstructed_words = "\n".join(f"{year}\t0x{reconstructed[year].encoded:06x}" for year in range(1901, 2100)) + "\n"
  (PROBE_DIR / "hko-reconstructed-words.tsv").write_text(reconstructed_words, encoding="utf-8")

  summary = {
    "source_url_pattern": SOURCE_URL,
    "source_files": len(tuple(source_years)),
    "source_years": [1901, 2100],
    "boundary_only_years": [2100],
    "reconstructed_years": [1901, 2099],
    "reconstructed_count": len(reconstructed),
    "repository_word_count": len(repo_words),
    "agreement_count": len(reconstructed) - len(mismatches),
    "mismatches": mismatches,
    "daily_row_anomalies": daily_row_anomalies,
    "source_manifest_sha256": sha256(source_manifest.encode("utf-8")),
    "comparison_sha256": sha256(comparison.encode("utf-8")),
    "reconstructed_words_sha256": sha256(reconstructed_words.encode("utf-8")),
  }
  (PROBE_DIR / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
  print(json.dumps(summary, indent=2))


if __name__ == "__main__":
  main()
