# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import importlib.util
import json
import math

from datetime import datetime
from types import ModuleType
from typing import Final, List

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

# `jieqi_moment` (statistics/common.py) truncates to microseconds, the export to
# milliseconds; the two float roundings can land a millisecond apart, never more.
IDENTITY_TOLERANCE_MS: Final[int] = 1

EPOCH_DT: Final[datetime] = datetime(1970, 1, 1)


def to_ms(dt: datetime) -> int:
  delta = dt - EPOCH_DT
  return (delta.days * 86_400 + delta.seconds) * 1000 + delta.microseconds // 1000


def check_jieqi_table() -> int:
  """Hold the exported jieqi table (`toolbox/jieqi_table.py`) to its invariants.

  Checked here: schema @2 with an explicit UT1 scale and no UTC/Unix aliases or leap-second
  conversion; entry count, strict time order, calendar-year containment, per-year completeness,
  the idx ↔ name_zh mapping against an independent transcription, jd_ut1 ↔ iso_ut1 consistency,
  byte-level determinism, and agreement within 1 ms between the exported UT1 times and
  `statistics/common.py::jieqi_moment`.
  HKO reference accuracy is tested in `jieqi_golden_test.cpp`.
  Needs the built library
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
  if doc["schema"] != "celestial-calendar/jieqi-table@2" or doc["timescale"] != "UT1":
    failures.append("export must use schema @2 with an explicit UT1 scale")
  if doc["models"]["leap_seconds"] is not None:
    failures.append("UT1 export must not claim a UTC/leap-second conversion")
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
    if cur["jd_ut1"] <= prev["jd_ut1"]:
      failures.append(f"not strictly increasing: {prev['iso_ut1']} -> {cur['iso_ut1']}")

  for entry in entries:
    label = f"year {entry['year']} idx {entry['idx']}"

    # Containment: a year's 24 crossings all land inside that calendar year.
    if int(entry["iso_ut1"][0:4]) != entry["year"]:
      failures.append(f"{label}: iso_ut1 {entry['iso_ut1']} escapes its year")

    # The idx ↔ name mapping, pinned against the independent transcription.
    if entry["name_zh"] != JIEQI_NAMES_ZH[entry["idx"]]:
      failures.append(f"{label}: name_zh {entry['name_zh']}, expected {JIEQI_NAMES_ZH[entry['idx']]}")

    if "unix_ms" in entry or "iso_utc" in entry:
      failures.append(f"{label}: misleading UTC/Unix aliases are present")
    display_ms = to_ms(datetime.strptime(entry["iso_ut1"], "%Y-%m-%dT%H:%M:%S.%f"))
    jd_ms = (entry["jd_ut1"] - 2440587.5) * 86_400_000
    # The display truncates <1 ms; the separate JD double adds at most one ULP here.
    if abs(display_ms - jd_ms) > 1.0 + math.ulp(entry["jd_ut1"]) * 86_400_000:
      failures.append(f"{label}: iso_ut1 and jd_ut1 disagree")

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
      display_ms = to_ms(datetime.strptime(entry["iso_ut1"], "%Y-%m-%dT%H:%M:%S.%f"))
      if abs(to_ms(moment) - display_ms) > IDENTITY_TOLERANCE_MS:
        failures.append(
          f"year {entry['year']} idx {entry['idx']}: export {entry['iso_ut1']} vs common.py {moment.isoformat()}"
        )
  except Exception as e:
    failures.append(f"identity re-derivation via statistics/common.py failed: {type(e).__name__}: {e}")

  if failures:
    for failure in failures:
      red_print(failure)
    red_print(f"jieqi table check failed ({len(failures)} finding(s))")
    return 1

  green_print(f"jieqi table check passed: {len(entries)} entries, identity re-derivation, determinism")
  return 0
