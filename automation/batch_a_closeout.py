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
# SPDX-License-Identifier: MIT

import hashlib
import json
import re

from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Final, Sequence

if __package__:
  from .source_digest import canonical_cpp
  from .third_party_notices import NOTICE_SOURCES, NoticeSource
else:
  from source_digest import canonical_cpp
  from third_party_notices import NOTICE_SOURCES, NoticeSource


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
CLOSEOUT_ROOT_RELATIVE: Final[Path] = Path("src/test/provenance/batch-a-closeout")
RECORD_NAME: Final[str] = "record.json"
REGISTRY_NAME: Final[str] = "retained_host_blocks.json"
RECORD_SHA256: Final[str] = "45419b992a095fbe0714cc9c51008234bb8800ec5837c16c79033f6db67b52a9"
REGISTRY_SHA256: Final[str] = "c8f4391b1d8740c74cd2720835e2dce8f8e75b0a4c7425fa892115095319bf64"

DISPOSITION_GROUPS: Final[dict[tuple[str, str, str], frozenset[str]]] = {
  (
    "retained_under_owner_risk_acceptance",
    "retained_third_party_outside_mit",
    "commercial_authorisation_required",
  ): frozenset({"R01", "V01", "V02", "V03", "V04"}),
  (
    "retained_under_owner_risk_acceptance",
    "retained_third_party_outside_mit",
    "no_redistribution_grant_scientific_context",
  ): frozenset({"R12"}),
  (
    "project_authored_classification_acceptance",
    "project_authored_mit",
    "project_authored_classification",
  ): frozenset(
    {
      "R03",
      "R04",
      "R15",
      "R17",
      "R20",
      "R23",
      "R24",
      "R25",
      "R26",
      "R28",
      "R29",
      "R30",
      "R31",
      "R32",
      "R33",
      "R34",
      "R36",
    }
  ),
  (
    "retained_under_owner_risk_acceptance",
    "retained_third_party_outside_mit",
    "written_reservation",
  ): frozenset({"R22", "V32"}),
  (
    "retained_under_owner_risk_acceptance",
    "retained_third_party_outside_mit",
    "symmetric_silence",
  ): frozenset({"V06", "V07", "V08", "V09", "V10", "V11", "V20", "V21", "V22", "V23"}),
  (
    "retained_under_owner_risk_acceptance",
    "retained_third_party_outside_mit",
    "gpl_source_decode",
  ): frozenset({"V05"}),
  (
    "retained_under_owner_risk_acceptance",
    "retained_third_party_outside_mit",
    "output_conditional",
  ): frozenset({"V15", "V16", "V17", "V18", "V19"}),
  (
    "retained_under_owner_risk_acceptance",
    "retained_third_party_outside_mit",
    "no_terms_no_snapshot",
  ): frozenset({"V30"}),
  ("recorded_historical_boundary", "project_authored_mit", "not_applicable"): frozenset(
    {"R02", "V24", "V31", "V33", "V35", "V38"}
  ),
  (
    "recorded_historical_boundary",
    "retained_third_party_outside_mit",
    "no_terms_no_snapshot",
  ): frozenset({"V28"}),
  ("closed_by_existing_evidence", "project_authored_mit", "not_applicable"): frozenset({"V34"}),
  ("closed_by_existing_evidence", "no_active_stored_bytes", "not_applicable"): frozenset({"V36"}),
  (
    "closed_by_existing_evidence",
    "retained_third_party_outside_mit",
    "output_conditional",
  ): frozenset({"V37"}),
  (
    "retained_under_owner_risk_acceptance",
    "retained_third_party_outside_mit",
    "eula_text_unrecovered",
  ): frozenset({"T03"}),
  ("out_of_denominator", "no_active_stored_bytes", "not_applicable"): frozenset({"V39", "V40", "V41"}),
  ("out_of_denominator", "citation_only", "not_applicable"): frozenset({"V43"}),
}
EXPECTED_ROWS: Final[frozenset[str]] = frozenset().union(*DISPOSITION_GROUPS.values())

SPLIT_ROW_PARTS: Final[dict[str, str]] = {
  "R17": "6378.14 km radius and parallax-geometry remainder",
  "R23": "Meeus equation 25.11 implementation remainder",
  "R34": "365.2422-day and 29.530588853-day mean-period remainder",
  "V11": "Skyfield twilight leg; USNO and NOAA legs remain closed by their existing terms evidence",
  "V22": "V22b generated JDEs; V22a leap facts are closed by SOFA evidence",
}

REQUIRED_REGISTRY_IDS: Final[frozenset[str]] = frozenset(
  """
  r01-algo1 r01-algo3 r05 r06 r07 r09 r10 r11-forward r11-reverse r12 r13 r14
  r16-longitude r16-latitude r17-baseline r18 r19 r21 r22 r23-constant r27 r34-julian r34-au
  r37-t01 t03-native t03-wheel v01-algo1-test v01-algo3-test v02-algo2 v02-common v02-diff
  v02-cabi v03 v04-test v04-automation v05 v06 v07 v07-refresh v08 v09 v10 v11 v11-refresh
  v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22-sofa v22-pyerfa v23 v25 v26 v27 v28 v29 v30 v32-coord
  v32-sidereal v32-precession v32-earth v32-elp v32-phase v32-solar v32-rise-set
  v32-refraction v32-julian v32-cabi v37-earth-vsop v37-earth-nutation v37-sun-geometric
  v37-sun-corrected v37-moon-coord v37-moon-perturbation v37-elp v37-julian notice-emscripten
  notice-musl notice-libcxx notice-libcxxabi notice-libunwind notice-compiler-rt notice-sofa notice-erfa
  """.split()
)

IDENTITY_GATE_HOSTS: Final[frozenset[str]] = frozenset(
  {
    "automation/jieqi_table.py",
    "src/astro/delta_t.hpp",
    "src/astro/earth.hpp",
    "src/astro/earth/precession.hpp",
    "src/astro/elp2000_82b.hpp",
    "src/astro/julian_day.hpp",
    "src/astro/leap_second.hpp",
    "src/astro/moon.hpp",
    "src/astro/toolbox.hpp",
    "src/calendar/lunar/algo1.hpp",
    "src/calendar/lunar/algo3.hpp",
    "src/test/astro/delta_t_test_helper.hpp",
    "src/test/astro/earth_test.cpp",
    "src/test/astro/elp2000_82b_test.cpp",
    "src/test/astro/julian_day_test.cpp",
    "src/test/astro/moon_phase_test.cpp",
    "src/test/astro/moon_test.cpp",
    "src/test/astro/rise_set_golden_test.cpp",
    "src/test/astro/rise_set_moon_golden_test.cpp",
    "src/test/astro/sidereal_time_test.cpp",
    "src/test/astro/sun_test.cpp",
    "src/test/jieqi_golden_test.cpp",
    "src/test/lunar/algo3_test.cpp",
    "src/test/shared_lib/cabi_smoke_test.cpp",
    "statistics/sun_equatorial_horizons_crawler.py",
    "statistics/sunrise_golden_crawler.py",
    "statistics/usno_data.txt",
  }
)

NON_PROJECT_SPDX_HOSTS: Final[frozenset[str]] = frozenset({"THIRD_PARTY_NOTICES.txt", "run-clang-tidy.py"})
MIT_LICENSE_BYTES: Final[bytes] = b"""MIT License

Copyright (c) 2024-2026 Ningqi Wang (0xf3cd)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""
MIT_SPDX_MARKER: Final[str] = "SPDX-License-Identifier: MIT"
OLD_FULL_HEADER_MARKER: Final[str] = "it under the terms of the GNU General " + "Public License"
OLD_SHORT_HEADER_MARKER: Final[str] = "# License: GNU General " + "Public License v3.0"
EXPECTED_PROJECT_SPDX_FILES: Final[int] = 199
A4_SCAN_ROOTS: Final[tuple[str, ...]] = (
  "automation",
  "bindings",
  "docs",
  "src",
  "statistics",
  "third_party",
  "toolbox",
)
A4_SCAN_FILES: Final[tuple[str, ...]] = (
  "AGENTS.md",
  "Dockerfile",
  "LICENSE",
  "README.md",
  "Requirements-producer.in",
  "Requirements-producer.txt",
  "Requirements-statistics.txt",
  "Requirements.txt",
  "THIRD_PARTY_NOTICES.txt",
  "checks.py",
  "project.py",
  "run-clang-tidy.py",
)
A4_SCAN_EXCLUDED_PARTS: Final[frozenset[str]] = frozenset(
  {".git", ".review", ".venv", "__pycache__", "build", "node_modules", "wheelhouse"}
)
GPL_3_OR_LATER: Final[str] = "GPL" + "-3.0-or-later"
GPL_V2: Final[str] = "GPL" + "v2"
GPL_V3: Final[str] = "GPL" + "v3"
LGPL_3_OR_LATER: Final[str] = "LGPL" + "-3.0-or-later"
RESIDUAL_GPL_PATTERN: Final[re.Pattern[str]] = re.compile(
  "GNU General "
  r"Public License|(?<![A-Za-z0-9])L?GPL(?:v[0-9]+|-[0-9]+(?:\.[0-9]+)?(?:-or-later)?)(?![A-Za-z0-9.-])"
)
RESIDUAL_GPL_ALLOWLIST: Final[Counter[tuple[str, str]]] = Counter(
  {
    ("THIRD_PARTY_NOTICES.txt", GPL_V2): 8,
    ("automation/astrotime_delta_t_provenance.py", GPL_3_OR_LATER): 2,
    ("bindings/javascript/package-lock.json", LGPL_3_OR_LATER): 14,
    (
      "src/test/provenance/astrotime-analysis/55115f4bf59cbdc47970b7f2d69a9715a467a3e9/GRANT.md",
      GPL_3_OR_LATER,
    ): 1,
    (
      "src/test/provenance/astrotime-analysis/ed1cdc2fd6c5122b391a82289aa2cc060340552d/GRANT.md",
      GPL_3_OR_LATER,
    ): 1,
    ("src/test/lunar/algo3_ytliu0_golden_test.cpp", GPL_V3): 1,
    ("statistics/algo3_ytliu0_golden.py", GPL_V3): 1,
    ("third_party/emscripten/6.0.6/compiler-rt-LICENSE.TXT", GPL_V2): 2,
    ("third_party/emscripten/6.0.6/libcxx-LICENSE.TXT", GPL_V2): 2,
    ("third_party/emscripten/6.0.6/libcxxabi-LICENSE.TXT", GPL_V2): 2,
    ("third_party/emscripten/6.0.6/libunwind-LICENSE.TXT", GPL_V2): 2,
    ("third_party/llvm/llvmorg-22.1.2/LICENSE.TXT", GPL_V2): 2,
  }
)
ROW_ID_PATTERN: Final[re.Pattern[str]] = re.compile(
  r"\b(?:R(?:0[1-9]|[12][0-9]|3[0-7])|V(?:0[1-9]|[1-3][0-9]|4[0-3])|T0[1-3])\b"
)
MARKING_PHRASES: Final[tuple[str, ...]] = (
  "Retained material boundary",
  "Retained material boundaries",
  "These HKO-derived words remain under their source terms and outside the project MIT grant.",
  "Retained third-party components identified",
)
SOURCE_FAMILIES: Final[tuple[tuple[str, ...], ...]] = (
  ("hong kong observatory", "hko"),
  ("xu jianwei", "寿星万年历"),
  ("nasa/tp-2006-214141", "nasa"),
  ("bretagnon and francou", "vsop87d"),
  ("fred espenak", "espenak"),
  ("iau sofa", "sofa"),
  ("erfa",),
  ("morrison-stephenson-hohenkerk-zawilski", "morrison-stephenson", "zawilski"),
  ("jean meeus", "meeus"),
  ("microsoft",),
  ("ytliu0",),
  ("jpl horizons", "horizons"),
  ("usno", "us naval observatory"),
  ("pymeeus",),
  ("skyfield",),
  ("pyerfa",),
  ("nist",),
  ("llvm", "llvmorg"),
  ("emscripten",),
  ("musl",),
  ("libc++",),
  ("libunwind",),
  ("compiler-rt",),
  ("taipeidaniel",),
  ("stevegs.com",),
  ("internal julian", "internal regression"),
)

EXPECTED_NOTICE_APPLICABILITY: Final[dict[str, frozenset[str]]] = {
  "Emscripten 6.0.6 — LICENSE": frozenset({"notice-emscripten"}),
  "musl — COPYRIGHT": frozenset({"notice-musl"}),
  "libc++ — LICENSE.TXT": frozenset({"notice-libcxx"}),
  "libc++abi — LICENSE.TXT": frozenset({"notice-libcxxabi"}),
  "libunwind — LICENSE.TXT": frozenset({"notice-libunwind"}),
  "compiler-rt — LICENSE.TXT": frozenset({"notice-compiler-rt"}),
  "IAU SOFA issue 2023-10-11 — SOFA Software License": frozenset(
    {"notice-sofa", "r10", "r13", "r14", "r18", "r19", "v22-sofa"}
  ),
  "ERFA v2.0.1 — LICENSE": frozenset(
    {
      "notice-erfa",
      "r11-forward",
      "r11-reverse",
      "r16-latitude",
      "r16-longitude",
      "r17-baseline",
      "r21",
      "r23-constant",
      "r27",
      "r34-au",
      "r34-julian",
    }
  ),
  "NASA/TP-2006-214141 — acknowledgment": frozenset({"r06", "v25", "v27"}),
  "Delta T algorithms 1, 3, and 5 — source attribution": frozenset({"r05", "r07", "r09"}),
  "Hong Kong Observatory — retained lunar-year words": frozenset({"r01-algo1", "r01-algo3"}),
  "VSOP87D — retained Earth coefficients": frozenset({"r12"}),
  "Astronomical Algorithms — retained daily-variation series": frozenset({"r22"}),
  "Microsoft — statically linked C/C++ runtime portions": frozenset({"t03-native", "t03-wheel"}),
}


@dataclass(frozen=True)
class CloseoutCounts:
  rows: int
  retained_blocks: int
  marked_hosts: int
  adjacent_records: int
  notice_mappings: int


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _read_pinned(path: Path, expected_sha256: str, label: str) -> bytes:
  data = path.read_bytes()
  digest = hashlib.sha256(data).hexdigest()
  _require(digest == expected_sha256, f"{label} hash mismatch: {digest}")
  return data


def _object_without_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
  result: dict[str, object] = {}
  for key, value in pairs:
    _require(key not in result, f"duplicate JSON key: {key}")
    result[key] = value
  return result


def _load_json(data: bytes, label: str) -> dict:
  try:
    value = json.loads(data, object_pairs_hook=_object_without_duplicates)
  except (UnicodeDecodeError, json.JSONDecodeError) as error:
    raise RuntimeError(f"{label} is not valid UTF-8 JSON") from error
  _require(isinstance(value, dict), f"{label} root must be an object")
  return value


def _safe_path(repo_root: Path, relative: str, label: str) -> Path:
  path = Path(relative)
  _require(not path.is_absolute() and ".." not in path.parts, f"{label} path must remain repository-relative")
  target = repo_root / path
  _require(target.is_file(), f"{label} path is missing: {relative}")
  return target


def _digest(path: Path, method: str) -> str:
  if method == "raw_sha256":
    data = path.read_bytes()
  elif method == "canonical_cpp":
    data = canonical_cpp(path.read_text(encoding="utf-8")).encode("utf-8")
  else:
    raise RuntimeError(f"unknown direct-digest method: {method}")
  return hashlib.sha256(data).hexdigest()


def _expected_row_shapes() -> dict[str, tuple[str, str, str]]:
  return {row: shape for shape, rows in DISPOSITION_GROUPS.items() for row in rows}


def _verify_row_record(repo_root: Path, record: dict) -> dict[str, dict]:
  _require(set(record) == {"schema", "scope", "rows"}, "closeout record top-level fields differ")
  _require(record["schema"] == 1, "closeout record schema differs")
  _require(isinstance(record["scope"], str) and record["scope"], "closeout record scope is empty")
  rows = record["rows"]
  _require(isinstance(rows, dict), "closeout rows must be an object")
  row_ids = set(rows)
  _require(
    row_ids == EXPECTED_ROWS,
    f"closeout row inventory differs: missing={sorted(EXPECTED_ROWS - row_ids)}, "
    f"unexpected={sorted(row_ids - EXPECTED_ROWS)}",
  )

  expected_shapes = _expected_row_shapes()
  required_fields = {
    "disposition_kind",
    "material_scope",
    "terms_shape",
    "permission_claim",
    "owner_risk_acceptance",
    "destination",
    "source_identity",
    "current_reproducibility",
    "unavailable",
    "evidence",
  }
  optional_fields = {"covered_part", "source_facts"}
  for row_id, row in rows.items():
    _require(isinstance(row, dict), f"{row_id} must be an object")
    fields = set(row)
    _require(required_fields <= fields, f"{row_id} is incomplete: missing={sorted(required_fields - fields)}")
    _require(
      fields <= required_fields | optional_fields, f"{row_id} has unexpected fields: {sorted(fields - required_fields)}"
    )
    shape = (row["disposition_kind"], row["material_scope"], row["terms_shape"])
    _require(shape == expected_shapes[row_id], f"{row_id} disposition/material/terms shape differs")
    _require(row["permission_claim"] is False, f"{row_id} must not claim upstream permission")
    expected_risk = (
      row["material_scope"] == "retained_third_party_outside_mit"
      or row["disposition_kind"] == "project_authored_classification_acceptance"
    )
    _require(row["owner_risk_acceptance"] is expected_risk, f"{row_id} owner-risk acceptance differs")
    for field in ("destination", "source_identity", "current_reproducibility"):
      _require(isinstance(row[field], str) and row[field], f"{row_id} {field} is empty")
    _require(isinstance(row["unavailable"], list), f"{row_id} unavailable fields must be a list")
    _verify_evidence(repo_root, row_id, row["evidence"])

  for row_id, covered_part in SPLIT_ROW_PARTS.items():
    _require(rows[row_id].get("covered_part") == covered_part, f"{row_id} split-row assignment is incomplete")
  _require("covered_part" not in rows["V07"], "V07 must have one complete row entry")
  _require("covered_part" in rows["V11"], "V11 split-row assignment is incomplete")
  _require("covered_part" not in rows["V18"], "V18 must have one complete row entry")
  _require(rows["V18"]["unavailable"] == ["original generator"], "V18 unavailable fields differ")
  _require(
    rows["V03"].get("source_facts")
    == {
      "matching_minutes": 12,
      "mismatching_minutes": 1,
      "retained_hko_utc": "2024-09-03T01:56Z",
      "usno_utc": "2024-09-03T01:55Z",
      "relation": "mismatch_one_minute",
    },
    "V03 HKO/USNO partition differs",
  )
  _require(
    rows["V04"]["source_identity"]
    == "Hong Kong Observatory 2022-2028 solar-term almanac rows based on HMNAO and USNO data"
    and rows["V04"]["unavailable"]
    == ["retained raw XML responses", "upstream redistribution permission", "underlying HMNAO/USNO rights allocation"],
    "V04 underlying-source boundary differs",
  )
  _require(
    rows["V37"]["source_identity"]
    == "seven PyMeeus-derived regression blocks and one internal Julian-day regression block"
    and "internal Julian-row source" in rows["V37"]["unavailable"],
    "V37 source partition differs",
  )
  _require(
    rows["T03"]["current_reproducibility"]
    == "native and wheel capture paths exist; approved evidence and standing contracts await branch captures"
    and rows["T03"]["unavailable"]
    == [
      "captured Enterprise 2026 terms text until exposed by the runner",
      "exact selected runtime/toolset/archive identity until branch capture",
    ],
    "T03 capture boundary differs",
  )
  _verify_vsop87d(repo_root, rows["R12"])
  return rows


def _verify_evidence(repo_root: Path, row_id: str, evidence: object) -> None:
  _require(isinstance(evidence, dict), f"{row_id} evidence must be an object")
  kind = evidence.get("kind")
  if kind == "existing_gate":
    _require(set(evidence) == {"kind", "path"}, f"{row_id} existing-gate evidence fields differ")
    _safe_path(repo_root, evidence["path"], f"{row_id} existing gate")
  elif kind == "implementation_locations":
    _require(set(evidence) == {"kind", "paths"}, f"{row_id} implementation evidence fields differ")
    _require(isinstance(evidence["paths"], list) and evidence["paths"], f"{row_id} implementation paths are empty")
    for relative in evidence["paths"]:
      _safe_path(repo_root, relative, f"{row_id} implementation")
  elif kind == "direct_digest":
    _require(set(evidence) == {"kind", "blocks"}, f"{row_id} direct-digest evidence fields differ")
    blocks = evidence["blocks"]
    _require(isinstance(blocks, list) and blocks, f"{row_id} direct-digest blocks are empty")
    paths: set[str] = set()
    for block in blocks:
      _require(isinstance(block, dict), f"{row_id} direct-digest block must be an object")
      _require(set(block) == {"path", "method", "sha256"}, f"{row_id} direct-digest block fields differ")
      _require(block["path"] not in paths, f"{row_id} direct-digest path is duplicated: {block['path']}")
      paths.add(block["path"])
      path = _safe_path(repo_root, block["path"], f"{row_id} direct digest")
      digest = _digest(path, block["method"])
      _require(digest == block["sha256"], f"{row_id} direct digest differs for {block['path']}: {digest}")
  elif kind == "not_applicable":
    _require(set(evidence) == {"kind"}, f"{row_id} not-applicable evidence fields differ")
  else:
    raise RuntimeError(f"{row_id} evidence kind is unknown: {kind}")


def _verify_vsop87d(repo_root: Path, row: dict) -> None:
  facts = row.get("source_facts")
  _require(isinstance(facts, dict), "R12 source facts are missing")
  expected = {
    "url": "https://ftp.imcce.fr/pub/ephem/planets/vsop87/VSOP87D.ear",
    "source_sha256": "8b160c859136d467f2be7fc29efa8a9652e95516dfbde00e4c739d7ddc90ca91",
    "scaling_pointer": "src/astro/vsop87d/defines.hpp:SCALING_FACTOR",
    "table_count": 17,
    "row_count": 2425,
    "amplitude_spelling_differences": 0,
    "phase_spelling_differences": 0,
    "frequency_spelling_differences": 61,
    "maximum_frequency_spelling_difference": "4e-11",
    "binary64_field_differences": 0,
    "relation": "L0 measurement; source bytes and comparison program are not retained, so it is not replayable offline",
  }
  for key, value in expected.items():
    _require(facts.get(key) == value, f"R12 {key} differs")
  parse_rule = facts.get("parse_rule")
  _require(
    isinstance(parse_rule, str)
    and all(token in parse_rule for token in ("variable 1/2/3", "L/B/R", "amplitude", "phase", "frequency", "1e8")),
    "R12 parse rule differs",
  )
  defines = (repo_root / "src/astro/vsop87d/defines.hpp").read_text(encoding="utf-8")
  _require("inline constexpr double SCALING_FACTOR = 1e8;" in defines, "R12 scaling pointer differs")

  tables = facts.get("repository_tables")
  expected_names = [
    *(f"L{index}" for index in range(6)),
    *(f"B{index}" for index in range(5)),
    *(f"R{index}" for index in range(6)),
  ]
  _require(isinstance(tables, dict) and list(tables) == expected_names, "R12 repository table inventory differs")
  source = (repo_root / "src/astro/vsop87d/earth_coeff.hpp").read_text(encoding="utf-8")
  row_count = 0
  for name in expected_names:
    match = re.search(
      rf"inline constexpr std::array<Coefficients, (\d+)> {name} \{{\{{.*?\n\}}\}};",
      source,
      re.DOTALL,
    )
    _require(match is not None, f"R12 repository table is missing: {name}")
    rows = int(match.group(1))
    digest = hashlib.sha256(canonical_cpp(match.group(0)).encode("utf-8")).hexdigest()
    _require(tables[name] == {"rows": rows, "canonical_cpp_sha256": digest}, f"R12 table record differs: {name}")
    row_count += rows
  _require(row_count == facts["row_count"], f"R12 repository row count differs: {row_count}")


def _normalise(text: str) -> str:
  return " ".join(text.split())


def _normalise_marking(text: str) -> str:
  lines = (re.sub(r"^\s*(?://|#|\*)\s?", "", line) for line in text.splitlines())
  return _normalise(" ".join(lines))


def _a4_scan_texts(repo_root: Path) -> dict[str, str]:
  paths = [repo_root / relative for relative in A4_SCAN_FILES]
  for relative in A4_SCAN_ROOTS:
    root = repo_root / relative
    _require(root.is_dir(), f"A4 scan root is missing: {relative}")
    paths.extend(path for path in root.rglob("*") if path.is_file())

  texts: dict[str, str] = {}
  for path in sorted(set(paths)):
    relative = path.relative_to(repo_root)
    if relative.suffix == ".ipynb" or A4_SCAN_EXCLUDED_PARTS.intersection(relative.parts):
      continue
    try:
      texts[relative.as_posix()] = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
      continue
  return texts


def _exact_marker_hosts(texts: dict[str, str], marker: str) -> set[str]:
  return {relative for relative, text in texts.items() if marker in text}


def _residual_gpl_tokens(texts: dict[str, str]) -> Counter[tuple[str, str]]:
  tokens: Counter[tuple[str, str]] = Counter()
  for relative, text in texts.items():
    tokens.update((relative, match.group(0)) for match in RESIDUAL_GPL_PATTERN.finditer(text))
  return tokens


def _verify_a4_license_surfaces(repo_root: Path) -> None:
  _require((repo_root / "LICENSE").read_bytes() == MIT_LICENSE_BYTES, "root LICENSE is not canonical MIT text")
  texts = _a4_scan_texts(repo_root)

  for label, marker in (("full", OLD_FULL_HEADER_MARKER), ("short", OLD_SHORT_HEADER_MARKER)):
    control = _exact_marker_hosts({"<known-positive>": marker}, marker)
    _require(control == {"<known-positive>"}, f"old {label} header scan positive control failed")
    hosts = _exact_marker_hosts(texts, marker)
    _require(not hosts, f"old {label} project header remains: {sorted(hosts)}")

  project_spdx_hosts: set[str] = set()
  for relative, text in texts.items():
    header = "\n".join(text.splitlines()[:25])
    if "Ningqi Wang (0xf3cd)" not in header or "https://github.com/0xf3cd/celestial-calendar" not in header:
      continue
    _require(header.count(MIT_SPDX_MARKER) == 1, f"MIT SPDX project header differs: {relative}")
    project_spdx_hosts.add(relative)
  _require(
    len(project_spdx_hosts) == EXPECTED_PROJECT_SPDX_FILES,
    f"MIT SPDX project-header population differs: {len(project_spdx_hosts)}",
  )
  _require(
    texts["run-clang-tidy.py"].count("SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception") == 1,
    "run-clang-tidy.py upstream SPDX marking differs",
  )

  package = _load_json(texts["bindings/javascript/package.json"].encode(), "npm project metadata")
  package_lock = _load_json(texts["bindings/javascript/package-lock.json"].encode(), "npm lock metadata")
  _require(package.get("license") == "MIT", "npm project license metadata differs")
  _require(package_lock.get("packages", {}).get("", {}).get("license") == "MIT", "npm root lock license differs")
  _require(texts["bindings/python/pyproject.toml"].count('license = "MIT"') == 1, "Python license metadata differs")
  _require(texts["toolbox/build_npm.py"].count('"license": "MIT",') == 1, "staged npm license differs")
  _require(
    texts["bindings/python/test/wheel/verify.py"].count('metadata["License-Expression"] == "MIT"') == 1,
    "wheel License-Expression expectation differs",
  )

  for relative in ("bindings/javascript/README.md", "bindings/python/README.md"):
    section = _section(texts[relative], "## License")
    _require("licensed under MIT" in section, f"package README MIT scope differs: {relative}")
    _require("THIRD_PARTY_NOTICES.txt" in section, f"package README third-party exception differs: {relative}")
  readme_license = _section(texts["README.md"], "## 13. License")
  _require("Project-authored material is licensed under the MIT License" in readme_license, "README MIT scope differs")
  for pointer in ("THIRD_PARTY_NOTICES.txt", "inline or adjacent attribution records"):
    _require(pointer in readme_license, f"README third-party exception pointer differs: {pointer}")

  _require(
    texts["project.py"].count('BUILD_VERSION: Final[str] = "0.7.0"') == 1,
    "project version is not 0.7.0",
  )
  for relative in ("docs/CHANGELOG.md", "docs/RELEASE_NOTES.md"):
    _require(texts[relative].count("## [v0.7.0]") == 1, f"v0.7.0 release heading differs: {relative}")
    _require(
      "Project-authored material is now licensed under MIT" in texts[relative],
      f"MIT release note differs: {relative}",
    )
  _require("License: MIT." in texts["AGENTS.md"], "AGENTS.md repository license differs")
  _require(MIT_SPDX_MARKER in texts["AGENTS.md"], "AGENTS.md file-header convention differs")

  residual_control = _residual_gpl_tokens({"<known-positive>": GPL_V3})
  _require(
    residual_control == Counter({("<known-positive>", GPL_V3): 1}),
    "residual GPL scan positive control failed",
  )
  residual = _residual_gpl_tokens(texts)
  _require(
    residual == RESIDUAL_GPL_ALLOWLIST,
    f"residual GPL allowlist differs: missing={RESIDUAL_GPL_ALLOWLIST - residual}, "
    f"unexpected={residual - RESIDUAL_GPL_ALLOWLIST}",
  )


def _verify_source_identity(block: dict, host_text: str) -> None:
  identity = block["source_identity"].casefold()
  context = " ".join(
    (
      host_text.casefold(),
      block["locator"].casefold(),
      block.get("notice_title", "").casefold(),
    )
  )
  matched_families = _source_families(identity)
  _require(matched_families, f"{block['id']} source identity is not recognised")
  _require(
    any(any(_contains_source_alias(context, alias) for alias in family) for family in matched_families),
    f"{block['id']} source identity does not match its host",
  )


def _contains_source_alias(text: str, alias: str) -> bool:
  if re.fullmatch(r"[a-z0-9]+", alias):
    return re.search(rf"(?<![a-z0-9]){re.escape(alias)}(?![a-z0-9])", text) is not None
  return alias in text


def _source_families(text: str) -> list[tuple[str, ...]]:
  folded = text.casefold()
  return [family for family in SOURCE_FAMILIES if any(_contains_source_alias(folded, alias) for alias in family)]


def _section(text: str, heading: str) -> str:
  _require(text.count(heading) == 1, f"adjacent attribution section differs: {heading}")
  start = text.index(heading)
  end = text.find("\n## ", start + len(heading))
  return (text[start:] if end < 0 else text[start:end]).rstrip() + "\n"


def _verify_registry(
  repo_root: Path,
  registry: dict,
  rows: dict[str, dict],
  notice_sources: Sequence[NoticeSource],
  require_mit_spdx: bool | None,
) -> tuple[list[dict], int, int]:
  _require(
    set(registry) == {"schema", "scope", "non_project_spdx_hosts", "blocks"},
    "retained registry top-level fields differ",
  )
  _require(registry["schema"] == 1, "retained registry schema differs")
  _require(isinstance(registry["scope"], str) and registry["scope"], "retained registry scope is empty")
  _require(
    set(registry["non_project_spdx_hosts"]) == NON_PROJECT_SPDX_HOSTS,
    "non-project SPDX host inventory differs",
  )
  blocks = registry["blocks"]
  _require(isinstance(blocks, list), "retained registry blocks must be an array")
  ids = [block.get("id") for block in blocks if isinstance(block, dict)]
  _require(len(ids) == len(blocks), "retained registry block must be an object")
  _require(len(ids) == len(set(ids)), "retained registry block IDs are duplicated")
  id_set = set(ids)
  _require(
    id_set == REQUIRED_REGISTRY_IDS,
    f"retained registry inventory differs: missing={sorted(REQUIRED_REGISTRY_IDS - id_set)}, "
    f"unexpected={sorted(id_set - REQUIRED_REGISTRY_IDS)}",
  )

  locators: set[tuple[str, str]] = set()
  registered_paths: set[str] = set()
  owners: set[str] = set()
  adjacent_sections: dict[str, set[str]] = {}
  notice_applicability: dict[str, set[str]] = {}
  base_fields = {"id", "path", "locator", "source_identity", "material_scope", "owner", "marking_mode"}
  for block in blocks:
    mode = block.get("marking_mode")
    mode_fields = (
      {"marker"}
      if mode == "in_file"
      else {
        "data_sha256",
        "adjacent_path",
        "adjacent_sha256",
        "section",
        "section_sha256",
      }
    )
    allowed_fields = base_fields | mode_fields | {"notice_title"}
    _require(mode in {"in_file", "adjacent_record"}, f"{block['id']} marking mode differs")
    _require(set(block) == allowed_fields - ({"notice_title"} - set(block)), f"{block['id']} registry fields differ")
    for field in ("id", "path", "locator", "source_identity", "owner"):
      _require(isinstance(block[field], str) and block[field], f"{block['id']} {field} is empty")
    _require(block["material_scope"] == "retained_third_party_outside_mit", f"{block['id']} scope differs")
    path = _safe_path(repo_root, block["path"], f"{block['id']} retained host")
    registered_paths.add(block["path"])
    locator = (block["path"], block["locator"])
    _require(locator not in locators, f"duplicate retained locator: {block['path']}::{block['locator']}")
    locators.add(locator)
    owner_ids = set(ROW_ID_PATTERN.findall(block["owner"]))
    _require(owner_ids, f"{block['id']} owner names no row")
    owners.update(owner_ids)
    block_families = set(_source_families(block["source_identity"]))
    owner_families = {
      family
      for owner_id in owner_ids & set(rows)
      if rows[owner_id]["material_scope"] == "retained_third_party_outside_mit"
      for family in _source_families(rows[owner_id]["source_identity"])
    }
    if owner_families:
      _require(block_families <= owner_families, f"{block['id']} source identity differs from its owning row")

    if mode == "in_file":
      marker = _normalise_marking(block["marker"])
      host = _normalise_marking(path.read_text(encoding="utf-8"))
      _require(host.count(marker) == 1, f"{block['id']} retained marking differs")
      _require("outside the project MIT grant" in host, f"{block['id']} MIT boundary is missing")
      _verify_source_identity(block, host)
    else:
      _verify_adjacent_record(repo_root, block, adjacent_sections)
    if "notice_title" in block:
      title = block["notice_title"]
      _require(isinstance(title, str) and title, f"{block['id']} notice title is empty")
      notice_applicability.setdefault(title, set()).add(block["id"])

  retained_rows = {
    row_id for row_id, row in rows.items() if row["material_scope"] == "retained_third_party_outside_mit"
  }
  _require(retained_rows <= owners, f"retained rows lack registry owners: {sorted(retained_rows - owners)}")
  _require(
    IDENTITY_GATE_HOSTS <= registered_paths,
    f"identity-gate hosts lack registry entries: {sorted(IDENTITY_GATE_HOSTS - registered_paths)}",
  )
  _verify_orphan_markings(repo_root, blocks)
  _verify_orphan_adjacent_sections(repo_root, adjacent_sections)

  expected_notice_titles = {source.title for source in notice_sources}
  _require(
    set(EXPECTED_NOTICE_APPLICABILITY) == expected_notice_titles,
    "notice source inventory differs from the applicability contract",
  )
  actual_notice_applicability = {title: frozenset(ids) for title, ids in notice_applicability.items()}
  _require(
    actual_notice_applicability == EXPECTED_NOTICE_APPLICABILITY,
    "notice applicability mapping differs",
  )

  if require_mit_spdx is None:
    require_mit_spdx = (repo_root / "LICENSE").read_bytes().startswith(b"MIT License\n")
  if require_mit_spdx:
    project_hosts = {block["path"] for block in blocks if block["marking_mode"] == "in_file"} - NON_PROJECT_SPDX_HOSTS
    for relative in project_hosts:
      text = (repo_root / relative).read_text(encoding="utf-8")
      _require(text.count("SPDX-License-Identifier: MIT") == 1, f"MIT SPDX host marking differs: {relative}")

  return blocks, len(registered_paths), sum(len(sections) for sections in adjacent_sections.values())


def _verify_adjacent_record(repo_root: Path, block: dict, adjacent_sections: dict[str, set[str]]) -> None:
  data = _safe_path(repo_root, block["path"], f"{block['id']} retained data").read_bytes()
  _require(hashlib.sha256(data).hexdigest() == block["data_sha256"], f"{block['id']} retained data hash differs")
  adjacent = _safe_path(repo_root, block["adjacent_path"], f"{block['id']} adjacent record")
  adjacent_data = adjacent.read_bytes()
  _require(
    hashlib.sha256(adjacent_data).hexdigest() == block["adjacent_sha256"],
    f"{block['id']} adjacent record hash differs",
  )
  text = adjacent_data.decode("utf-8")
  section = _section(text, block["section"])
  _require(
    hashlib.sha256(section.encode("utf-8")).hexdigest() == block["section_sha256"],
    f"{block['id']} adjacent section hash differs",
  )
  normalised = _normalise(section)
  for expected in (block["path"], block["data_sha256"], block["source_identity"], "outside the project MIT grant"):
    _require(expected in normalised, f"{block['id']} adjacent reciprocal field differs: {expected}")
  adjacent_sections.setdefault(block["adjacent_path"], set()).add(block["section"])


def _marked_source_paths(repo_root: Path) -> list[Path]:
  paths: list[Path] = []
  for root_name in ("src", "automation", "bindings", "statistics", "toolbox"):
    for path in (repo_root / root_name).rglob("*"):
      if not path.is_file() or "src/test/provenance" in path.as_posix() or "automation/tests" in path.as_posix():
        continue
      if path.suffix in {".cpp", ".hpp", ".h", ".py"} or path.name == "CMakeLists.txt":
        paths.append(path)
  paths.extend((repo_root / "run-clang-tidy.py", repo_root / "THIRD_PARTY_NOTICES.txt"))
  return paths


def _verify_orphan_markings(repo_root: Path, blocks: list[dict]) -> None:
  registered = {
    block["path"]: [
      _normalise_marking(candidate["marker"]) for candidate in blocks if candidate["path"] == block["path"]
    ]
    for block in blocks
    if block["marking_mode"] == "in_file"
  }
  for path in _marked_source_paths(repo_root):
    relative = path.relative_to(repo_root).as_posix()
    for line in path.read_text(encoding="utf-8").splitlines():
      if relative != "THIRD_PARTY_NOTICES.txt" and not line.lstrip().startswith(("#", "//", "*")):
        continue
      if not any(phrase in line for phrase in MARKING_PHRASES):
        continue
      marking = _normalise_marking(line)
      candidates = registered.get(relative, [])
      _require(
        any(candidate.startswith(marking) or marking.startswith(candidate) for candidate in candidates),
        f"orphan retained marking: {relative}: {marking}",
      )


def _verify_orphan_adjacent_sections(repo_root: Path, expected: dict[str, set[str]]) -> None:
  for relative, sections in expected.items():
    text = (repo_root / relative).read_text(encoding="utf-8")
    actual = set(re.findall(r"(?m)^## V\d{2}: .+$", text))
    _require(
      actual == sections,
      f"orphan or missing adjacent section in {relative}: "
      f"missing={sorted(sections - actual)}, unexpected={sorted(actual - sections)}",
    )


def verify_batch_a_closeout(
  repo_root: Path = REPO_ROOT,
  record_sha256: str = RECORD_SHA256,
  registry_sha256: str = REGISTRY_SHA256,
  notice_sources: Sequence[NoticeSource] = NOTICE_SOURCES,
  require_mit_spdx: bool | None = None,
) -> CloseoutCounts:
  if require_mit_spdx is None:
    require_mit_spdx = (repo_root / "LICENSE").read_bytes().startswith(b"MIT License\n")
  if require_mit_spdx:
    _verify_a4_license_surfaces(repo_root)

  closeout_root = repo_root / CLOSEOUT_ROOT_RELATIVE
  record = _load_json(_read_pinned(closeout_root / RECORD_NAME, record_sha256, "closeout record"), "closeout record")
  registry = _load_json(
    _read_pinned(closeout_root / REGISTRY_NAME, registry_sha256, "retained registry"),
    "retained registry",
  )
  rows = _verify_row_record(repo_root, record)
  blocks, marked_hosts, adjacent_records = _verify_registry(
    repo_root,
    registry,
    rows,
    notice_sources,
    require_mit_spdx,
  )
  return CloseoutCounts(
    rows=len(rows),
    retained_blocks=len(blocks),
    marked_hosts=marked_hosts,
    adjacent_records=adjacent_records,
    notice_mappings=len({source.title for source in notice_sources}),
  )


if __name__ == "__main__":
  print(verify_batch_a_closeout())
