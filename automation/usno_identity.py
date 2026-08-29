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
import json
import re

from dataclasses import dataclass
from datetime import date, datetime, timedelta, timezone
from decimal import Decimal, ROUND_FLOOR, ROUND_HALF_UP
from pathlib import Path
from typing import Final, Mapping


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
USNO_ROOT: Final[Path] = REPO_ROOT / "src" / "test" / "provenance" / "usno" / "2026-08-26"
API_VERSION: Final[str] = "4.0.1"
NUMBER: Final[str] = r"[+-]?(?:\d+\.\d+|\d+\.|\.\d+|\d+)"
USNO_RECORD_SHA256: Final[dict[str, str]] = {
  "v12-rstt-oneday.json": "4eb0687ca55f0be00a8ee265e3b05dc586e093f7e19e0ba0536f705106b26d77",
  "v13-siderealtime.json": "129dc55146f8f103cda3691d2ec3a14570413b8d07f4719ef8090b6f8409d658",
  "v14-moon-phases-year-2024.json": "86d0bf8c6aeaa2d25729d6252709d65d508f20f8533d266eb034bdc3d876979e",
  "v26-deltat.json": "c6574d897385bc84f8ebe5b6d392c666d3bf81583e8a6ac83e91ab080560b998",
  "v29-juliandate.json": "25e368ba28f4d0874a89d588205e9658d0bf7aff09eafa54b6852691b25b99da",
}


@dataclass(frozen=True)
class IdentityCounts:
  v12_records: int
  v13_records: int
  v14_events: int
  v03_matching_minutes: int
  v03_mismatching_minutes: int
  v26_rows: int
  v26_citation_relations: int
  v29_responses: int


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _record(path: Path, expected_hash: str) -> dict:
  data = path.read_bytes()
  digest = hashlib.sha256(data).hexdigest()
  _require(digest == expected_hash, f"USNO record hash mismatch for {path.name}: {digest}")
  payload = json.loads(data)
  _require(payload.get("schema") == 1, f"USNO record schema differs for {path.name}")
  return payload


def _block(text: str, start: str, end: str) -> str:
  start_at = text.index(start)
  return text[start_at : text.index(end, start_at)]


def _decimal_text(value: Decimal) -> str:
  text = format(value, "f")
  if "." in text:
    text = text.rstrip("0").rstrip(".")
  return "0" if text in {"-0", ""} else text


def _response_identity(response: dict, label: str) -> None:
  _require(response.get("apiversion") == API_VERSION, f"{label} API version differs")
  _require(
    re.fullmatch(r"[0-9a-f]{64}", response.get("sha256", "")) is not None,
    f"{label} response hash format differs",
  )
  _require(type(response.get("bytes")) is int and response["bytes"] > 0, f"{label} response byte count differs")


def _v12_source_rows(repo_root: Path) -> list[dict[str, str | int]]:
  source = (repo_root / "src/test/astro/rise_set_moon_golden_test.cpp").read_text(encoding="utf-8")
  table = _block(source, "const std::vector<MoonRow> USNO_ROWS {", "};")
  pattern = re.compile(
    rf"\{{\s*(\d+),\s*(\d+),\s*(\d+),\s*({NUMBER}),\s*({NUMBER}),\s*"
    r'"([^"]*)",\s*"([^"]*)",\s*"([^"]*)"\s*\}'
  )
  return [
    {
      "year": int(match.group(1)),
      "month": int(match.group(2)),
      "day": int(match.group(3)),
      "lat": _decimal_text(Decimal(match.group(4))),
      "lon": _decimal_text(Decimal(match.group(5))),
      "rise": match.group(6).strip(),
      "transit": match.group(7).strip(),
      "set": match.group(8).strip(),
    }
    for match in pattern.finditer(table)
  ]


def _verify_v12(record: dict, repo_root: Path) -> int:
  _require(record["row"] == "V12", "V12 row label differs")
  _require(record["endpoint"] == "https://aa.usno.navy.mil/api/rstt/oneday", "V12 endpoint differs")
  _require(record["query_parameter_order"] == ["date", "coords", "tz"], "V12 request shape differs")
  _require(record["coordinate_order"] == "latitude,east-positive-longitude", "V12 coordinate order differs")
  _require(
    record["normalization"] == "last event wins for duplicate phenomena; absent phenomenon is an empty string",
    "V12 normalization rule differs",
  )
  _require(
    record["historical_collection"] == {"date": "2026-08-15", "apiversion": None, "response_bodies_retained": False},
    "V12 historical boundary differs",
  )
  _require(
    record["current_recapture"]
    == {
      "date": "2026-08-26",
      "apiversion": API_VERSION,
      "response_bodies_in_repository": False,
      "response_hashes_recorded": 181,
      "retention": "Raw response bodies are not included; this record retains hashes and normalized cells.",
    },
    "V12 recapture metadata differs",
  )
  source_rows = _v12_source_rows(repo_root)
  records = record["records"]
  _require(len(source_rows) == len(records) == 181, "V12 record count differs")
  for index, (source, item) in enumerate(zip(source_rows, records, strict=True)):
    _require(item["index"] == index, f"V12 record order differs at {index}")
    expected_request = {
      "date": f"{source['year']:04d}-{source['month']:02d}-{source['day']:02d}",
      "coords": f"{source['lat']},{source['lon']}",
      "tz": "0",
    }
    _require(item["request"] == expected_request, f"V12 request differs at {index}")
    _response_identity(item["response"], f"V12 record {index}")
    expected_cells = {name: source[name] for name in ("rise", "transit", "set")}
    _require(item["normalized_cells"] == expected_cells, f"V12 normalized cell differs at {index}")
  return len(records)


def _v13_source_rows(repo_root: Path) -> tuple[list[tuple[str, ...]], list[tuple[str, ...]]]:
  source = (repo_root / "src/test/astro/sidereal_time_test.cpp").read_text(encoding="utf-8")
  greenwich = _block(source, "TEST(SiderealTime, GreenwichUsno)", "TEST(SiderealTime, LocalApparentUsno)")
  local = _block(source, "TEST(SiderealTime, LocalApparentUsno)", "} // namespace astro::sidereal::test")
  pattern = re.compile(rf"\{{\s*({NUMBER}),\s*({NUMBER}),\s*({NUMBER}),\s*({NUMBER})\s*\}}")
  return (
    [match.groups() for match in pattern.finditer(greenwich)],
    [match.groups() for match in pattern.finditer(local)],
  )


def _jd_request_time(jd: str) -> tuple[str, str]:
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


def _hms_degrees(value: str) -> str:
  hour, minute, second = value.split(":")
  degrees = (Decimal(hour) + Decimal(minute) / 60 + Decimal(second) / 3600) * 15
  return format(degrees.quantize(Decimal("0.000000000001"), rounding=ROUND_HALF_UP), "f")


def _normalized_degrees(value: str) -> str:
  return format(Decimal(value).quantize(Decimal("0.000000000001"), rounding=ROUND_HALF_UP), "f")


def _verify_v13(record: dict, repo_root: Path) -> int:
  _require(record["row"] == "V13", "V13 row label differs")
  _require(record["endpoint"] == "https://aa.usno.navy.mil/api/siderealtime", "V13 endpoint differs")
  _require(
    record["query_parameter_order"] == ["date", "time", "coords", "reps", "intv_mag", "intv_unit"],
    "V13 request shape differs",
  )
  _require(record["coordinate_order"] == "latitude,east-positive-longitude", "V13 coordinate order differs")
  _require(record["repository_longitude_convention"] == "west-positive", "V13 repository longitude convention differs")
  _require(
    record["normalization"] == {"hms_to_degrees": "(hours + minutes/60 + seconds/3600) * 15", "decimal_places": 12},
    "V13 normalization differs",
  )
  _require(record["delta_t_relation_seconds"] == "69.184", "V13 Delta-T relation differs")
  _require(
    record["historical_collection"] == {"date": "2026-07-19", "apiversion": None, "original_seed": None},
    "V13 historical boundary differs",
  )
  _require(
    record["current_recapture"]
    == {
      "date": "2026-08-26",
      "apiversion": API_VERSION,
      "response_bodies_in_repository": False,
      "response_hashes_recorded": 60,
      "retention": "Raw response bodies are not included; this record retains hashes, cells, and normalized values.",
    },
    "V13 recapture metadata differs",
  )

  greenwich_rows, local_rows = _v13_source_rows(repo_root)
  source = [("greenwich", index, row) for index, row in enumerate(greenwich_rows)]
  source.extend(("local", index, row) for index, row in enumerate(local_rows))
  records = record["records"]
  _require(len(greenwich_rows) == 40 and len(local_rows) == 20 and len(records) == 60, "V13 record count differs")
  for item, (kind, index, row) in zip(records, source, strict=True):
    _require(item["kind"] == kind and item["index"] == index, f"V13 record order differs at {kind} {index}")
    repository = item["repository"]
    _require(
      repository["jd_ut1"] == row[0] and repository["jde_tt"] == row[1],
      f"V13 JD relation differs at {kind} {index}",
    )
    delta_t = (Decimal(row[1]) - Decimal(row[0])) * Decimal(86400)
    _require(
      delta_t.quantize(Decimal("0.001"), rounding=ROUND_HALF_UP) == Decimal("69.184"),
      f"V13 Delta-T differs at {kind} {index}",
    )
    date_text, time_text = _jd_request_time(row[0])
    request = item["request"]
    expected_common = {"date": date_text, "time": time_text, "reps": "1", "intv_mag": "1", "intv_unit": "days"}
    _response_identity(item["response"], f"V13 {kind} {index}")
    cells = item["response"]["cells"]
    if kind == "greenwich":
      expected_coords = "0,0"
      _require(
        repository["expected_gmst_deg"] == row[2] and repository["expected_gast_deg"] == row[3],
        f"V13 Greenwich repository values differ at {index}",
      )
      expected = {"gmst_deg": _normalized_degrees(row[2]), "gast_deg": _normalized_degrees(row[3])}
      normalized = {"gmst_deg": _hms_degrees(cells["gmst"]), "gast_deg": _hms_degrees(cells["gast"])}
    else:
      _require(repository["longitude_west"] == row[2], f"V13 west longitude differs at {index}")
      lon_east = _decimal_text(-Decimal(row[2]))
      expected_coords = f"0,{lon_east}"
      _require(repository["expected_last_deg"] == row[3], f"V13 local repository value differs at {index}")
      expected = {"last_deg": _normalized_degrees(row[3])}
      normalized = {"last_deg": _hms_degrees(cells["last"])}
    _require(request == {**expected_common, "coords": expected_coords}, f"V13 request differs at {kind} {index}")
    _require(item["normalized_cells"] == normalized == expected, f"V13 normalized cell differs at {kind} {index}")
  return len(records)


def _v14_source_values(repo_root: Path) -> dict[str, list[str]]:
  source = (repo_root / "src/test/astro/moon_phase_test.cpp").read_text(encoding="utf-8")
  names = {
    "New Moon": "usno_new_moon_2024",
    "First Quarter": "usno_first_quarter_2024",
    "Full Moon": "usno_full_moon_2024",
    "Last Quarter": "usno_last_quarter_2024",
  }
  values = {}
  for phase, name in names.items():
    table = _block(source, f"const std::vector<double> {name} {{", "};")
    values[phase] = re.findall(r"\d+\.\d+", table)
  return values


def _civil_jd(year: int, month: int, day: int, time_text: str) -> str:
  hour, minute = (int(value) for value in time_text.split(":"))
  days = (date(year, month, day) - date(2000, 1, 1)).days
  value = Decimal("2451544.5") + days + Decimal(hour * 60 + minute) / Decimal(1440)
  return format(value.quantize(Decimal("0.000001"), rounding=ROUND_HALF_UP), "f")


def _hko_new_moons(repo_root: Path) -> list[str]:
  source = (repo_root / "src/test/astro/moon_phase_test.cpp").read_text(encoding="utf-8")
  diff_test = _block(source, "TEST(NewMoon, DiffTest2)", "TEST(NewMoon, MomentsYearBoundaryIsUtc)")
  rows = re.findall(
    r"to_ymd\((\d+),\s*(\d+),\s*(\d+)\),\s*hms\s*\{\s*(\d+)h\s*\+\s*(\d+)min\s*\}",
    diff_test,
  )
  utc = []
  for year, month, day, hour, minute in rows:
    local = datetime(int(year), int(month), int(day), int(hour), int(minute), tzinfo=timezone(timedelta(hours=8)))
    utc.append(local.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%MZ"))
  return utc


def _verify_v14(record: dict, repo_root: Path) -> tuple[int, int, int]:
  _require(record["row"] == "V14", "V14 row label differs")
  _require(record["endpoint"] == "https://aa.usno.navy.mil/api/moon/phases/year", "V14 endpoint differs")
  _require(record["request"] == {"year": "2024"}, "V14 request differs")
  _require(
    record["historical_collection"] == {"date": "2026-08-11", "apiversion": None, "response_body_retained": False},
    "V14 historical boundary differs",
  )
  recapture = record["current_recapture"]
  _require(
    recapture["date"] == "2026-08-26"
    and recapture["apiversion"] == API_VERSION
    and recapture["raw_response_body_in_repository"] is False
    and recapture["retention"]
    == "The raw response body is not included; this record retains its hash and normalized events.",
    "V14 recapture metadata differs",
  )
  _require(re.fullmatch(r"[0-9a-f]{64}", recapture["response_sha256"]) is not None, "V14 response hash differs")
  _require(recapture["numphases"] == 50, "V14 response count differs")
  _require(
    record["conversion"]
    == {
      "input": "UTC civil minute",
      "delta_t_applied": False,
      "decimal_places": 6,
      "rounding": "ROUND_HALF_UP",
    },
    "V14 six-decimal formatting rule differs",
  )

  source_values = _v14_source_values(repo_root)
  by_phase = {phase: [] for phase in source_values}
  new_moons = []
  events = record["events"]
  _require(len(events) == 50, "V14 event count differs")
  for event in events:
    expected_utc = f"{event['year']:04d}-{event['month']:02d}-{event['day']:02d}T{event['time']}Z"
    expected_jd = _civil_jd(event["year"], event["month"], event["day"], event["time"])
    _require(event["utc"] == expected_utc, "V14 UTC event differs")
    _require(
      event["civil_jd"] == expected_jd and re.fullmatch(r"\d+\.\d{6}", expected_jd),
      "V14 civil-JD conversion differs",
    )
    by_phase[event["phase"]].append(expected_jd)
    if event["phase"] == "New Moon":
      new_moons.append(expected_utc)
  _require(by_phase == source_values, "V14 repository six-decimal values differ")

  hko = _hko_new_moons(repo_root)
  boundary = record["v03_hko_boundary"]
  _require(boundary["status"] == "open", "V03 was incorrectly closed")
  _require(len(hko) == len(new_moons) == len(boundary["events"]) == 13, "V03 boundary count differs")
  expected_boundary = []
  for hko_utc, usno_utc in zip(hko, new_moons, strict=True):
    expected_boundary.append(
      {"hko_utc": hko_utc, "usno_utc": usno_utc, "relation": "match" if hko_utc == usno_utc else "mismatch_one_minute"}
    )
  _require(boundary["events"] == expected_boundary, "V03 HKO/USNO minute relation differs")
  matches = sum(item["relation"] == "match" for item in expected_boundary)
  mismatches = [item for item in expected_boundary if item["relation"] != "match"]
  _require(boundary["matching_minutes"] == matches == 12, "V03 matching-minute count differs")
  _require(boundary["mismatching_minutes"] == len(mismatches) == 1, "V03 mismatch count differs")
  _require(
    mismatches
    == [{"hko_utc": "2024-09-03T01:56Z", "usno_utc": "2024-09-03T01:55Z", "relation": "mismatch_one_minute"}],
    "V03 September HKO/USNO mismatch differs",
  )
  return len(events), matches, len(mismatches)


def _delta_t_rows(path: Path) -> list[tuple[int, int, int, str]]:
  rows = []
  for line in path.read_text(encoding="ascii").splitlines():
    year, month, day, value = line.split()
    rows.append((int(year), int(month), int(day), value))
  return rows


def _delta_t_repository_rows(repo_root: Path) -> dict[int, Decimal]:
  source = (repo_root / "src/test/astro/delta_t_test_helper.hpp").read_text(encoding="utf-8")
  table = _block(source, "const inline DatasetType ACCURATE_DELTA_T_TABLE", "};")
  return {int(year): Decimal(value) for year, value in re.findall(r"\{\s*(\d{4})\.0,\s*(\d+\.\d+)\s*\}", table)}


def _verify_v26(record: dict, repo_root: Path) -> tuple[int, int]:
  _require(record["row"] == "V26", "V26 row label differs")
  source = record["source"]
  _require(source["url"] == "https://maia.usno.navy.mil/ser7/deltat.data", "V26 source URL differs")
  _require(source["product_url"] == "https://maia.usno.navy.mil/products/deltaT", "V26 product URL differs")
  _require(
    source["distribution"] == "Distribution Statement A. Approved for public release: distribution unlimited.",
    "V26 distribution statement differs",
  )
  tracked = record["tracked_copy"]
  expected_metadata = {
    "path": "statistics/usno_data.txt",
    "retrieved": "2026-08-01",
    "retrieval": "direct curl of the source URL",
    "sha256": "9f88e53593495a09219fe956eeadea0fa9f8e3e02c310b2aa2b70852383cdf6f",
    "bytes": 13419,
    "rows": 639,
    "first_date": "1973-02-01",
    "last_date": "2026-04-01",
    "last_modified": "Thu, 18 Jun 2026 17:26:25 GMT",
    "last_modified_observed": "2026-08-26",
  }
  _require(tracked == expected_metadata, "V26 URL/hash/header metadata differs")
  data_path = repo_root / tracked["path"]
  data = data_path.read_bytes()
  _require(hashlib.sha256(data).hexdigest() == tracked["sha256"], "V26 tracked hash differs")
  _require(len(data) == tracked["bytes"], "V26 tracked byte count differs")
  rows = _delta_t_rows(data_path)
  _require(len(rows) == tracked["rows"], "V26 row count differs")
  _require(rows[0][:3] == (1973, 2, 1) and rows[-1][:3] == (2026, 4, 1), "V26 date range differs")

  expected_revision = {
    "previous_commit": "202a0bdd2e26198988ac256b95ec6f65625597c3",
    "current_commit": "9ecb18ac19b30f69d7887d44a9b87ed638ae2760",
    "previous_sha256": "f3ebe52ca09176b8b84b01af53182eec83f52adc45b68ce737e8eb0b81a37ad9",
    "previous_rows": 617,
    "overlap_rows": 617,
    "unchanged_overlap_values": 616,
    "appended_rows": 22,
    "format_change": "one leading space added to every row",
    "changed_values": [{"date": "2024-06-01", "before": "69.2045", "after": "69.2044"}],
  }
  _require(record["revision"] == expected_revision, "V26 revision relation differs")

  source_values = {(year, month, day): value for year, month, day, value in rows}
  expected_relations = (
    ("V27 year 2000", (2000, 1, 1), "63.8285", "src/test/astro/julian_day_test.cpp", "63.83", 2),
    ("V25 year 2010", (2010, 1, 1), "66.0699", "src/test/astro/delta_t_test_helper.hpp", "66.1", 1),
    ("V25 year 2014", (2014, 1, 1), "67.2810", "src/test/astro/delta_t_test_helper.hpp", "67.3", 1),
  )
  relations = record["citation_relations"]
  repository_values = _delta_t_repository_rows(repo_root)
  _require(len(relations) == len(expected_relations), "V26 citation relation count differs")
  for relation, expected_relation in zip(relations, expected_relations, strict=True):
    condition, ymd, source_value, repository_path, repository_value, places = expected_relation
    _require(relation["condition"] == condition, f"{condition} label differs")
    _require(relation["date"] == date(*ymd).isoformat(), f"{condition} date differs")
    _require(relation["source_value"] == source_values[ymd] == source_value, f"{condition} source value differs")
    _require(relation["repository_path"] == repository_path, f"{condition} repository path differs")
    _require(relation["repository_value"] == repository_value, f"{condition} repository value differs")
    _require(relation["rounding_places"] == places, f"{condition} rounding precision differs")
    quantum = Decimal(1).scaleb(-places)
    rounded = Decimal(source_value).quantize(quantum, rounding=ROUND_HALF_UP)
    _require(rounded == Decimal(repository_value), f"{condition} rounding relation differs")
    if condition == "V27 year 2000":
      julian_day = (repo_root / repository_path).read_text(encoding="utf-8")
      _require("63.8285 s" in julian_day and "stored 63.83 s" in julian_day, f"{condition} target value differs")
      _require("23h + 58min + 56s + 170ms" in julian_day, f"{condition} anchor differs")
    else:
      _require(repository_values[ymd[0]] == Decimal(repository_value), f"{condition} target value differs")
  return len(rows), len(relations)


def _verify_v29(record: dict, repo_root: Path) -> int:
  _require(record["row"] == "V29", "V29 row label differs")
  _require(record["service_url"] == "https://aa.usno.navy.mil/data/JulianDate", "V29 service URL differs")
  _require(record["endpoint"] == "https://aa.usno.navy.mil/api/juliandate", "V29 endpoint differs")
  _require(record["request"] == {"date": "2024-06-01", "time": "12:00:00"}, "V29 request differs")
  _require(
    record["historical_collection"] == {"date": None, "apiversion": None, "response_body_retained": False},
    "V29 historical boundary differs",
  )
  recapture = record["current_recapture"]
  _require(
    recapture["date"] == "2026-08-26"
    and recapture["raw_response_body_in_repository"] is False
    and recapture["normalized_response_embedded"] is True
    and recapture["retention"]
    == "The raw response body is not included; this record embeds its parsed response and raw-body hash.",
    "V29 recapture metadata differs",
  )
  _require(
    recapture["response_sha256"] == "3637571ae2f4c4b7bca03e7e57d99ba1d14982a5b339b4efd1aa97dc967e6c84",
    "V29 response hash differs",
  )
  response = recapture["response"]
  _require(response["apiversion"] == API_VERSION and len(response["data"]) == 1, "V29 API response shape differs")
  row = response["data"][0]
  expected = {"day": 1, "era": "AD", "jd": "2460463.000000", "month": 6, "time": "12:00:00.0", "tz": 0, "year": 2024}
  _require(row == expected, "V29 timezone/era/JD response differs")
  _require(
    record["repository_relation"] == {"path": "src/test/shared_lib/cabi_smoke_test.cpp", "jd": "2460463.000000"},
    "V29 repository relation differs",
  )
  source = (repo_root / record["repository_relation"]["path"]).read_text(encoding="utf-8")
  _require("ut1_to_jd(2024, 6, 1, 0.5)" in source, "V29 repository request relation differs")
  _require("ASSERT_NEAR(jd.value, 2460463.0, 1e-6)" in source, "V29 repository JD differs")
  _require("era=AD, tz=0, JD 2460463.000000" in source, "V29 repository response citation differs")
  return 1


def verify_usno_identities(
  repo_root: Path = REPO_ROOT,
  usno_root: Path = USNO_ROOT,
  record_hashes: Mapping[str, str] = USNO_RECORD_SHA256,
) -> IdentityCounts:
  _require(set(record_hashes) == set(USNO_RECORD_SHA256), "USNO record inventory differs")
  _require(
    {path.name for path in usno_root.iterdir() if path.is_file()} == set(record_hashes),
    "USNO record directory inventory differs",
  )
  records = {name: _record(usno_root / name, digest) for name, digest in record_hashes.items()}
  v12 = _verify_v12(records["v12-rstt-oneday.json"], repo_root)
  v13 = _verify_v13(records["v13-siderealtime.json"], repo_root)
  v14, matches, mismatches = _verify_v14(records["v14-moon-phases-year-2024.json"], repo_root)
  v26, relations = _verify_v26(records["v26-deltat.json"], repo_root)
  v29 = _verify_v29(records["v29-juliandate.json"], repo_root)
  return IdentityCounts(v12, v13, v14, matches, mismatches, v26, relations, v29)


if __name__ == "__main__":
  print(verify_usno_identities())
