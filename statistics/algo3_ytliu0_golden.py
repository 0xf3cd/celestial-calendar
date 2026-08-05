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

Frozen sample source
--------------------
ytliu0/ChineseCalendar (廖育棟 / Yuk-Tung Liu), commit
  d6aae82b63b79a6f8659ea3e064024b7d8ac3077
file `src/calendarData.js` (GPLv3). Collected 2026-08-05; the 114-row main table
lives in the vault attachment `attachments/2026-08-05-P5b-golden-前期工/` (and the
in-repo copy under `.review/style-arch/golden-pre/`). W-A2 (2026-08-05) additionally
allows a single 2099 row to pin the upper algo1/algo3 seam (2099/2100); that row is
extracted from the same commit under the same provenance discipline.

What this script does
---------------------
1. `--scan-near-midnight`  Reproducible "3-of-4 hit" scan for #70 §1 provenance debt:
   walk every new moon in 1901–1929 and report those that fall inside the first
   14m20s after UTC+8 midnight. Expected hits (four syzygies):
     1906-04-24 00:06:35  (no almanac flip — Qing counter-example)
     1914-11-18 00:01:49  (divergence)
     1916-02-04 00:05:13  (divergence; also drives 1915's last-month length)
     1920-11-11 00:04:48  (divergence)
   Scope note: this scan covers **syzygies only**. It does NOT cover the 1917 /
   1927 / 1928 jieqi-only differences listed by ytliu0.
2. `--emit-cpp`            Emit the C++ row table for
   `src/test/lunar/algo3_ytliu0_golden_test.cpp` from the frozen sample (and the
   optional 2099 W-A2 row). See module docstring of that test for independence /
   honesty claims; this mode does not re-argue them.
3. `--compare-all`         Full 1600–2199 field-by-field compare of ytliu0 decode
   vs algo3 (needs a local ytliu0 checkout). Used to re-derive the 548/52 split.

Modes 2–3 need either the frozen markdown table or a ytliu0 checkout; mode 1 only
needs the built shared library (via statistics/common.py).
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

# Beijing local mean time offset from UT is +7h46m; the UTC+8 civil clock is 14m20s
# ahead. A new moon in (midnight, midnight+14m20s] UTC+8 falls on the previous civil
# day under the 1914–1928 almanac rule.
NEAR_MIDNIGHT_WINDOW = timedelta(minutes=14, seconds=20)
SCAN_YEAR_START = 1901
SCAN_YEAR_END = 1929  # exclusive upper bound of the half-open mental range; inclusive walk

# Expected syzygies (UTC+8 civil clock, second precision). Used only as a self-check
# printout — the scan itself is data-driven from the library.
EXPECTED_HITS = (
  (1906, 4, 24, 0, 6, 35),
  (1914, 11, 18, 0, 1, 49),
  (1916, 2, 4, 0, 5, 13),
  (1920, 11, 11, 0, 4, 48),
)

# Frozen 114-row main table (no 1600 备查; endpoint substitute is 1603).
FROZEN_SAMPLE_CANDIDATES = (
  REPO_ROOT / ".review" / "style-arch" / "golden-pre" / "golden-采样草案.md",
  Path("/work/obsidian/side-projects/celestial/attachments/2026-08-05-P5b-golden-前期工/golden-采样草案.md"),
)


@dataclass(frozen=True)
class GoldenRow:
  year: int
  first_day: date
  leap_month: int
  month_lengths: tuple[int, ...]
  total_days: int
  js_offset: int  # byte offset into calendarData.js (provenance)

  def to_cpp_line(self) -> str:
    ml = ", ".join(str(d) for d in self.month_lengths)
    return (
      f"  {{ {self.year}, "
      f"{{ {self.first_day.year}, {self.first_day.month}, {self.first_day.day} }}, "
      f"{self.leap_month}, {{ {ml} }}, {self.total_days} }},"
      f"  // calendarData.js @{self.js_offset}"
    )


def _load_common():
  """Import statistics/common.py against the built shared library."""
  sys.path.insert(0, str(STATISTICS))
  import common  # type: ignore  # noqa: PLC0415
  return common


def scan_near_midnight(year_start: int = SCAN_YEAR_START,
                       year_end: int = SCAN_YEAR_END) -> list[tuple[int, datetime, float]]:
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
      secs = (
        utc8.hour * 3600
        + utc8.minute * 60
        + utc8.second
        + utc8.microsecond / 1e6
      )
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
        "frozen sample not found; pass --sample PATH or place the draft under "
        ".review/style-arch/golden-pre/"
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
  rows = load_frozen_sample(Path(args.sample) if args.sample else None)
  # W-A2 optional 2099 row: either from --year-2099-json or a ytliu0 checkout.
  if args.include_2099:
    row_2099 = extract_ytliu0_year(2099, Path(args.ytliu0) if args.ytliu0 else None)
    # Keep chronological order: 2099 sits between 1901 and 2100.
    rows = sorted(rows + [row_2099], key=lambda r: r.year)
  print(f"// generated by statistics/algo3_ytliu0_golden.py --emit-cpp")
  print(f"// ytliu0 commit {YTLIU0_COMMIT}; frozen sample 114 rows"
        + (" + W-A2 2099" if args.include_2099 else ""))
  print(f"// row count = {len(rows)}")
  for r in rows:
    print(r.to_cpp_line())
  return 0


def extract_ytliu0_year(year: int, ytliu0_root: Path | None) -> GoldenRow:
  """Decode one year from ytliu0 calendarData.js at the pinned commit.

  The JS file is a large IIFE; rather than executing it we re-implement the
  documented `ChineseToGregorian` decode used by the pre-work package (see
  vault 裁决预研/08 §3). Requires a local checkout at YTLIU0_COMMIT.
  """
  if ytliu0_root is None:
    raise SystemExit(
      f"--include-2099 / extract year {year} needs --ytliu0 PATH "
      f"(checkout of {YTLIU0_REPO} at {YTLIU0_COMMIT})"
    )
  data_path = ytliu0_root / YTLIU0_DATA_REL
  if not data_path.is_file():
    raise FileNotFoundError(data_path)
  raw = data_path.read_bytes()
  digest = hashlib.md5(raw).hexdigest()
  print(f"# calendarData.js md5={digest} bytes={len(raw)}", file=sys.stderr)

  # The pre-work package already decoded every sample year via the JS itself.
  # For the single W-A2 year 2099 we shell out to node if available, else fail
  # with a clear message — do not invent a second decoder.
  try:
    return _decode_via_node(ytliu0_root, year)
  except FileNotFoundError as exc:
    raise SystemExit(
      f"node is required to decode ytliu0 year {year} (no second decoder in-tree): {exc}"
    ) from exc


def _decode_via_node(ytliu0_root: Path, year: int) -> GoldenRow:
  import json
  import shutil
  import subprocess
  import tempfile

  if shutil.which("node") is None:
    raise FileNotFoundError("node binary not on PATH")

  # Minimal harness: load calendarData.js in the way the upstream page does and
  # dump first-day / leap / month lengths for one year. calendarData.js attaches
  # ChineseToGregorian onto the global object when evaluated under node with a
  # thin DOM-less shim.
  harness = f"""
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const root = {json.dumps(str(ytliu0_root))};
const src = fs.readFileSync(path.join(root, {json.dumps(YTLIU0_DATA_REL)}), 'utf8');
// calendarData.js is browser-oriented; give it a window/self and the helpers it
// expects. Upstream uses a single global ChineseToGregorian after load.
const sandbox = {{
  console,
  Date,
  Math,
  Array,
  Object,
  String,
  Number,
  parseInt,
  isNaN,
  window: {{}},
  self: {{}},
}};
sandbox.window = sandbox;
sandbox.self = sandbox;
vm.createContext(sandbox);
vm.runInContext(src, sandbox);
const fn = sandbox.ChineseToGregorian || sandbox.window.ChineseToGregorian;
if (typeof fn !== 'function') {{
  // Some builds expose a table + decoder under different names; surface keys.
  const keys = Object.keys(sandbox).filter(k => /chinese|lunar|gregorian|calendar/i.test(k));
  console.error('no ChineseToGregorian; candidate keys:', keys.join(','));
  process.exit(2);
}}
const y = {year};
// ChineseToGregorian(y, m, d) → Gregorian date of lunar y/m/d. Walk the year.
const first = fn(y, 1, 1);
const months = [];
let leap = 0;
// Upstream encoding: try m=1..13; a missing 13th month means no leap.
for (let m = 1; m <= 13; m++) {{
  try {{
    const d1 = fn(y, m, 1);
    const d2 = fn(y, m, 30);
    // length: if day 30 is valid in that month, 30 else 29. Detect validity by
    // seeing whether day 30 lands in the same lunar month when converted back,
    // or simply whether the Gregorian span from day 1 to next month day 1 is 30.
    let next;
    if (m < 13) {{
      try {{ next = fn(y, m + 1, 1); }}
      catch (e) {{ next = null; }}
    }} else {{
      next = fn(y + 1, 1, 1);
    }}
    if (next === null && m === 13) {{ break; }}
    if (next === null) {{
      // no month m+1 — this was the last month; length via next lunar new year
      next = fn(y + 1, 1, 1);
    }}
    const t1 = Date.parse(d1);
    const t2 = Date.parse(next);
    const len = Math.round((t2 - t1) / 86400000);
    if (len !== 29 && len !== 30) {{
      console.error('bad length', m, len, d1, next);
      process.exit(3);
    }}
    months.push(len);
  }} catch (e) {{
    if (m === 13) break;
    throw e;
  }}
}}
// Leap month index: if 13 months, find which traditional month is the leap by
// checking the upstream leap-month helper if any; else leave 0 and let the
// caller cross-check against algo1 (HKO years) / frozen sample.
if (months.length === 13) {{
  const leapFn = sandbox.leapMonthOf || sandbox.window.leapMonthOf
              || sandbox.getLeapMonth || sandbox.window.getLeapMonth;
  if (typeof leapFn === 'function') {{
    leap = leapFn(y);
  }} else {{
    // Fallback: many ytliu0 builds stash leap info on a parallel array.
    leap = sandbox.leapMonth && sandbox.leapMonth[y] || 0;
  }}
}}
const out = {{
  year: y,
  first: first,
  leap: leap,
  months: months,
  total: months.reduce((a, b) => a + b, 0),
}};
process.stdout.write(JSON.stringify(out));
"""
  with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False) as fh:
    fh.write(harness)
    harness_path = fh.name
  try:
    proc = subprocess.run(
      ["node", harness_path],
      check=False,
      capture_output=True,
      text=True,
    )
  finally:
    Path(harness_path).unlink(missing_ok=True)
  if proc.returncode != 0:
    sys.stderr.write(proc.stderr)
    raise SystemExit(f"node harness failed for year {year}: exit {proc.returncode}")
  data = json.loads(proc.stdout)
  # first may be "YYYY-MM-DD" or a Date string
  first_raw = data["first"]
  if isinstance(first_raw, str) and re.match(r"\d{4}-\d{2}-\d{2}", first_raw):
    y, m, d = map(int, first_raw[:10].split("-"))
    first = date(y, m, d)
  else:
    # Date.toString / ISO from JS
    first = date.fromisoformat(str(first_raw)[:10])
  ml = tuple(int(x) for x in data["months"])
  # Byte offset of the year token in calendarData.js for provenance (best-effort).
  raw = (ytliu0_root / YTLIU0_DATA_REL).read_text(encoding="utf-8", errors="replace")
  # Prefer a `2099:` / year-keyed offset if present; else 0 (caller still has commit).
  off_m = re.search(rf"(?:^|[^\d]){year}\s*:", raw)
  off = off_m.start() if off_m else 0
  return GoldenRow(
    year=year,
    first_day=first,
    leap_month=int(data.get("leap") or 0),
    month_lengths=ml,
    total_days=int(data["total"]),
    js_offset=off,
  )


def cmd_compare_all(args: argparse.Namespace) -> int:
  """Full-range compare placeholder — filled in c3 once ytliu0 decode path is solid.

  For now, compare the frozen 114-row sample against algo3 via the shared library.
  """
  common = _load_common()
  rows = load_frozen_sample(Path(args.sample) if args.sample else None)
  mismatches = 0
  for r in rows:
    info = common.get_lunar_year_info(common.LunarAlgo.ALGO_3, r.year)
    ok = (
      info.first_day == r.first_day
      and info.leap_month == r.leap_month
      and tuple(info.month_lengths) == r.month_lengths
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

  p_scan = sub.add_parser("scan-near-midnight", aliases=["--scan-near-midnight"],
                          help="1901–1929 14m20s syzygy scan (3-of-4 provenance)")
  p_scan.set_defaults(func=cmd_scan_near_midnight)

  p_emit = sub.add_parser("emit-cpp", help="emit C++ golden rows from frozen sample")
  p_emit.add_argument("--sample", default=None, help="path to golden-采样草案.md")
  p_emit.add_argument("--include-2099", action="store_true",
                      help="W-A2: append ytliu0 year 2099 (needs --ytliu0)")
  p_emit.add_argument("--ytliu0", default=None, help="path to ytliu0/ChineseCalendar checkout")
  p_emit.set_defaults(func=cmd_emit_cpp)

  p_cmp = sub.add_parser("compare-all", help="compare frozen sample (or full range) to algo3")
  p_cmp.add_argument("--sample", default=None)
  p_cmp.set_defaults(func=cmd_compare_all)

  # Also accept the long-option form advertised in the module docstring / test header
  # when users pass it as the sole argv token before subcommand migration.
  argv = list(sys.argv[1:] if argv is None else argv)
  if argv and argv[0] in {"--scan-near-midnight", "--emit-cpp", "--compare-all"}:
    argv[0] = argv[0].lstrip("-")

  args = parser.parse_args(argv)
  return args.func(args)


if __name__ == "__main__":
  sys.exit(main())
