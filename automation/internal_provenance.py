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

import hashlib
import importlib.util
import json
import re
import sys

from dataclasses import dataclass
from pathlib import Path
from tempfile import TemporaryDirectory
from types import ModuleType
from typing import Callable, Final
from unittest.mock import patch

if __package__:
  from .source_identity import require as _require
  from .source_digest import RETAINED_REGISTRY, require_facts, retained_slice, verify_retained_data
else:
  from source_identity import require as _require
  from source_digest import RETAINED_REGISTRY, require_facts, retained_slice, verify_retained_data


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
NOAA_ACKNOWLEDGMENT: Final[str] = (
  "CelestialCalendar acknowledges NOAA's Global Monitoring Laboratory (GML) for the solar-calculator output used "
  "in this comparison."
)
HORIZONS_API_VERSION: Final[str] = "1.2"
HORIZONS_STORED_DE_SOURCE: Final[str] = "DE440"
HORIZONS_CURRENT_DE_SOURCE: Final[str] = "DE441"
HORIZONS_URL: Final[str] = "https://ssd.jpl.nasa.gov/api/horizons.api"
USNO_API_VERSION: Final[str] = "4.0.1"

HORIZONS_JDES: Final[tuple[str, ...]] = (
  "2432253.451627",
  "2432975.419454",
  "2433034.902818",
  "2433162.091558",
  "2433787.150963",
  "2435390.614473",
  "2435617.087892",
  "2435771.938697",
  "2438063.700493",
  "2439754.668377",
  "2440526.881017",
  "2440597.184260",
  "2440705.218788",
  "2442726.143416",
  "2445127.187259",
  "2445269.770144",
  "2446762.840711",
  "2448454.950968",
  "2451708.856236",
  "2452912.895567",
  "2453252.717744",
  "2453529.584620",
  "2454516.733665",
  "2454981.361671",
  "2455545.315223",
  "2456122.270342",
  "2456937.645140",
  "2457345.493073",
  "2458391.280009",
  "2459227.436405",
  "2459478.301612",
  "2460459.539681",
  "2460722.377352",
  "2463426.950821",
  "2463478.002658",
  "2463567.787810",
  "2464346.781906",
  "2465052.280288",
  "2465606.569112",
  "2466795.003140",
  "2469331.309816",
  "2469951.514795",
)


@dataclass(frozen=True)
class InternalProvenanceCounts:
  history_tables: int
  horizons_inputs: int
  julian_internal_rows: int


def _read(repo_root: Path, relative: str) -> str:
  return (repo_root / relative).read_text(encoding="utf-8")


def _test_block(text: str, marker: str) -> str:
  start = text.index(marker)
  end = text.find("\nTEST(", start + len(marker))
  return text[start:] if end == -1 else text[start:end]


def _normalized_comment_text(text: str) -> str:
  uncommented = (re.sub(r"^\s*(?://|#)\s?", "", line) for line in text.splitlines())
  return " ".join(" ".join(uncommented).split())


def _load_module(path: Path) -> ModuleType:
  spec = importlib.util.spec_from_file_location(f"_internal_provenance_{path.stem}_{id(path)}", path)
  _require(spec is not None and spec.loader is not None, f"cannot load {path}")
  module = importlib.util.module_from_spec(spec)
  if path.name == "sun_equatorial_horizons_crawler.py":
    shared = _load_module(path.with_name("horizons_common.py"))
    with patch.dict(sys.modules, {"horizons_common": shared}):
      spec.loader.exec_module(module)
  else:
    spec.loader.exec_module(module)
  return module


def _require_rejection(call: Callable[[], object], message_fragment: str, label: str) -> None:
  try:
    call()
  except RuntimeError as error:
    _require(
      message_fragment in str(error),
      f"{label} does not reject with the expected diagnostic: {message_fragment}",
    )
  except Exception as error:
    raise RuntimeError(f"{label} does not reject with the expected diagnostic: {message_fragment}") from error
  else:
    raise RuntimeError(f"{label} is not rejected")


def _verify_r35(repo_root: Path) -> None:
  sun = _read(repo_root, "src/astro/sun.hpp")
  start = sun.index("namespace astro::sun::geocentric_coord::math")
  end = sun.index("namespace detail", start)
  record = sun[start:end]
  normalized = " ".join(record.replace("//", " ").replace("*", " ").split())
  _require(record.count("github.com/leetcola/nong/wiki/") == 2, "R35 nong conceptual-source inventory differs")
  _require("conceptual and implementation ideas" in normalized, "R35 nong source role differs")
  _require("does not claim copied code or byte identity" in normalized, "R35 identity boundary differs")


def _verify_v37(repo_root: Path) -> tuple[int, int]:
  table_ids = ("earth-0", "earth-1", "sun-0", "sun-2", "moon-0", "moon-1", "elp2000_82b-0")
  verify_retained_data(repo_root, (*table_ids, "julian_day-0"))
  data = json.loads((repo_root / RETAINED_REGISTRY).read_text(encoding="utf-8"))["data_blocks"]
  modification_facts = {
    "earth-0": (
      "Earth.geometric_heliocentric_position",
      "characterization-tight",
      "JPL DE441",
      "sun_horizons_golden_test.cpp",
    ),
    "earth-1": (
      "nutation_longitude/obliquity",
      "characterization-tight",
      "Meeus Example 22.a",
      "Table 22.A",
      "sidereal_time_test.cpp",
    ),
    "sun-0": (
      "Sun.geometric_geocentric_position",
      "geometric mirrors",
      "all other rows are independent samples",
      "sun_horizons_golden_test.cpp",
      "JPL DE441",
    ),
    "sun-2": ("bare-κ/R aberration", "λ column shifted", "β and r did not shift"),
    "moon-0": ("converted from degrees to radians", "All four columns shifted"),
    "moon-1": ("no stored digit has shifted",),
    "elp2000_82b-0": ("values shifted", "moon_horizons_golden_test.cpp", "JPL DE441"),
  }
  for block_id in table_ids:
    entry = data[block_id]
    source = _read(repo_root, entry["path"])
    end = source.index(entry["start"])
    begin = max(source.rfind("TEST(", 0, end), source.rfind("inline auto ", 0, end))
    comment = source[begin:end]
    require_facts(
      comment,
      (
        "PyMeeus-derived regression material",
        "original version and sample seed are not recorded",
        *modification_facts.get(block_id, ()),
      ),
      f"V37 {block_id}",
    )

  julian = _read(repo_root, "src/test/astro/julian_day_test.cpp")
  start = julian.index("const std::unordered_map<double, Datetime> JDE_TEST_DATASET")
  end = julian.index("\n};", start)
  dataset = julian[start:end]
  internal_marker = "Seven rows are internal regression material"
  meeus_marker = "Meeus Ch.7 textbook anchor"
  meeus_worked_marker = "Meeus Ch.7 worked value"
  external_marker = "Four rows from http://www.stevegs.com/utils/jd_calc/"
  _require(dataset.count("internal regression material") == 1, "V37 Julian-day internal label count differs")
  _require(
    meeus_marker in dataset
    and meeus_worked_marker in dataset
    and internal_marker in dataset
    and external_marker in dataset,
    "V37 Julian-day row labels differ",
  )
  external_at = dataset.index(external_marker)
  row_pattern = re.compile(r"^\s*\{\s*\d", re.MULTILINE)
  rows_before_external = len(row_pattern.findall(dataset[:external_at]))
  internal_rows = rows_before_external - 2
  _require(rows_before_external == 9 and internal_rows == 7, "V37 internal Julian-day row count differs")
  _require(len(row_pattern.findall(dataset[external_at:])) == 4, "V37 external Julian-day row count differs")
  return len(table_ids) + 1, internal_rows


def _verify_v07(repo_root: Path) -> int:
  crawler_path = repo_root / "statistics" / "sun_equatorial_horizons_crawler.py"
  sun_test_path = repo_root / "src" / "test" / "astro" / "sun_test_data.hpp"
  crawler = _load_module(crawler_path)
  _require(crawler.HORIZONS_API_VERSION == HORIZONS_API_VERSION, "V07 Horizons API version pin differs")
  _require(
    crawler.STORED_TABLE_DE_SOURCE == HORIZONS_STORED_DE_SOURCE,
    "V07 stored-table DE-source pin differs",
  )
  _require(
    crawler.CURRENT_HORIZONS_DE_SOURCE == HORIZONS_CURRENT_DE_SOURCE,
    "V07 current Horizons DE-source pin differs",
  )
  _require(tuple(crawler.JDES) == HORIZONS_JDES, "V07 fixed 42-input inventory differs")
  expected_params = {
    "format": "text",
    "COMMAND": "'10'",
    "OBJ_DATA": "'NO'",
    "MAKE_EPHEM": "'YES'",
    "EPHEM_TYPE": "'OBSERVER'",
    "CENTER": "'500@399'",
    "TLIST": " ".join(f"'{jde}'" for jde in HORIZONS_JDES),
    "TLIST_TYPE": "'JD'",
    "TIME_TYPE": "'TT'",
    "QUANTITIES": "'2'",
    "ANG_FORMAT": "'DEG'",
    "EXTRA_PREC": "'YES'",
    "CAL_FORMAT": "'BOTH'",
    "CSV_FORMAT": "'YES'",
  }
  _require(crawler.horizons_params() == expected_params, "V07 Horizons query shape differs")

  def response(version: str, source: str) -> str:
    return f"API VERSION: {version}\nTarget body name: Sun (10)\n{{source: {source}}}\n$$SOE\nnot,a,row\n$$EOE\n"

  _require_rejection(
    lambda: crawler.parse_horizons_response(response("9.9", HORIZONS_CURRENT_DE_SOURCE)),
    "API version",
    "V07 unexpected API version",
  )
  for version in ("1.3", ""):
    _require_rejection(
      lambda version=version: crawler.parse_horizons_response(response(version, HORIZONS_CURRENT_DE_SOURCE)),
      "API version",
      "V07 unexpected API version",
    )
  _require_rejection(
    lambda: crawler.parse_horizons_response(response(HORIZONS_API_VERSION, "DE442")),
    "ephemeris",
    "V07 unexpected DE source",
  )
  _require_rejection(
    lambda: crawler.parse_horizons_response(response(HORIZONS_API_VERSION, "DE440")),
    "ephemeris",
    "V07 unexpected DE source",
  )

  sun_test = sun_test_path.read_text(encoding="utf-8")
  table_start = sun_test.index("// Retained data: sun-3.")
  golden_block = sun_test[sun_test.rfind("inline auto ", 0, table_start) : table_start]
  stored_de_sources = re.findall(r"\bVSOP87D-vs-(DE\d+)\b", golden_block)
  _require(
    stored_de_sources == [HORIZONS_STORED_DE_SOURCE],
    "V07 stored table DE source differs",
  )
  verify_retained_data(repo_root, ("sun-3",))
  table = retained_slice(sun_test, "// Retained data: sun-3.", "};")
  stored = {
    jd: (ra, dec)
    for jd, ra, dec in re.findall(
      r"\{\s*(\d+\.\d+),\s*\{\s*([+-]?\d+\.\d+),\s*([+-]?\d+\.\d+)\s*\}",
      table,
    )
  }
  with TemporaryDirectory(prefix="celestial-v07-") as temporary:
    probe = Path(temporary) / "stored.cpp"
    probe.write_text(table + "};\n", encoding="utf-8")
    _require(crawler.stored_rows(probe) == stored, "V07 stored-row parser differs")
  _require(tuple(stored) == HORIZONS_JDES, "V07 stored table inputs differ")
  data = "\n".join(
    f"date,{'2432253.451627002' if jde == HORIZONS_JDES[0] else jde},,,{ra},{dec}," for jde, (ra, dec) in stored.items()
  )
  valid_response = (
    f"API VERSION: {HORIZONS_API_VERSION}\nTarget body name: Sun (10)\n"
    f"{{source: {HORIZONS_CURRENT_DE_SOURCE}}}\n"
    f"Date__(TT), Date_________JDTT,,, R.A.__(a-app), DEC___(a-app),\n$$SOE\n{data}\n$$EOE\n"
  )

  calls: list[tuple[str, dict]] = []

  class FakeResponse:
    text = valid_response

    @staticmethod
    def raise_for_status() -> None:
      return None

  class FakeRequests:
    @staticmethod
    def get(url: str, **kwargs) -> FakeResponse:
      calls.append((url, kwargs))
      return FakeResponse()

  original_requests = crawler.horizons_common.requests
  crawler.horizons_common.requests = FakeRequests()
  try:
    fetched = crawler.fetch_horizons()
  finally:
    crawler.horizons_common.requests = original_requests
  _require(
    calls == [(HORIZONS_URL, {"params": expected_params, "timeout": 60})],
    "V07 Horizons endpoint or request shape differs",
  )
  _require(crawler.parse_horizons_response(fetched) == stored, "V07 Horizons response parser differs")
  mutated = dict(stored)
  first_jde = HORIZONS_JDES[0]
  mutated[first_jde] = ("0.000000000", mutated[first_jde][1])
  _require_rejection(
    lambda: crawler.require_exact_match(mutated, stored),
    "stored digits differ",
    "V07 stored-digit mismatch",
  )
  emitted = re.findall(r"\{\s*(\d+\.\d+),\s*\{\s*([+-]?\d+\.\d+),\s*([+-]?\d+\.\d+)\s*\}", crawler.format_rows(stored))
  _require(emitted == [(jd, *stored[jd]) for jd in HORIZONS_JDES], "V07 emitted table columns or values differ")
  return len(crawler.JDES)


def _verify_v11_v42(repo_root: Path) -> None:
  crawler_path = repo_root / "statistics" / "sunrise_golden_crawler.py"
  test_path = repo_root / "src" / "test" / "astro" / "rise_set_golden_test.cpp"
  crawler_text = crawler_path.read_text(encoding="utf-8")
  test_text = test_path.read_text(encoding="utf-8")
  for relative, text in ((str(crawler_path), crawler_text), (str(test_path), test_text)):
    _require(
      NOAA_ACKNOWLEDGMENT in _normalized_comment_text(text),
      f"NOAA GML acknowledgment is missing from {relative}",
    )
    _require("sunrise-sunset.org" not in text, f"unsupported V42 numeric prose remains in {relative}")

  crawler = _load_module(crawler_path)
  _require(crawler.USNO_API_VERSION == USNO_API_VERSION, "V11 USNO API version pin differs")
  _require_rejection(
    lambda: crawler.parse_usno(
      {
        "apiversion": "9.9",
        "properties": {"data": {"sundata": [{"phen": "Rise", "time": "06:18"}]}},
      }
    ),
    "unexpected USNO API version",
    "V11 unexpected USNO API version",
  )

  calls: list[tuple[str, dict]] = []

  class FakeResponse:
    @staticmethod
    def raise_for_status() -> None:
      return None

    @staticmethod
    def json() -> dict:
      return {
        "apiversion": USNO_API_VERSION,
        "properties": {"data": {"sundata": [{"phen": "Rise", "time": "06:18"}]}},
      }

  def fake_get(url: str, **kwargs) -> FakeResponse:
    calls.append((url, kwargs))
    return FakeResponse()

  class FakeRequests:
    get = staticmethod(fake_get)

  original_requests = crawler.requests
  crawler.requests = FakeRequests()
  try:
    _require(
      crawler.fetch_usno(-0.22, -78.51, 2026, 3, 20, -5) == {"Rise": "06:18"},
      "V11 USNO parser result differs",
    )
  finally:
    crawler.requests = original_requests
  _require(
    calls
    == [
      (
        "https://aa.usno.navy.mil/api/rstt/oneday",
        {
          "params": {"date": "2026-03-20", "coords": "-0.22,-78.51", "tz": "-5"},
          "headers": {"User-Agent": "celestial-calendar-golden/0.1"},
          "timeout": 30,
        },
      )
    ],
    "V11 USNO request shape differs",
  )

  verify_retained_data(repo_root, ("rise_set_golden-0", "rise_set_golden-1"))
  _require(
    hashlib.sha256(repr((crawler.SITES, crawler.DATES)).encode()).hexdigest()
    == "23f9f0a164094dd5bce38f09222ad9c7d512a7f3d92bf5a2d8915fd6d4ad6483",
    "V11 crawler retained inputs differ",
  )


def verify_internal_provenance(repo_root: Path = REPO_ROOT) -> InternalProvenanceCounts:
  _verify_r35(repo_root)
  history_tables, julian_internal_rows = _verify_v37(repo_root)
  horizons_inputs = _verify_v07(repo_root)
  _verify_v11_v42(repo_root)
  return InternalProvenanceCounts(
    history_tables=history_tables,
    horizons_inputs=horizons_inputs,
    julian_internal_rows=julian_internal_rows,
  )


if __name__ == "__main__":
  print(verify_internal_provenance())
