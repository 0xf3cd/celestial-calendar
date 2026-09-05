# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import importlib.util
import json

from datetime import datetime
from types import ModuleType
from typing import Final, List, Tuple

from . import paths
from .ctypes_smoke import load_common
from .utils import green_print, red_print, yellow_print


def load_exporter() -> ModuleType:
  """Import `toolbox/jieqi_table.py` from a file location rather than via `sys.path`
  (same reason as `load_common`: a generic module name invites collisions)."""
  exporter_py = paths.proj_root() / "toolbox" / "jieqi_table.py"
  spec = importlib.util.spec_from_file_location("jieqi_table", exporter_py)
  if spec is None or spec.loader is None:
    raise RuntimeError(f"Cannot load module spec from {exporter_py}")
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


# Independent transcription of the name table in `src/calendar/jieqi.hpp`, positioned by
# `to_index` (0 = 立春). The exporter echoes `get_jieqi_name`; this table is what pins the
# idx ↔ name mapping from outside that call -- a swapped mapping passes every numeric check
# with every label wrong (#164).
JIEQI_NAMES_ZH: Final[List[str]] = [
  "立春",
  "雨水",
  "惊蛰",
  "春分",
  "清明",
  "谷雨",
  "立夏",
  "小满",
  "芒种",
  "夏至",
  "小暑",
  "大暑",
  "立秋",
  "处暑",
  "白露",
  "秋分",
  "寒露",
  "霜降",
  "立冬",
  "小雪",
  "大雪",
  "冬至",
  "小寒",
  "大寒",
]

# Retained material boundary (V04): the 48 HKO reference instants below remain under their source
# terms and outside the project MIT grant; upstream permission was not obtained and is not claimed.
# Chain-external anchors: HKO almanac wall clocks (HKT = UTC+8, minute precision), the first
# and last years of the HKO window. Transcribed from HKO_ROWS in
# `src/test/jieqi_golden_test.cpp`; entry is the calendar-order index (0 = 小寒).
HKO_ANCHORS: Final[List[Tuple[int, int, int, int, int, int]]] = [
  (2022, 0, 1, 5, 17, 14),
  (2022, 1, 1, 20, 10, 39),
  (2022, 2, 2, 4, 4, 51),
  (2022, 3, 2, 19, 0, 43),
  (2022, 4, 3, 5, 22, 44),
  (2022, 5, 3, 20, 23, 33),
  (2022, 6, 4, 5, 3, 20),
  (2022, 7, 4, 20, 10, 24),
  (2022, 8, 5, 5, 20, 26),
  (2022, 9, 5, 21, 9, 23),
  (2022, 10, 6, 6, 0, 26),
  (2022, 11, 6, 21, 17, 14),
  (2022, 12, 7, 7, 10, 38),
  (2022, 13, 7, 23, 4, 7),
  (2022, 14, 8, 7, 20, 29),
  (2022, 15, 8, 23, 11, 16),
  (2022, 16, 9, 7, 23, 32),
  (2022, 17, 9, 23, 9, 4),
  (2022, 18, 10, 8, 15, 22),
  (2022, 19, 10, 23, 18, 36),
  (2022, 20, 11, 7, 18, 45),
  (2022, 21, 11, 22, 16, 20),
  (2022, 22, 12, 7, 11, 46),
  (2022, 23, 12, 22, 5, 48),
  (2028, 0, 1, 6, 3, 55),
  (2028, 1, 1, 20, 21, 22),
  (2028, 2, 2, 4, 15, 31),
  (2028, 3, 2, 19, 11, 26),
  (2028, 4, 3, 5, 9, 25),
  (2028, 5, 3, 20, 10, 17),
  (2028, 6, 4, 4, 14, 3),
  (2028, 7, 4, 19, 21, 9),
  (2028, 8, 5, 5, 7, 12),
  (2028, 9, 5, 20, 20, 10),
  (2028, 10, 6, 5, 11, 16),
  (2028, 11, 6, 21, 4, 2),
  (2028, 12, 7, 6, 21, 30),
  (2028, 13, 7, 22, 14, 54),
  (2028, 14, 8, 7, 7, 21),
  (2028, 15, 8, 22, 22, 1),
  (2028, 16, 9, 7, 10, 22),
  (2028, 17, 9, 22, 19, 45),
  (2028, 18, 10, 8, 2, 9),
  (2028, 19, 10, 23, 5, 13),
  (2028, 20, 11, 7, 5, 27),
  (2028, 21, 11, 22, 2, 54),
  (2028, 22, 12, 6, 22, 25),
  (2028, 23, 12, 21, 16, 20),
]

# Same budget as the C++ golden test: HKO's own ±0.5 min rounding + UT1/UTC conflation
# (±0.9 s) + full-chain error (seconds). The export's millisecond truncation (< 1 ms)
# fits inside it; a ΔT-class 69 s slip does not.
HKO_TOLERANCE_MS: Final[int] = 60_000

# `jieqi_moment` (statistics/common.py) truncates to microseconds, the export to
# milliseconds; the two float roundings can land a millisecond apart, never more.
IDENTITY_TOLERANCE_MS: Final[int] = 1

EPOCH_DT: Final[datetime] = datetime(1970, 1, 1)


def to_ms(dt: datetime) -> int:
  delta = dt - EPOCH_DT
  return (delta.days * 86_400 + delta.seconds) * 1000 + delta.microseconds // 1000


def check_jieqi_table() -> int:
  """Hold the exported jieqi table (`toolbox/jieqi_table.py`) to its invariants.

  Checked here: entry count, strict time order, calendar-year containment, per-year
  completeness, the idx ↔ name_zh mapping against an independent transcription, unix_ms ↔
  iso_utc self-consistency, byte-level determinism, an independent re-derivation through the
  `statistics/common.py` binding, and HKO almanac anchors. Needs the built library
  (`./project.py --build`): a missing build turns the gate red, never a silent skip.
  """
  print("#" * 60)
  yellow_print("Checking the exported jieqi table against its invariants...")

  failures: List[str] = []

  try:
    exporter = load_exporter()
    first = exporter.serialize(exporter.generate(exporter.DEFAULT_START_YEAR, exporter.DEFAULT_END_YEAR))
  except Exception as e:
    red_print(f"Cannot generate the table: {type(e).__name__}: {e}")
    return 1

  doc = json.loads(first)
  entries = doc["entries"]

  start_year = doc["parameters"]["start_year"]
  end_year = doc["parameters"]["end_year"]
  years = list(range(start_year, end_year + 1))

  # Determinism: a second run must be byte-identical (no timestamps, stable ordering).
  second = exporter.serialize(exporter.generate(start_year, end_year))
  if first != second:
    failures.append("two runs of the export differ byte-wise")

  # Count: 24 per year, both ends inclusive.
  expected_count = exporter.JIEQI_PER_YEAR * len(years)
  if len(entries) != expected_count:
    failures.append(f"entry count {len(entries)}, expected {expected_count}")

  # Strictly increasing in time -- the table's ordering contract (#164).
  for prev, cur in zip(entries, entries[1:], strict=False):
    if cur["unix_ms"] <= prev["unix_ms"]:
      failures.append(f"not strictly increasing: {prev['iso_utc']} -> {cur['iso_utc']}")

  for entry in entries:
    label = f"year {entry['year']} idx {entry['idx']}"

    # Containment: a year's 24 crossings all land inside that calendar year.
    if int(entry["iso_utc"][0:4]) != entry["year"]:
      failures.append(f"{label}: iso_utc {entry['iso_utc']} escapes its year")

    # The idx ↔ name mapping, pinned against the independent transcription.
    if entry["name_zh"] != JIEQI_NAMES_ZH[entry["idx"]]:
      failures.append(f"{label}: name_zh {entry['name_zh']}, expected {JIEQI_NAMES_ZH[entry['idx']]}")

    # unix_ms and iso_utc describe the same millisecond.
    if to_ms(datetime.strptime(entry["iso_utc"], "%Y-%m-%dT%H:%M:%S.%fZ")) != entry["unix_ms"]:
      failures.append(f"{label}: iso_utc and unix_ms disagree")

  # Every year carries each of the 24 indices exactly once.
  for year in years:
    idxs = sorted(e["idx"] for e in entries if e["year"] == year)
    if idxs != list(range(exporter.JIEQI_PER_YEAR)):
      failures.append(f"year {year}: idx set {idxs}")

  # Identity: every entry re-derived through the independent `statistics/common.py` binding.
  try:
    common = load_common()
    for entry in entries:
      moment = common.jieqi_moment(entry["year"], common.Jieqi(entry["idx"])).moment
      if abs(to_ms(moment) - entry["unix_ms"]) > IDENTITY_TOLERANCE_MS:
        failures.append(
          f"year {entry['year']} idx {entry['idx']}: export {entry['iso_utc']} vs common.py {moment.isoformat()}"
        )
  except Exception as e:
    failures.append(f"identity re-derivation via statistics/common.py failed: {type(e).__name__}: {e}")

  # Chain-external anchors: HKO almanac wall clocks (HKT = UTC+8).
  for year, hko_entry, month, day, hour, minute in HKO_ANCHORS:
    idx = (JIEQI_NAMES_ZH.index("小寒") + hko_entry) % exporter.JIEQI_PER_YEAR
    matches = [e for e in entries if e["year"] == year and e["idx"] == idx]
    if len(matches) != 1:
      failures.append(f"HKO anchor year {year} entry {hko_entry}: {len(matches)} matching rows")
      continue
    hko_ms = to_ms(datetime(year, month, day, hour, minute)) - 8 * 3_600_000
    if abs(matches[0]["unix_ms"] - hko_ms) > HKO_TOLERANCE_MS:
      failures.append(
        f"HKO anchor year {year} entry {hko_entry}: export {matches[0]['iso_utc']} vs "
        f"almanac {year}-{month:02d}-{day:02d} {hour:02d}:{minute:02d} HKT"
      )

  if failures:
    for failure in failures:
      red_print(failure)
    red_print(f"jieqi table check failed ({len(failures)} finding(s))")
    return 1

  green_print(
    f"jieqi table check passed: {len(entries)} entries, "
    f"{len(HKO_ANCHORS)} HKO anchors, identity re-derivation, determinism"
  )
  return 0
