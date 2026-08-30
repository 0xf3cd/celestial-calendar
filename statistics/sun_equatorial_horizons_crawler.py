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
#
# Manual replay for TEST(Sun, EquatorialApparentVsJplHorizons). It submits the fixed 42 JDEs
# below and rejects an unexpected API identity or any stored-digit mismatch before printing rows.
# Importing this module does not contact Horizons; only main() performs the request.

import re
import sys

from decimal import Decimal
from pathlib import Path
from typing import Final

import requests


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
SUN_TEST: Final[Path] = REPO_ROOT / "src" / "test" / "astro" / "sun_test.cpp"
HORIZONS_URL: Final[str] = "https://ssd.jpl.nasa.gov/api/horizons.api"
HORIZONS_API_VERSION: Final[str] = "1.2"
STORED_TABLE_DE_SOURCE: Final[str] = "DE440"
CURRENT_HORIZONS_DE_SOURCE: Final[str] = "DE441"

JDES: Final[tuple[str, ...]] = (
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

ROW_PATTERN: Final[re.Pattern[str]] = re.compile(
  r"\{\s*(\d+\.\d+),\s*\{\s*([-+]?\d+\.\d+),\s*([-+]?\d+\.\d+)\s*\}\s*\},"
)
DECIMAL_CELL: Final[re.Pattern[str]] = re.compile(r"[-+]?(?:\d+\.\d*|\d*\.\d+)(?:[Ee][-+]?\d+)?")


def horizons_params() -> dict[str, str]:
  return {
    "format": "text",
    "COMMAND": "'10'",
    "OBJ_DATA": "'NO'",
    "MAKE_EPHEM": "'YES'",
    "EPHEM_TYPE": "'OBSERVER'",
    "CENTER": "'500@399'",
    "TLIST": "'" + " ".join(JDES) + "'",
    "TLIST_TYPE": "'JD'",
    "TIME_TYPE": "'TT'",
    "QUANTITIES": "'2'",
    "ANG_FORMAT": "'DEG'",
    "EXTRA_PREC": "'YES'",
    "CAL_FORMAT": "'BOTH'",
    "CSV_FORMAT": "'YES'",
  }


def stored_rows(path: Path = SUN_TEST) -> dict[str, tuple[str, str]]:
  text = path.read_text(encoding="utf-8")
  test_start = text.index("TEST(Sun, EquatorialApparentVsJplHorizons)")
  dataset_start = text.index("const std::unordered_map", test_start)
  dataset_end = text.index("\n  for (", dataset_start)
  return {jde: (ra, dec) for jde, ra, dec in ROW_PATTERN.findall(text[dataset_start:dataset_end])}


def _validate_response_identity(text: str) -> str:
  prelude, marker, remainder = text.partition("$$SOE")
  if not marker:
    raise RuntimeError("Horizons response has no data marker")
  api_version = re.search(r"(?m)^API VERSION:\s*(\S+)\s*$", prelude)
  if api_version is None or api_version.group(1) != HORIZONS_API_VERSION:
    raise RuntimeError(f"unexpected Horizons API version; expected {HORIZONS_API_VERSION}")
  if "Sun (10)" not in prelude:
    raise RuntimeError("Horizons response is not for Sun (10)")
  if f"{{source: {CURRENT_HORIZONS_DE_SOURCE}}}" not in prelude:
    raise RuntimeError(f"unexpected Horizons ephemeris; expected {CURRENT_HORIZONS_DE_SOURCE}")
  return remainder


def parse_horizons_response(text: str) -> dict[str, tuple[str, str]]:
  remainder = _validate_response_identity(text)
  data, marker, _trailer = remainder.partition("$$EOE")
  if not marker:
    raise RuntimeError("Horizons response has no end marker")

  fixed_jdes = set(JDES)
  rows: dict[str, tuple[str, str]] = {}
  for line in data.splitlines():
    if not line.strip():
      continue
    numeric_cells = [cell.strip() for cell in line.split(",") if DECIMAL_CELL.fullmatch(cell.strip())]
    if len(numeric_cells) != 3:
      raise RuntimeError(f"unexpected Horizons quantity-2 row: {line.strip()}")
    jde_cell, ra, dec = numeric_cells
    jde = format(Decimal(jde_cell).quantize(Decimal("0.000001")), "f")
    if jde not in fixed_jdes:
      raise RuntimeError(f"unexpected JDE in Horizons response: {jde_cell}")
    if jde in rows:
      raise RuntimeError(f"duplicate JDE in Horizons response: {jde}")
    rows[jde] = (ra, dec)

  missing = [jde for jde in JDES if jde not in rows]
  if missing:
    raise RuntimeError(f"JDEs missing from Horizons response: {missing}")
  return rows


def require_exact_match(
  rows: dict[str, tuple[str, str]],
  expected: dict[str, tuple[str, str]],
) -> None:
  mismatches = [(jde, rows[jde], expected[jde]) for jde in JDES if rows[jde] != expected[jde]]
  if mismatches:
    details = "; ".join(f"{jde}: {actual} != {stored}" for jde, actual, stored in mismatches)
    raise RuntimeError(f"Horizons stored digits differ on {len(mismatches)} of 42 rows: {details}")


def format_rows(rows: dict[str, tuple[str, str]]) -> str:
  return "\n".join(f"    {{ {jde:>14}, {{ {rows[jde][0]:>13}, {rows[jde][1]:>13} }} }}," for jde in JDES)


def fetch_horizons() -> str:
  response = requests.get(HORIZONS_URL, params=horizons_params(), timeout=60)
  response.raise_for_status()
  return response.text


def main() -> None:
  expected = stored_rows()
  # Reject table/query drift before making an external request.
  if tuple(expected) != JDES:
    raise RuntimeError("stored Sun-equatorial JDE inputs differ from the fixed 42-input query")

  rows = parse_horizons_response(fetch_horizons())
  require_exact_match(rows, expected)
  print("// JPL Horizons API 1.2, Sun (10), geocenter, DE441, TT, apparent RA/Dec")
  print(format_rows(rows))
  print("all 42 Horizons rows match every stored digit", file=sys.stderr)


if __name__ == "__main__":
  main()
