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
import importlib.util
import re
import tokenize

from dataclasses import dataclass
from io import BytesIO
from pathlib import Path
from types import ModuleType
from typing import Callable, Final


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
HORIZONS_CRAWLER_CODE_SHA256: Final[str] = "21ccaf5b6ea041947df0c7844f083e48fcb24f5e56509d82eddf03342b41ebe1"
SUN_EQUATORIAL_GOLDEN_CODE_SHA256: Final[str] = "9a3ea072d0d94c912570deab756ff8b3cdd9718667e0d7e9f0d8eb3c6f0d0396"
SUNRISE_CRAWLER_CODE_SHA256: Final[str] = "89efef54952feb4671c0447bdb6d0bcb449bfbee5ff143ef33205759e6a020fa"
RISE_SET_GOLDEN_CODE_SHA256: Final[str] = "de32a033ebc2c61656b4220dea1532edf54c71810878cf12c411235b245352ff"

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


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _read(repo_root: Path, relative: str) -> str:
  return (repo_root / relative).read_text(encoding="utf-8")


def _test_block(text: str, marker: str) -> str:
  start = text.index(marker)
  end = text.find("\nTEST(", start + len(marker))
  return text[start:] if end == -1 else text[start:end]


def _canonical_cpp(text: str) -> str:
  without_comments = re.sub(r"//[^\n]*|/\*.*?\*/", " ", text, flags=re.DOTALL)
  return " ".join(without_comments.split())


def _normalized_comment_text(text: str) -> str:
  uncommented = (re.sub(r"^\s*(?://|#)\s?", "", line) for line in text.splitlines())
  return " ".join(" ".join(uncommented).split())


def _python_code_sha256(text: str) -> str:
  comment_columns = {
    token.start[0]: token.start[1]
    for token in tokenize.tokenize(BytesIO(text.encode("utf-8")).readline)
    if token.type == tokenize.COMMENT
  }
  code_lines = (
    line[: comment_columns.get(line_number, len(line))].rstrip()
    for line_number, line in enumerate(text.splitlines(), start=1)
  )
  canonical = "\n".join(line for line in code_lines if line.strip())
  return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def _load_module(path: Path) -> ModuleType:
  spec = importlib.util.spec_from_file_location(f"_internal_provenance_{path.stem}_{id(path)}", path)
  _require(spec is not None and spec.loader is not None, f"cannot load {path}")
  module = importlib.util.module_from_spec(spec)
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
  for anchor in ("202a0bd", "397422cc", "066c28db", "5101b6d2", "bc79c991"):
    _require(anchor in record, f"R35 project history anchor is missing: {anchor}")
  _require(record.count("github.com/leetcola/nong/wiki/") == 2, "R35 nong conceptual-source inventory differs")
  _require("conceptual and implementation ideas" in normalized, "R35 nong source role differs")
  _require("does not claim copied code or byte identity" in normalized, "R35 identity boundary differs")


def _verify_v37(repo_root: Path) -> tuple[int, int]:
  table_records = (
    (
      "src/test/astro/earth_test.cpp",
      "TEST(Earth, Vsop87dEvaluate)",
      ("Introduced by b5f6c35", "no later numeric shift is recorded"),
    ),
    (
      "src/test/astro/earth_test.cpp",
      "TEST(Earth, NutationMeeus)",
      ("Introduced by a99d4d7", "no later numeric shift is recorded"),
    ),
    (
      "src/test/astro/sun_test.cpp",
      "TEST(Sun, GeocentricPosition)",
      ("Introduced by b5f6c35", "no later numeric shift is recorded"),
    ),
    (
      "src/test/astro/sun_test.cpp",
      "TEST(Sun, CorrectedPosition)",
      ("Introduced by f4b0980", "shifted in da22c87", "PR #100"),
    ),
    (
      "src/test/astro/moon_test.cpp",
      "TEST(Moon, CoordAndPpi)",
      ("Introduced by 066c28d", "shifted in 7ea26ee", "PR #96"),
    ),
    (
      "src/test/astro/moon_test.cpp",
      "TEST(Moon, Perturbation)",
      ("Introduced by 066c28d", "no stored digit has shifted since", "7ea26ee", "PR #96"),
    ),
    (
      "src/test/astro/elp2000_82b_test.cpp",
      "TEST(Elp2000, Evaluate)",
      ("Introduced by 066c28d", "shifted in 7ea26ee", "PR #96"),
    ),
  )
  for relative, marker, anchors in table_records:
    block = _test_block(_read(repo_root, relative), marker)
    _require("PyMeeus-derived regression material" in block, f"V37 role label is missing: {marker}")
    for anchor in anchors:
      _require(anchor in block, f"V37 history anchor is missing from {marker}: {anchor}")

  julian = _read(repo_root, "src/test/astro/julian_day_test.cpp")
  start = julian.index("const std::unordered_map<double, Datetime> JDE_TEST_DATASET")
  end = julian.index("\n};", start)
  dataset = julian[start:end]
  _require("introduced by da333dd" in julian[:start], "V37 Julian-day introduction anchor is missing")
  _require("no later numeric shift is" in julian[:start], "V37 Julian-day shift history is missing")
  internal_marker = "The other seven pre-V28 rows are internal regression material"
  meeus_marker = "Meeus Ch.7 textbook anchor"
  meeus_worked_marker = "Meeus Ch.7 worked value"
  v28_marker = "V28 rows from http://www.stevegs.com/utils/jd_calc/"
  _require(dataset.count("internal regression material") == 1, "V37 Julian-day internal label count differs")
  _require(
    meeus_marker in dataset and meeus_worked_marker in dataset and internal_marker in dataset and v28_marker in dataset,
    "V37 Julian-day row labels differ",
  )
  v28_at = dataset.index(v28_marker)
  row_pattern = re.compile(r"^\s*\{\s*\d", re.MULTILINE)
  pre_v28_rows = len(row_pattern.findall(dataset[:v28_at]))
  internal_rows = pre_v28_rows - 2
  _require(pre_v28_rows == 9 and internal_rows == 7, "V37 internal Julian-day row count differs")
  _require(len(row_pattern.findall(dataset[v28_at:])) == 4, "V37 V28 Julian-day row count differs")
  return len(table_records) + 1, internal_rows


def _verify_v07(repo_root: Path) -> int:
  crawler_path = repo_root / "statistics" / "sun_equatorial_horizons_crawler.py"
  sun_test_path = repo_root / "src" / "test" / "astro" / "sun_test.cpp"
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
    "TLIST": "'" + " ".join(HORIZONS_JDES) + "'",
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
  _require_rejection(
    lambda: crawler.parse_horizons_response(response(HORIZONS_API_VERSION, "DE440")),
    "ephemeris",
    "V07 unexpected DE source",
  )

  sun_test = sun_test_path.read_text(encoding="utf-8")
  golden_block = _test_block(sun_test, "TEST(Sun, EquatorialApparentVsJplHorizons)")
  stored_de_sources = re.findall(r"\bVSOP87D-vs-(DE\d+)\b", golden_block)
  _require(
    stored_de_sources == [HORIZONS_STORED_DE_SOURCE],
    "V07 stored table DE source differs",
  )
  golden_code_sha256 = hashlib.sha256(_canonical_cpp(golden_block).encode("utf-8")).hexdigest()
  _require(
    golden_code_sha256 == SUN_EQUATORIAL_GOLDEN_CODE_SHA256,
    f"V07 active golden values or assertions differ; got {golden_code_sha256}",
  )
  stored = crawler.stored_rows(sun_test_path)
  _require(tuple(stored) == HORIZONS_JDES, "V07 stored table inputs differ")
  data = "\n".join(
    f"date,{'2432253.451627002' if jde == HORIZONS_JDES[0] else jde},,,{ra},{dec}," for jde, (ra, dec) in stored.items()
  )
  valid_response = (
    f"API VERSION: {HORIZONS_API_VERSION}\nTarget body name: Sun (10)\n"
    f"{{source: {HORIZONS_CURRENT_DE_SOURCE}}}\n$$SOE\n{data}\n$$EOE\n"
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

  original_requests = crawler.requests
  crawler.requests = FakeRequests()
  try:
    fetched = crawler.fetch_horizons()
  finally:
    crawler.requests = original_requests
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
  expected_format = "\n".join(
    f"    {{ {jde:>14}, {{ {stored[jde][0]:>13}, {stored[jde][1]:>13} }} }}," for jde in HORIZONS_JDES
  )
  _require(crawler.format_rows(stored) == expected_format, "V07 emitted table format differs")
  crawler_code_sha256 = _python_code_sha256(crawler_path.read_text(encoding="utf-8"))
  _require(
    crawler_code_sha256 == HORIZONS_CRAWLER_CODE_SHA256,
    f"V07 crawler active code or values differ; got {crawler_code_sha256}",
  )
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

  crawler_code_sha256 = _python_code_sha256(crawler_text)
  _require(
    crawler_code_sha256 == SUNRISE_CRAWLER_CODE_SHA256,
    f"V11 crawler active code or values differ; got {crawler_code_sha256}",
  )
  canonical_test = _canonical_cpp(test_text)
  golden_code_sha256 = hashlib.sha256(canonical_test.encode("utf-8")).hexdigest()
  _require(
    golden_code_sha256 == RISE_SET_GOLDEN_CODE_SHA256,
    f"V42 active golden values or assertions differ; got {golden_code_sha256}",
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
