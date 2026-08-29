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

"""Rebuild the committed USNO records from local, untracked response bodies.

The committed records retain hashes and normalized values, not the raw recapture corpus. The
`--evidence-dir` layout is `v12-bodies/NNN-*.json`, `v13-bodies/{g,l}NN-YYYY-MM-DD.json`,
`v14-year-2024.json`, `v26-current-deltat.data`, and `v29-jd-api.json`.
"""

import argparse
import hashlib
import json
import re

from datetime import date, datetime, timedelta, timezone
from decimal import Decimal, ROUND_FLOOR, ROUND_HALF_UP
from pathlib import Path
from typing import Final


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
OUTPUT_ROOT: Final[Path] = REPO_ROOT / "src" / "test" / "provenance" / "usno" / "2026-08-26"
API_VERSION: Final[str] = "4.0.1"
NUMBER: Final[str] = r"[+-]?(?:\d+\.\d+|\d+\.|\.\d+|\d+)"


def require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def json_bytes(payload: object) -> bytes:
  return (json.dumps(payload, indent=2, ensure_ascii=True) + "\n").encode()


def decimal_text(value: Decimal) -> str:
  text = format(value, "f")
  if "." in text:
    text = text.rstrip("0").rstrip(".")
  return "0" if text in {"-0", ""} else text


def sha256(path: Path) -> str:
  return hashlib.sha256(path.read_bytes()).hexdigest()


def block(text: str, start: str, end: str) -> str:
  start_at = text.index(start)
  return text[start_at : text.index(end, start_at)]


def v12_rows(repo_root: Path) -> list[dict[str, str | int]]:
  source = (repo_root / "src/test/astro/rise_set_moon_golden_test.cpp").read_text(encoding="utf-8")
  table = block(source, "const std::vector<MoonRow> USNO_ROWS {", "};")
  pattern = re.compile(
    rf"\{{\s*(\d+),\s*(\d+),\s*(\d+),\s*({NUMBER}),\s*({NUMBER}),\s*"
    r'"([^"]*)",\s*"([^"]*)",\s*"([^"]*)"\s*\}'
  )
  rows = [
    {
      "year": int(match.group(1)),
      "month": int(match.group(2)),
      "day": int(match.group(3)),
      "lat": decimal_text(Decimal(match.group(4))),
      "lon": decimal_text(Decimal(match.group(5))),
      "rise": match.group(6).strip(),
      "transit": match.group(7).strip(),
      "set": match.group(8).strip(),
    }
    for match in pattern.finditer(table)
  ]
  require(len(rows) == 181, f"V12 source rows={len(rows)}, expected 181")
  return rows


def build_v12(repo_root: Path, evidence: Path) -> dict:
  rows = v12_rows(repo_root)
  bodies = sorted((evidence / "v12-bodies").glob("*.json"))
  require(len(bodies) == 181, f"V12 retained bodies={len(bodies)}, expected 181")
  records = []
  for index, (row, body) in enumerate(zip(rows, bodies, strict=True)):
    require(body.name.startswith(f"{index:03d}-"), f"V12 body order differs at {index}")
    payload = json.loads(body.read_bytes())
    require(payload.get("apiversion") == API_VERSION, f"V12 body {index} API version differs")
    data = payload["properties"]["data"]
    times: dict[str, list[str]] = {}
    for entry in data["moondata"]:
      times.setdefault(entry["phen"], []).append(entry["time"])
    normalized = {
      "rise": times.get("Rise", [""])[-1],
      "transit": times.get("Upper Transit", [""])[-1],
      "set": times.get("Set", [""])[-1],
    }
    expected = {name: row[name] for name in ("rise", "transit", "set")}
    require(normalized == expected, f"V12 normalized cells differ at {index}")
    request = {
      "date": f"{row['year']:04d}-{row['month']:02d}-{row['day']:02d}",
      "coords": f"{row['lat']},{row['lon']}",
      "tz": "0",
    }
    geometry = payload["geometry"]["coordinates"]
    require(Decimal(str(geometry[0])) == Decimal(row["lon"]), f"V12 response longitude differs at {index}")
    require(Decimal(str(geometry[1])) == Decimal(row["lat"]), f"V12 response latitude differs at {index}")
    records.append(
      {
        "index": index,
        "request": request,
        "response": {
          "sha256": sha256(body),
          "bytes": len(body.read_bytes()),
          "apiversion": payload["apiversion"],
        },
        "normalized_cells": normalized,
      }
    )
  return {
    "schema": 1,
    "row": "V12",
    "endpoint": "https://aa.usno.navy.mil/api/rstt/oneday",
    "query_parameter_order": ["date", "coords", "tz"],
    "coordinate_order": "latitude,east-positive-longitude",
    "normalization": "last event wins for duplicate phenomena; absent phenomenon is an empty string",
    "historical_collection": {
      "date": "2026-08-15",
      "apiversion": None,
      "response_bodies_retained": False,
    },
    "current_recapture": {
      "date": "2026-08-26",
      "apiversion": API_VERSION,
      "response_bodies_in_repository": False,
      "response_hashes_recorded": 181,
      "retention": "Raw response bodies are not included; this record retains hashes and normalized cells.",
    },
    "records": records,
  }


def v13_rows(repo_root: Path) -> tuple[list[tuple[str, ...]], list[tuple[str, ...]]]:
  source = (repo_root / "src/test/astro/sidereal_time_test.cpp").read_text(encoding="utf-8")
  greenwich = block(source, "TEST(SiderealTime, GreenwichUsno)", "TEST(SiderealTime, LocalApparentUsno)")
  local = block(source, "TEST(SiderealTime, LocalApparentUsno)", "} // namespace astro::sidereal::test")
  row_pattern = re.compile(rf"\{{\s*({NUMBER}),\s*({NUMBER}),\s*({NUMBER}),\s*({NUMBER})\s*\}}")
  greenwich_rows = [match.groups() for match in row_pattern.finditer(greenwich)]
  local_rows = [match.groups() for match in row_pattern.finditer(local)]
  require(len(greenwich_rows) == 40, f"V13 Greenwich rows={len(greenwich_rows)}, expected 40")
  require(len(local_rows) == 20, f"V13 local rows={len(local_rows)}, expected 20")
  return greenwich_rows, local_rows


def jd_request_time(jd: str) -> tuple[str, str]:
  mjd = Decimal(jd) - Decimal("2400000.5")
  whole_days = int(mjd.to_integral_value(rounding=ROUND_FLOOR))
  seconds = int(((mjd - whole_days) * Decimal(86400)).to_integral_value(rounding=ROUND_HALF_UP))
  day = date(1858, 11, 17) + timedelta(days=whole_days)
  if seconds == 86400:
    day += timedelta(days=1)
    seconds = 0
  hour, remainder = divmod(seconds, 3600)
  minute, second = divmod(remainder, 60)
  return day.isoformat(), f"{hour:02d}:{minute:02d}:{second:02d}"


def hms_degrees(value: str) -> str:
  hour, minute, second = value.split(":")
  degrees = (Decimal(hour) + Decimal(minute) / 60 + Decimal(second) / 3600) * 15
  return format(degrees.quantize(Decimal("0.000000000001"), rounding=ROUND_HALF_UP), "f")


def normalized_degrees(value: str) -> str:
  return format(Decimal(value).quantize(Decimal("0.000000000001"), rounding=ROUND_HALF_UP), "f")


def build_v13(repo_root: Path, evidence: Path) -> dict:
  greenwich_rows, local_rows = v13_rows(repo_root)
  records = []
  specs = [
    ("greenwich", index, row, evidence / "v13-bodies" / f"g{index:02d}-{jd_request_time(row[0])[0]}.json")
    for index, row in enumerate(greenwich_rows)
  ]
  specs.extend(
    ("local", index, row, evidence / "v13-bodies" / f"l{index:02d}-{jd_request_time(row[0])[0]}.json")
    for index, row in enumerate(local_rows)
  )
  require(len(specs) == 60, "V13 request count differs")
  for kind, index, row, body in specs:
    require(body.is_file(), f"V13 retained body missing: {body.name}")
    payload = json.loads(body.read_bytes())
    require(payload.get("apiversion") == API_VERSION, f"V13 {kind} {index} API version differs")
    date_text, time_text = jd_request_time(row[0])
    if kind == "greenwich":
      coords = "0,0"
      expected = {"gmst_deg": normalized_degrees(row[2]), "gast_deg": normalized_degrees(row[3])}
      cells = {"gmst": payload["properties"]["data"][0]["gmst"], "gast": payload["properties"]["data"][0]["gast"]}
      normalized = {f"{name}_deg": hms_degrees(value) for name, value in cells.items()}
      repository = {"jd_ut1": row[0], "jde_tt": row[1], "expected_gmst_deg": row[2], "expected_gast_deg": row[3]}
    else:
      lon_east = -Decimal(row[2])
      coords = f"0,{decimal_text(lon_east)}"
      expected = {"last_deg": normalized_degrees(row[3])}
      cells = {"last": payload["properties"]["data"][0]["last"]}
      normalized = {"last_deg": hms_degrees(cells["last"])}
      repository = {
        "jd_ut1": row[0],
        "jde_tt": row[1],
        "longitude_west": row[2],
        "expected_last_deg": row[3],
      }
    require(normalized == expected, f"V13 {kind} {index} normalized cell differs")
    returned = payload["geometry"]["coordinates"]
    request_lon = Decimal(coords.split(",")[1])
    require(Decimal(str(returned[0])) == request_lon, f"V13 {kind} {index} returned longitude differs")
    require(Decimal(str(returned[1])) == 0, f"V13 {kind} {index} returned latitude differs")
    records.append(
      {
        "kind": kind,
        "index": index,
        "repository": repository,
        "request": {
          "date": date_text,
          "time": time_text,
          "coords": coords,
          "reps": "1",
          "intv_mag": "1",
          "intv_unit": "days",
        },
        "response": {
          "sha256": sha256(body),
          "bytes": len(body.read_bytes()),
          "apiversion": payload["apiversion"],
          "cells": cells,
        },
        "normalized_cells": normalized,
      }
    )
  return {
    "schema": 1,
    "row": "V13",
    "endpoint": "https://aa.usno.navy.mil/api/siderealtime",
    "query_parameter_order": ["date", "time", "coords", "reps", "intv_mag", "intv_unit"],
    "coordinate_order": "latitude,east-positive-longitude",
    "repository_longitude_convention": "west-positive",
    "normalization": {"hms_to_degrees": "(hours + minutes/60 + seconds/3600) * 15", "decimal_places": 12},
    "delta_t_relation_seconds": "69.184",
    "historical_collection": {"date": "2026-07-19", "apiversion": None, "original_seed": None},
    "current_recapture": {
      "date": "2026-08-26",
      "apiversion": API_VERSION,
      "response_bodies_in_repository": False,
      "response_hashes_recorded": 60,
      "retention": "Raw response bodies are not included; this record retains hashes, cells, and normalized values.",
    },
    "records": records,
  }


def civil_jd(year: int, month: int, day: int, time_text: str) -> str:
  hour, minute = (int(value) for value in time_text.split(":"))
  days = (date(year, month, day) - date(2000, 1, 1)).days
  value = Decimal("2451544.5") + days + Decimal(hour * 60 + minute) / Decimal(1440)
  return format(value.quantize(Decimal("0.000001"), rounding=ROUND_HALF_UP), "f")


def v14_source_values(repo_root: Path) -> dict[str, list[str]]:
  source = (repo_root / "src/test/astro/moon_phase_test.cpp").read_text(encoding="utf-8")
  names = {
    "New Moon": "usno_new_moon_2024",
    "First Quarter": "usno_first_quarter_2024",
    "Full Moon": "usno_full_moon_2024",
    "Last Quarter": "usno_last_quarter_2024",
  }
  values = {}
  for phase, name in names.items():
    table = block(source, f"const std::vector<double> {name} {{", "};")
    values[phase] = re.findall(r"\d+\.\d+", table)
  require(sum(len(rows) for rows in values.values()) == 50, "V14 repository phase count differs")
  return values


def hko_new_moons(repo_root: Path) -> list[str]:
  source = (repo_root / "src/test/astro/moon_phase_test.cpp").read_text(encoding="utf-8")
  diff_test = block(source, "TEST(NewMoon, DiffTest2)", "TEST(NewMoon, MomentsYearBoundaryIsUtc)")
  rows = re.findall(
    r"to_ymd\((\d+),\s*(\d+),\s*(\d+)\),\s*hms\s*\{\s*(\d+)h\s*\+\s*(\d+)min\s*\}",
    diff_test,
  )
  require(len(rows) == 13, f"V03 HKO rows={len(rows)}, expected 13")
  utc = []
  for year, month, day, hour, minute in rows:
    local = datetime(int(year), int(month), int(day), int(hour), int(minute), tzinfo=timezone(timedelta(hours=8)))
    utc.append(local.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%MZ"))
  return utc


def build_v14(repo_root: Path, evidence: Path) -> dict:
  response = evidence / "v14-year-2024.json"
  payload = json.loads(response.read_bytes())
  require(payload.get("apiversion") == API_VERSION, "V14 API version differs")
  require(payload.get("numphases") == 50, "V14 response count differs")
  source_values = v14_source_values(repo_root)
  by_phase = {phase: [] for phase in source_values}
  events = []
  new_moons = []
  for entry in payload["phasedata"]:
    jd = civil_jd(entry["year"], entry["month"], entry["day"], entry["time"])
    by_phase[entry["phase"]].append(jd)
    utc = f"{entry['year']:04d}-{entry['month']:02d}-{entry['day']:02d}T{entry['time']}Z"
    events.append({**entry, "utc": utc, "civil_jd": jd})
    if entry["phase"] == "New Moon":
      new_moons.append(utc)
  require(by_phase == source_values, "V14 six-decimal civil-JD relation differs")

  hko = hko_new_moons(repo_root)
  require(len(hko) == len(new_moons) == 13, "V03/USNO new-moon count differs")
  boundary = []
  for hko_utc, usno_utc in zip(hko, new_moons, strict=True):
    relation = "match" if hko_utc == usno_utc else "mismatch_one_minute"
    boundary.append({"hko_utc": hko_utc, "usno_utc": usno_utc, "relation": relation})
  require(sum(row["relation"] == "match" for row in boundary) == 12, "V03 exact-match count differs")
  mismatches = [row for row in boundary if row["relation"] != "match"]
  require(
    mismatches
    == [{"hko_utc": "2024-09-03T01:56Z", "usno_utc": "2024-09-03T01:55Z", "relation": "mismatch_one_minute"}],
    "V03 September boundary differs",
  )
  return {
    "schema": 1,
    "row": "V14",
    "endpoint": "https://aa.usno.navy.mil/api/moon/phases/year",
    "request": {"year": "2024"},
    "historical_collection": {"date": "2026-08-11", "apiversion": None, "response_body_retained": False},
    "current_recapture": {
      "date": "2026-08-26",
      "apiversion": API_VERSION,
      "raw_response_body_in_repository": False,
      "retention": "The raw response body is not included; this record retains its hash and normalized events.",
      "response_sha256": sha256(response),
      "numphases": 50,
    },
    "conversion": {
      "input": "UTC civil minute",
      "delta_t_applied": False,
      "decimal_places": 6,
      "rounding": "ROUND_HALF_UP",
    },
    "events": events,
    "v03_hko_boundary": {
      "status": "open",
      "matching_minutes": 12,
      "mismatching_minutes": 1,
      "events": boundary,
    },
  }


def delta_t_rows(path: Path) -> list[tuple[int, int, int, str]]:
  rows = []
  for line in path.read_text(encoding="ascii").splitlines():
    year, month, day, value = line.split()
    rows.append((int(year), int(month), int(day), value))
  return rows


def build_v26(repo_root: Path, evidence: Path) -> dict:
  tracked = repo_root / "statistics/usno_data.txt"
  retained = evidence / "v26-current-deltat.data"
  require(tracked.read_bytes() == retained.read_bytes(), "V26 retained response differs from statistics/usno_data.txt")
  rows = delta_t_rows(tracked)
  values = {(year, month, day): value for year, month, day, value in rows}
  anchors = [
    {
      "condition": "V27 year 2000",
      "date": "2000-01-01",
      "source_value": values[(2000, 1, 1)],
      "repository_path": "src/test/astro/julian_day_test.cpp",
      "repository_value": "63.83",
      "rounding_places": 2,
    },
    {
      "condition": "V25 year 2010",
      "date": "2010-01-01",
      "source_value": values[(2010, 1, 1)],
      "repository_path": "src/test/astro/delta_t_test_helper.hpp",
      "repository_value": "66.1",
      "rounding_places": 1,
    },
    {
      "condition": "V25 year 2014",
      "date": "2014-01-01",
      "source_value": values[(2014, 1, 1)],
      "repository_path": "src/test/astro/delta_t_test_helper.hpp",
      "repository_value": "67.3",
      "rounding_places": 1,
    },
  ]
  return {
    "schema": 1,
    "row": "V26",
    "source": {
      "url": "https://maia.usno.navy.mil/ser7/deltat.data",
      "product_url": "https://maia.usno.navy.mil/products/deltaT",
      "distribution": "Distribution Statement A. Approved for public release: distribution unlimited.",
    },
    "tracked_copy": {
      "path": "statistics/usno_data.txt",
      "retrieved": "2026-08-01",
      "retrieval": "direct curl of the source URL",
      "sha256": sha256(tracked),
      "bytes": len(tracked.read_bytes()),
      "rows": len(rows),
      "first_date": "1973-02-01",
      "last_date": "2026-04-01",
      "last_modified": "Thu, 18 Jun 2026 17:26:25 GMT",
      "last_modified_observed": "2026-08-26",
    },
    "revision": {
      "previous_commit": "202a0bdd2e26198988ac256b95ec6f65625597c3",
      "current_commit": "9ecb18ac19b30f69d7887d44a9b87ed638ae2760",
      "previous_sha256": "f3ebe52ca09176b8b84b01af53182eec83f52adc45b68ce737e8eb0b81a37ad9",
      "previous_rows": 617,
      "overlap_rows": 617,
      "unchanged_overlap_values": 616,
      "appended_rows": 22,
      "format_change": "one leading space added to every row",
      "changed_values": [{"date": "2024-06-01", "before": "69.2045", "after": "69.2044"}],
    },
    "citation_relations": anchors,
  }


def build_v29(evidence: Path) -> dict:
  response = evidence / "v29-jd-api.json"
  payload = json.loads(response.read_bytes())
  require(payload.get("apiversion") == API_VERSION, "V29 API version differs")
  require(len(payload.get("data", [])) == 1, "V29 response row count differs")
  return {
    "schema": 1,
    "row": "V29",
    "service_url": "https://aa.usno.navy.mil/data/JulianDate",
    "endpoint": "https://aa.usno.navy.mil/api/juliandate",
    "request": {"date": "2024-06-01", "time": "12:00:00"},
    "historical_collection": {"date": None, "apiversion": None, "response_body_retained": False},
    "current_recapture": {
      "date": "2026-08-26",
      "raw_response_body_in_repository": False,
      "normalized_response_embedded": True,
      "retention": "The raw response body is not included; this record embeds its parsed response and raw-body hash.",
      "response_sha256": sha256(response),
      "response": payload,
    },
    "repository_relation": {"path": "src/test/shared_lib/cabi_smoke_test.cpp", "jd": "2460463.000000"},
  }


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
    "--evidence-dir",
    type=Path,
    required=True,
    help="directory containing the retained V12/V13/V14/V26/V29 responses",
  )
  parser.add_argument("--output-dir", type=Path, default=OUTPUT_ROOT)
  parser.add_argument(
    "--write",
    action="store_true",
    help="write records instead of comparing them to the output directory",
  )
  args = parser.parse_args()

  records = {
    "v12-rstt-oneday.json": build_v12(REPO_ROOT, args.evidence_dir),
    "v13-siderealtime.json": build_v13(REPO_ROOT, args.evidence_dir),
    "v14-moon-phases-year-2024.json": build_v14(REPO_ROOT, args.evidence_dir),
    "v26-deltat.json": build_v26(REPO_ROOT, args.evidence_dir),
    "v29-juliandate.json": build_v29(args.evidence_dir),
  }
  args.output_dir.mkdir(parents=True, exist_ok=True)
  for name, payload in records.items():
    path = args.output_dir / name
    expected = json_bytes(payload)
    if args.write:
      path.write_bytes(expected)
    else:
      require(path.read_bytes() == expected, f"USNO record differs: {name}")
    print(f"{hashlib.sha256(expected).hexdigest()}  {name}")


if __name__ == "__main__":
  main()
