#!/usr/bin/env python3
# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

"""algo3 ↔ ytliu0 external-oracle tooling for P5b (#70 §2 / D-P).

Source
------
ytliu0/ChineseCalendar (廖育棟 / Yuk-Tung Liu), commit
  d6aae82b63b79a6f8659ea3e064024b7d8ac3077
file `src/calendarData.js` (GPLv3; md5 pin below). Collected 2026-08-05.
W-A2 allows a single 2099 row for the upper algo1/algo3 seam (2099/2100).

Subcommands
-----------
1. scan-near-midnight  Reproducible near-midnight syzygy scan (1901–1929, first
   14m20s after UTC+8 midnight). Expected hits (4 syzygies; 3 flipped the almanac):
     1906-04-24 00:06:35  (no almanac flip — Qing counter-example)
     1914-11-18 00:01:49  (divergence)
     1916-02-04 00:05:13  (divergence; also drives 1915's last-month length)
     1920-11-11 00:04:48  (divergence)
   Syzygies only — does NOT cover 1917 / 1927 / 1928 jieqi-only differences.
2. emit-cpp            Emit C++ rows matching `YTLIU0_ROWS`. Prefer
   `--ytliu0 <checkout>` (decodes *rows for* the embedded GOLDEN_YEARS set
   from calendarData.js — the year list lives in this script, not in the JS).
   Optional `--sample` frozen markdown is an author shortcut; with it,
   `--include-2099` also needs `--ytliu0` to append the W-A2 seam year.
3. compare-sample      If a frozen markdown is available (author machine /
   --sample), compare those rows to algo3. Does **not** re-derive 548/52 and
   does not cover W-A2 2099 unless the markdown includes it. Clone readers
   re-verify via the C++ golden tests, not this mode.

Mode 1 needs the built shared library. Mode 2 prefers --ytliu0 at the pinned
commit (GOLDEN_YEARS embedded). Mode 3 needs a frozen markdown path.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from dataclasses import dataclass
from datetime import date, datetime, timedelta
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
STATISTICS = Path(__file__).resolve().parent

# Pinned at the 2026-08-05 shallow clone HEAD (brief v2 / D-A13).
YTLIU0_COMMIT = "d6aae82b63b79a6f8659ea3e064024b7d8ac3077"
YTLIU0_REPO = "https://github.com/ytliu0/ChineseCalendar"
YTLIU0_DATA_REL = "src/calendarData.js"
YTLIU0_DATA_MD5 = "6c9649f384d178918d9cb4618f7d3e98"
YTLIU0_DATA_BYTES = 695460

# Beijing local mean time ≈ UT+7:46 (more precisely 7h45m40s at 116°25′E); UTC+8 is
# 14m20s ahead. A new moon with 0 ≤ secs_after_midnight < 14m20s UTC+8 falls on the
# previous civil day under the 1914–1928 almanac rule.
NEAR_MIDNIGHT_WINDOW = timedelta(minutes=14, seconds=20)
SCAN_YEAR_START = 1901
SCAN_YEAR_END = 1929  # inclusive walk via range(start, end + 1)

# Expected syzygies (UTC+8 civil clock, second precision). Used only as a self-check
# printout — the scan itself is data-driven from the library.
EXPECTED_HITS = (
  (1906, 4, 24, 0, 6, 35),
  (1914, 11, 18, 0, 1, 49),
  (1916, 2, 4, 0, 5, 13),
  (1920, 11, 11, 0, 4, 48),
)

# Year set for the C++ golden table (SSOT also in algo3_ytliu0_golden_test.cpp).
# Readers regenerate with: emit-cpp --ytliu0 <checkout-at-YTLIU0_COMMIT>
GOLDEN_YEARS = (
  1603,
  1607,
  1618,
  1625,
  1633,
  1639,
  1647,
  1652,
  1656,
  1660,
  1666,
  1669,
  1674,
  1678,
  1681,
  1688,
  1692,
  1696,
  1700,
  1705,
  1710,
  1713,
  1717,
  1722,
  1725,
  1730,
  1734,
  1737,
  1741,
  1745,
  1749,
  1752,
  1756,
  1761,
  1765,
  1769,
  1773,
  1776,
  1782,
  1786,
  1792,
  1795,
  1800,
  1805,
  1808,
  1812,
  1816,
  1819,
  1823,
  1827,
  1832,
  1835,
  1839,
  1844,
  1847,
  1851,
  1855,
  1858,
  1863,
  1867,
  1871,
  1874,
  1878,
  1883,
  1886,
  1890,
  1894,
  1900,
  1901,
  2099,
  2100,
  2101,
  2103,
  2105,
  2108,
  2110,
  2112,
  2115,
  2117,
  2120,
  2122,
  2124,
  2127,
  2129,
  2131,
  2133,
  2135,
  2137,
  2140,
  2142,
  2144,
  2147,
  2149,
  2151,
  2154,
  2156,
  2159,
  2161,
  2163,
  2165,
  2167,
  2169,
  2171,
  2172,
  2175,
  2177,
  2180,
  2182,
  2184,
  2187,
  2189,
  2191,
  2194,
  2196,
  2199,
)

# Optional author-side frozen markdown (not in the public tree; --sample overrides).
FROZEN_SAMPLE_CANDIDATES = (REPO_ROOT / ".review" / "style-arch" / "golden-pre" / "golden-采样草案.md",)


@dataclass(frozen=True)
class GoldenRow:
  year: int
  first_day: date
  leap_month: int
  month_lengths: tuple[int, ...]
  total_days: int
  js_offset: int  # byte offset into calendarData.js (provenance)

  def to_cpp_line(self) -> str:
    """Emit one row matching `YTLIU0_ROWS` in algo3_ytliu0_golden_test.cpp."""
    ml = ", ".join(str(d) for d in self.month_lengths)
    y, m, d = self.first_day.year, self.first_day.month, self.first_day.day
    return (
      f"  {{ {self.year}, std::chrono::year {{ {y} }} / {m} / {d}, "
      f"{self.leap_month}, {{ {ml} }} }},  "
      f"// js@{self.js_offset} total={self.total_days}"
    )


def _load_common():
  """Import statistics/common.py against the built shared library."""
  sys.path.insert(0, str(STATISTICS))
  import common  # type: ignore  # noqa: PLC0415

  return common


def scan_near_midnight(
  year_start: int = SCAN_YEAR_START, year_end: int = SCAN_YEAR_END
) -> list[tuple[int, datetime, float]]:
  """Return (year, utc8_datetime, seconds_after_midnight) for every hit."""
  common = _load_common()
  hits: list[tuple[int, datetime, float]] = []
  window_s = NEAR_MIDNIGHT_WINDOW.total_seconds()
  for year in range(year_start, year_end + 1):
    nm = common.new_moons_in_year(year)
    for moment in nm.new_moon_moments:
      # common.py renders new moons in UT1 civil clock; add 8h for UTC+8 wall time
      # (the notebooks use the same convention; |UT1−UTC| is sub-second here).
      utc8 = moment + timedelta(hours=8)
      secs = utc8.hour * 3600 + utc8.minute * 60 + utc8.second + utc8.microsecond / 1e6
      if secs < window_s:
        hits.append((year, utc8, secs))
  return hits


def cmd_scan_near_midnight(_: argparse.Namespace) -> int:
  hits = scan_near_midnight()
  print(f"# near-midnight syzygy scan {SCAN_YEAR_START}–{SCAN_YEAR_END}")
  print(f"# window = first {NEAR_MIDNIGHT_WINDOW} after UTC+8 midnight")
  print(f"# ytliu0 mechanism reference commit {YTLIU0_COMMIT}")
  print(f"# hits = {len(hits)} (expected 4: 1906 counter-example + 1914/1916/1920)")
  print("# scope: syzygies only; does NOT cover 1917/1927/1928 jieqi-only diffs")
  print()
  for year, utc8, secs in hits:
    print(f"{year}\t{utc8.isoformat(sep=' ', timespec='milliseconds')}\t+{secs:.3f}s")
  print()
  # Self-check against the frozen expected list (second precision).
  got = {(y, u.month, u.day, u.hour, u.minute, u.second) for y, u, _ in hits}
  exp = set(EXPECTED_HITS)
  if got != exp:
    print("SELF-CHECK MISMATCH", file=sys.stderr)
    print(f"  got: {sorted(got)}", file=sys.stderr)
    print(f"  exp: {sorted(exp)}", file=sys.stderr)
    return 1
  print("SELF-CHECK OK: 4/4 expected syzygies, of which 3 are the known divergences")
  print("  (1906 did not flip the almanac — Qing counter-example / 外推证伪点)")
  return 0


_ROW_RE = re.compile(
  r"^\|\s*(\d{4})\s*\|\s*([^|]+)\|\s*(\d{4})-(\d{2})-(\d{2})\s*\|\s*(\d+)\s*\|\s*"
  r"([0-9,]+)\s*\|\s*(\d+)\s*\|\s*偏移\s*(\d+)\s*\|",
  re.M,
)


def load_frozen_sample(path: Path | None = None) -> list[GoldenRow]:
  """Parse the 114-row main table from the frozen markdown draft."""
  if path is None:
    for candidate in FROZEN_SAMPLE_CANDIDATES:
      if candidate.is_file():
        path = candidate
        break
    else:
      raise FileNotFoundError(
        "frozen sample not found; pass --sample PATH or place the draft under .review/style-arch/golden-pre/"
      )
  text = path.read_text(encoding="utf-8")
  rows: list[GoldenRow] = []
  for m in _ROW_RE.finditer(text):
    year = int(m.group(1))
    y, mo, d = int(m.group(3)), int(m.group(4)), int(m.group(5))
    leap = int(m.group(6))
    ml = tuple(int(x) for x in m.group(7).split(","))
    total = int(m.group(8))
    off = int(m.group(9))
    if sum(ml) != total:
      raise ValueError(f"year {year}: sum(month_lengths)={sum(ml)} != total_days={total}")
    rows.append(
      GoldenRow(
        year=year,
        first_day=date(y, mo, d),
        leap_month=leap,
        month_lengths=ml,
        total_days=total,
        js_offset=off,
      )
    )
  if len(rows) != 114:
    raise ValueError(f"expected 114 main-table rows, got {len(rows)} from {path}")
  if any(r.year == 1600 for r in rows):
    raise ValueError("1600 备查 row must not be in the main table (use 1603 substitute)")
  return rows


def cmd_emit_cpp(args: argparse.Namespace) -> int:
  """Emit rows for YTLIU0_ROWS. Prefer --ytliu0; --sample is optional author path."""
  # Preferred path: decode every year in the embedded GOLDEN_YEARS set (already
  # includes W-A2 2099). --sample is ignored when --ytliu0 alone drives the set.
  if args.ytliu0 and not args.sample:
    root = Path(args.ytliu0)
    rows = [extract_ytliu0_year(y, root) for y in GOLDEN_YEARS]
    src_note = f"ytliu0 checkout; {len(GOLDEN_YEARS)} GOLDEN_YEARS"
    # --include-2099 is a no-op here: 2099 is already in GOLDEN_YEARS.
  else:
    rows = load_frozen_sample(Path(args.sample) if args.sample else None)
    src_note = f"frozen sample {len(rows)} rows"
    if args.include_2099:
      if not args.ytliu0:
        print(
          "emit-cpp: --include-2099 on the frozen-sample path requires "
          "--ytliu0 <checkout> (cannot decode 2099 without calendarData.js)",
          file=sys.stderr,
        )
        return 2
      if any(r.year == 2099 for r in rows):
        src_note += " (2099 already in sample)"
      else:
        row_2099 = extract_ytliu0_year(2099, Path(args.ytliu0))
        rows = sorted(rows + [row_2099], key=lambda r: r.year)
        src_note += " + W-A2 2099 from ytliu0"
  print("// generated by statistics/algo3_ytliu0_golden.py emit-cpp")
  print(f"// ytliu0 commit {YTLIU0_COMMIT}; {src_note}")
  print(f"// row count = {len(rows)}")
  for r in rows:
    print(r.to_cpp_line())
  return 0


def _parse_calendar_data_js(data_path: Path) -> dict[int, list[int]]:
  """Parse ChineseToGregorian() table: [year, 12 month-starts, leap_start, leap_mon, total]."""
  raw = data_path.read_text(encoding="utf-8")
  # Each row is a JS array literal of integers.
  rows: dict[int, list[int]] = {}
  for m in re.finditer(r"\[(-?\d+(?:,-?\d+){15})\]", raw):
    nums = [int(x) for x in m.group(1).split(",")]
    year = nums[0]
    rows[year] = nums
  return rows


def _row_byte_offset(data_path: Path, year: int) -> int:
  raw = data_path.read_text(encoding="utf-8")
  m = re.search(rf"\[{year},", raw)
  if m is None:
    raise KeyError(f"year {year} not found in {data_path}")
  return m.start()


def decode_ytliu0_row(nums: list[int], js_offset: int = 0) -> GoldenRow:
  """nums = [year, 12×doy starts, leap_start, leap_month, total_days]. doy is 1-based."""
  if len(nums) != 16:
    raise ValueError(f"expected 16 fields, got {len(nums)}: {nums[:4]}…")
  year = nums[0]
  starts12 = nums[1:13]
  leap_start = nums[13]
  leap = nums[14]
  total = nums[15]
  starts: list[int] = []
  for i, s in enumerate(starts12, start=1):
    starts.append(s)
    if leap != 0 and i == leap:
      starts.append(leap_start)
  starts.append(starts[0] + total)
  ml = tuple(starts[i + 1] - starts[i] for i in range(len(starts) - 1))
  if any(x not in (29, 30) for x in ml):
    raise ValueError(f"year {year}: bad month lengths {ml}")
  if sum(ml) != total:
    raise ValueError(f"year {year}: sum(ml)={sum(ml)} != total={total}")
  first = date(year, 1, 1) + timedelta(days=starts12[0] - 1)
  return GoldenRow(
    year=year,
    first_day=first,
    leap_month=leap,
    month_lengths=ml,
    total_days=total,
    js_offset=js_offset,
  )


def extract_ytliu0_year(year: int, ytliu0_root: Path | None) -> GoldenRow:
  """Decode one year from ytliu0 calendarData.js at the pinned commit."""
  if ytliu0_root is None:
    raise SystemExit(f"extract year {year} needs --ytliu0 PATH (checkout of {YTLIU0_REPO} at {YTLIU0_COMMIT})")
  data_path = ytliu0_root / YTLIU0_DATA_REL
  if not data_path.is_file():
    raise FileNotFoundError(data_path)
  raw = data_path.read_bytes()
  digest = hashlib.md5(raw).hexdigest()
  if len(raw) != YTLIU0_DATA_BYTES or digest != YTLIU0_DATA_MD5:
    raise SystemExit(
      f"calendarData.js fingerprint mismatch: bytes={len(raw)} md5={digest}; "
      f"expected bytes={YTLIU0_DATA_BYTES} md5={YTLIU0_DATA_MD5} at commit {YTLIU0_COMMIT}"
    )
  print(f"# calendarData.js md5={digest} bytes={len(raw)} commit={YTLIU0_COMMIT}", file=sys.stderr)
  table = _parse_calendar_data_js(data_path)
  if year not in table:
    raise KeyError(f"year {year} not in {data_path}")
  return decode_ytliu0_row(table[year], _row_byte_offset(data_path, year))


def cmd_compare_sample(args: argparse.Namespace) -> int:
  """Compare the frozen 114-row sample against algo3 (shared library).

  Not a full 1600–2199 oracle re-derivation of the 548/52 split.
  """
  common = _load_common()
  rows = load_frozen_sample(Path(args.sample) if args.sample else None)
  mismatches = 0
  for r in rows:
    info = common.get_lunar_year_info(common.LunarAlgo.ALGO_3, r.year)
    ok = (
      info.first_day == r.first_day and info.leap_month == r.leap_month and tuple(info.month_lengths) == r.month_lengths
    )
    if not ok:
      mismatches += 1
      print(
        f"MISMATCH {r.year}: golden=({r.first_day},{r.leap_month},{r.month_lengths}) "
        f"algo3=({info.first_day},{info.leap_month},{info.month_lengths})"
      )
  print(f"frozen-sample vs algo3: {len(rows) - mismatches}/{len(rows)} match, {mismatches} mismatch")
  # By construction of the sample the frozen 114 must all match today.
  return 0 if mismatches == 0 else 1


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  sub = parser.add_subparsers(dest="cmd", required=True)

  p_scan = sub.add_parser("scan-near-midnight", help="1901–1929 14m20s syzygy scan (4 hits / 3 divergences)")
  p_scan.set_defaults(func=cmd_scan_near_midnight)

  p_emit = sub.add_parser("emit-cpp", help="emit C++ golden rows matching YTLIU0_ROWS")
  p_emit.add_argument("--sample", default=None, help="path to golden-采样草案.md")
  p_emit.add_argument("--include-2099", action="store_true", help="W-A2: append ytliu0 year 2099 (needs --ytliu0)")
  p_emit.add_argument("--ytliu0", default=None, help="path to ytliu0/ChineseCalendar checkout")
  p_emit.set_defaults(func=cmd_emit_cpp)

  p_cmp = sub.add_parser(
    "compare-sample",
    help="compare frozen markdown sample to algo3 (not full-range 548/52)",
  )
  p_cmp.add_argument("--sample", default=None)
  p_cmp.set_defaults(func=cmd_compare_sample)
  # Legacy name; omit help= so argparse does not print a sentinel string.
  p_legacy = sub.add_parser("compare-all")
  p_legacy.add_argument("--sample", default=None)
  p_legacy.set_defaults(func=cmd_compare_sample)

  args = parser.parse_args(argv)
  return args.func(args)


if __name__ == "__main__":
  sys.exit(main())
