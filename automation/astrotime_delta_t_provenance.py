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

from pathlib import Path
from typing import Final

if __package__:
  from .source_digest import canonical_cpp
else:
  from source_digest import canonical_cpp


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
ASTROTIME_RECORD_COMMIT: Final[str] = "55115f4bf59cbdc47970b7f2d69a9715a467a3e9"
ASTROTIME_GENERATION_COMMIT: Final[str] = "298fa51777ec533951c4c1f83f8e5704b586754f"
ASTROTIME_ROOT: Final[Path] = REPO_ROOT / "src" / "test" / "provenance" / "astrotime-analysis" / ASTROTIME_RECORD_COMMIT
ASTROTIME_GRANT_SHA256: Final[str] = "075b3e670fbe4b32156d9f41386f444d65a0a7bbdc1e87dca2c0318ffd7c586f"
ASTROTIME_RECORD_SHA256: Final[str] = "ac3f00a8fe69af51c0e0fea8f945e5cf48cb81bfa0bac3a0471950e6558fd022"
ASTROTIME_GRANT_NORMALIZED_SHA256: Final[str] = "f00038723177475993d52bf4ec5bdd45f123cfad5201a7a9a478ba3fb3df4132"
ASTROTIME_RECORD_URL: Final[str] = (
  f"https://github.com/0xf3cd/AstroTime-Analysis/blob/{ASTROTIME_RECORD_COMMIT}/DeltaT/algo4/record.json"
)
ALGO4_FUNCTION_SHA256: Final[str] = "0b6065a66073f45c3cd510be87f31a22e65625665f569a91e7802c29fc17539e"
ASTROTIME_ALGO5_RECORD_COMMIT: Final[str] = "ed1cdc2fd6c5122b391a82289aa2cc060340552d"
ASTROTIME_ALGO5_GENERATION_COMMIT: Final[str] = "a1e95811b47c084f44b4b4bb7444560dd5b863bc"
ASTROTIME_ALGO5_DATA_COMMIT: Final[str] = "ddf3be1972e405ed02233837394cd3377226b65c"
ASTROTIME_ALGO5_ROOT_RELATIVE: Final[Path] = (
  Path("src") / "test" / "provenance" / "astrotime-analysis" / ASTROTIME_ALGO5_RECORD_COMMIT
)
ASTROTIME_ALGO5_ROOT: Final[Path] = REPO_ROOT / ASTROTIME_ALGO5_ROOT_RELATIVE
ASTROTIME_ALGO5_GRANT_SHA256: Final[str] = "f87240e7cbc4c0a21457ca0cf026dd848e619e1257737dab9307e3d1e73443f7"
ASTROTIME_ALGO5_RECORD_SHA256: Final[str] = "77fa6748fc19954073ee3c013731ac0200f4730efcb483f9f08fe8c921666455"
ASTROTIME_ALGO5_GRANT_NORMALIZED_SHA256: Final[str] = "63fbad6abcbf10a91f0561e675e1ae040401d54739d89c6d804598dd3b0cf447"
ASTROTIME_ALGO5_RECORD_URL: Final[str] = (
  f"https://github.com/0xf3cd/AstroTime-Analysis/blob/{ASTROTIME_ALGO5_RECORD_COMMIT}/DeltaT/algo5/record.json"
)
ALGO5_FUNCTION_SHA256: Final[str] = "44ef6c75688b7036892e8cd6ba528822ae97664ec67732db5f208fe467960d4b"
DEFAULT_FUNCTION_SHA256: Final[str] = "883c5e0652ec79f72d15bf125af88c7eceedb510d95f6936478e0c4fd37d1be8"


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _read_pinned(path: Path, expected_sha256: str, label: str) -> bytes:
  data = path.read_bytes()
  digest = hashlib.sha256(data).hexdigest()
  _require(digest == expected_sha256, f"{label} hash mismatch: {digest}")
  return data


def _cpp_block(text: str, declaration: str) -> str:
  start = text.index(declaration)
  opening = text.index("{", start)
  depth = 0
  for index in range(opening, len(text)):
    if text[index] == "{":
      depth += 1
    elif text[index] == "}":
      depth -= 1
      if depth == 0:
        return text[start : index + 1]
  raise RuntimeError(f"C++ block is incomplete: {declaration}")


def _cpp_term(coefficient: str, factor: str) -> str:
  operator = "-" if coefficient.startswith("-") else "+"
  return f"{operator} ({coefficient.removeprefix('-')} {factor})"


def _verify_algo5(
  repo_root: Path,
  algo5_root: Path,
  grant_sha256: str,
  record_sha256: str,
  function_sha256: str,
  default_function_sha256: str,
) -> None:
  _require(algo5_root.name == ASTROTIME_ALGO5_RECORD_COMMIT, "AstroTime algo5 record commit path differs")
  inventory = sorted(path.name for path in algo5_root.iterdir())
  _require(inventory == ["GRANT.md", "record.json"], "AstroTime algo5 retained record inventory differs")

  grant = _read_pinned(algo5_root / "GRANT.md", grant_sha256, "AstroTime algo5 grant").decode("utf-8")
  record = json.loads(_read_pinned(algo5_root / "record.json", record_sha256, "AstroTime algo5 record"))
  grant_text = " ".join(grant.split())

  _require(
    set(record)
    == {
      "schema_version",
      "record_type",
      "scope",
      "historical_generation",
      "third_party_inputs",
      "outputs",
      "validation_relations",
      "consumer_contract",
      "reproducibility",
    },
    "AstroTime algo5 record schema inventory differs",
  )
  _require(record["schema_version"] == 1, "AstroTime algo5 record schema differs")
  _require(record["record_type"] == "historical_input_output_record", "AstroTime algo5 record type differs")
  _require(
    record["scope"]
    == {
      "description": "Delta T algo5 outputs consumed by 0xf3cd/celestial-calendar",
      "grant": "DeltaT/algo5/GRANT.md",
      "consumer_repository": "https://github.com/0xf3cd/celestial-calendar",
      "consumer_symbol": "astro::delta_t::algo5::compute",
    },
    "AstroTime algo5 grant scope record differs",
  )
  for required in (
    "as project-authored material",
    "the eight observed-segment coefficients and the basis `u = year - 1990`",
    "`LAST_OBSERVATION_YEAR = 2026.4135844748857`",
    "`C = -150.64706473230285`",
    "GPL-3.0-or-later",
    "planned MIT licence",
    "no separate attribution, notice, or other condition",
    "does not apply to IERS Bulletin A",
    "the HMNAO long-term variation expression, its analytic integral",
    "does not license AstroTime-Analysis as a whole",
  ):
    _require(required in grant_text, f"AstroTime algo5 grant clause differs: {required}")
  for token in ("1.72", "3.5", "14", "31.4115", "284.8435805251424", "0.4487989505128276", "1825", "0.75"):
    _require(f"`{token}`" in grant, f"AstroTime algo5 grant exclusion differs: {token}")
  _require(
    hashlib.sha256(grant_text.encode()).hexdigest() == ASTROTIME_ALGO5_GRANT_NORMALIZED_SHA256,
    "AstroTime algo5 grant scope differs",
  )

  historical = record["historical_generation"]
  _require(
    set(historical)
    == {
      "repository",
      "commit",
      "parent_data_commit",
      "commit_date",
      "script",
      "analyzer",
      "inputs",
      "transformations",
      "observed_input",
      "environment",
    },
    "AstroTime algo5 historical generation inventory differs",
  )
  _require(
    historical["repository"] == "https://github.com/0xf3cd/AstroTime-Analysis"
    and historical["commit"] == ASTROTIME_ALGO5_GENERATION_COMMIT
    and historical["parent_data_commit"] == ASTROTIME_ALGO5_DATA_COMMIT
    and historical["commit_date"] == "2026-07-26T21:50:31-07:00",
    "AstroTime algo5 historical generation identity differs",
  )
  _require(
    historical["script"]
    == {
      "path": "DeltaT/algo5.py",
      "git_blob_sha1": "76b9a60cb5b12b3f315c58b1a00828c8ba32751a",
      "sha256": "4457e4eb88034aee34a680cac353e2f02a2eefddfc886fdaa73000638fe6fa1b",
    },
    "AstroTime algo5 script identity differs",
  )
  _require(
    historical["analyzer"]
    == {
      "path": "DeltaT/analyzer.py",
      "git_blob_sha1": "47388e7dabadd5aa13710c785b1ef65d5a6bc6d3",
      "sha256": "df7a637305da0ea544c4d5a86cfd0b216ce3315038395544eafe846bab0b31a7",
    },
    "AstroTime algo5 analyzer identity differs",
  )
  _require(
    historical["inputs"]
    == {
      "iers_bulletin_a": {
        "source": "https://www.iers.org/IERS/EN/Publications/Bulletins/bulletins.html",
        "raw_tree_sha1": "23574b254efe669d792dc2881dd133ea931cfaab",
        "raw_file_count": 1124,
        "header_csv": {
          "path": "DeltaT/cvs/bulletin_a_header.csv",
          "git_blob_sha1": "0a8a7301dd0ad7a1b4b09ab58d6a4d11fba27a66",
          "sha256": "5ab36a79166111ef47210f558e44eccd32bbe0d1160365a8ae1105e5415c44d5",
          "data_rows": 1124,
        },
        "final_values_csv": {
          "path": "DeltaT/cvs/bulletin_a_final_values.csv",
          "git_blob_sha1": "37f7c65d757bc1bdeb56e929400755f8af6c15ef",
          "sha256": "bee082ca1a5d302603af7d07146c25dc07d7b700e4b8f502493672db3b3798d6",
          "data_rows": 7848,
          "mjd_range": [53315, 61192],
          "date_range": ["2004-11-06", "2026-06-01"],
        },
      }
    },
    "AstroTime algo5 historical inputs differ",
  )
  _require(
    historical["transformations"]
    == [
      "Parse TAI-UTC effective dates from Bulletin A headers",
      "Discard (33.0, 1999-01-01) as the explicit historical project decision in algo5.py",
      "Parse final UT1-UTC rows and convert MJD from the 1858-11-17 epoch",
      "Compute decimal year with naive datetime.timestamp()",
      "Compute Delta T as TAI-UTC minus UT1-UTC plus 32.184 seconds",
      "Remove observations beyond two standard deviations of a degree-3 polynomial residual",
      "Fit the eight-term observed segment by constrained least squares at 2005.0 and the last observation",
      "Choose C so the long-term branch equals the observed fit at the last-observation boundary",
    ],
    "AstroTime algo5 historical transformations differ",
  )
  _require(
    historical["observed_input"]
    == {
      "deduplicated_rows": 7848,
      "decimal_year_range": ["2004.8469945355191", "2026.4135844748857"],
      "left_anchor": {
        "year": "2005.0",
        "window": "absolute decimal-year distance at most 30 / 365.25",
        "observations": 30,
        "median_delta_t_seconds": "64.697075",
      },
      "right_anchor": {
        "year": "2026.4135844748857",
        "window": "last 60 observations",
        "observations": 60,
        "median_delta_t_seconds": "69.15163",
      },
    },
    "AstroTime algo5 observed input differs",
  )
  _require(
    historical["environment"]
    == {
      "recorded": {},
      "unrecovered": [
        "Python version",
        "NumPy version",
        "BLAS implementation",
        "operating system and architecture",
        "timezone",
        "script invocation",
      ],
    },
    "AstroTime algo5 environment boundary differs",
  )

  expected_lod = {
    "authors": ["L. V. Morrison", "F. R. Stephenson", "C. Y. Hohenkerk", "M. Zawilski"],
    "addendum": "https://doi.org/10.1098/rspa.2020.0776",
    "combined_expression": ("https://web.archive.org/web/20230103030546id_/https://astro.ukho.gov.uk/nao/lvm/"),
    "lod": "1.72 * t - 3.5 * sin(2 * pi * (t + 0.75) / 14), where t = (year - 1825) / 100",
    "analytic_integral": ("31.4115 * t^2 + 284.8435805251424 * cos(0.4487989505128276 * (t + 0.75))"),
    "grant_status": "excluded from DeltaT/algo5/GRANT.md",
  }
  _require(
    record["third_party_inputs"] == {"corrected_lod_expression": expected_lod},
    "AstroTime algo5 third-party input differs",
  )

  expected_outputs = {
    "observed_segment": {
      "consumer_interval": "2005.0 <= year <= 2026.4135844748857",
      "fit_basis": "u = year - 1990",
      "consumer_basis": "u = year - 1990",
      "model": "a + b / u + c * u + d * u^2 + e * u^3 + f * u^4 + g * u^5 + h * u^6",
      "coefficients": [
        "-9963.526300002133",
        "33695.46874239917",
        "1251.8488008787037",
        "-84.95374332215921",
        "3.383624734734596",
        "-0.079054553776811",
        "0.0010034123781420099",
        "-5.341580725291782e-06",
      ],
    },
    "last_observation_year": "2026.4135844748857",
    "fitted_integration_constant": "-150.64706473230285",
  }
  _require(record["outputs"] == expected_outputs, "AstroTime algo5 generated outputs differ")

  expected_v25 = {
    "source_commit": ASTROTIME_ALGO5_DATA_COMMIT,
    "source_tree": "23574b254efe669d792dc2881dd133ea931cfaab",
    "repository_path": "src/test/astro/delta_t_test_helper.hpp",
    "procedure": [
      (
        "Build the Delta T series from the pinned header and final-values CSVs using the historical TAI-UTC "
        "and UT1-UTC conversion"
      ),
      (
        "For each January 1 boundary from 2015 through 2026, select the 31 observations whose dates are within "
        "15 days inclusive"
      ),
      "Take the median and round it to two decimal places for the retained validation table",
    ],
    "years": list(range(2015, 2027)),
    "window_observations": [31] * 12,
    "source_medians": [
      "67.64391",
      "68.10247",
      "68.5927",
      "68.96763",
      "69.22018",
      "69.36113",
      "69.35938",
      "69.29403",
      "69.20172",
      "69.17573",
      "69.1385",
      "69.10959",
    ],
    "repository_values": [
      "67.64",
      "68.10",
      "68.59",
      "68.97",
      "69.22",
      "69.36",
      "69.36",
      "69.29",
      "69.20",
      "69.18",
      "69.14",
      "69.11",
    ],
    "relation": "separate Bulletin A generation pin; not an algo5 fit output",
  }
  _require(
    record["validation_relations"] == {"v25_2015_2026": expected_v25},
    "AstroTime algo5 V25 relation differs",
  )
  _require(
    record["consumer_contract"]
    == {
      "pre_2005": "Delegate to astro::delta_t::algo2::compute",
      "observed_branch_boundary": "year <= LAST_OBSERVATION_YEAR",
      "future_branch": (
        "Use the corrected integrated LOD expression with the fitted integration constant; no upper bound"
      ),
      "non_finite": "Propagate through the noexcept arithmetic",
      "default_model": True,
    },
    "AstroTime algo5 consumer contract record differs",
  )
  _require(
    record["reproducibility"]
    == {
      "historical_input_output_identity": "pinned",
      "bit_for_bit_regeneration_environment": "unrecovered",
      "claim": (
        "This record identifies the historical inputs, procedure and stored outputs. It does not claim bit-for-bit "
        "regeneration of the unrecovered 2026 numerical environment."
      ),
    },
    "AstroTime algo5 reproducibility boundary differs",
  )

  delta_t = (repo_root / "src" / "astro" / "delta_t.hpp").read_text(encoding="utf-8")
  algo5_start = delta_t.index("namespace algo5 {")
  algo5_end = delta_t.index("} // namespace algo5", algo5_start)
  algo5_source = delta_t[algo5_start:algo5_end]
  addendum_url = expected_lod["addendum"]
  archive_url = expected_lod["combined_expression"]
  _require(algo5_source.count(ASTROTIME_ALGO5_RECORD_URL) == 2, "AstroTime algo5 source marking differs")
  _require(algo5_source.count(addendum_url) == 2, "AstroTime algo5 addendum marking differs")
  _require(algo5_source.count(archive_url) == 2, "AstroTime algo5 HMNAO marking differs")
  _require("AstroTime-Analysis/blob/main/" not in algo5_source, "AstroTime algo5 source marking floats")
  _require(
    algo5_source.count("https://astro.ukho.gov.uk/nao/lvm/") == 2,
    "AstroTime algo5 HMNAO source marking floats",
  )

  function_source = _cpp_block(
    algo5_source,
    "[[nodiscard]] constexpr auto compute(const double year) noexcept -> double",
  )
  canonical_function = canonical_cpp(function_source)
  _require(
    hashlib.sha256(canonical_function.encode()).hexdigest() == function_sha256,
    "AstroTime algo5 function differs",
  )
  for required_source in (
    "if (year < 2005)",
    "return algo2::compute(year);",
    "if (year <= LAST_OBSERVATION_YEAR)",
    "const double u = year - 1990;",
    "const double t = (year - 1825.0) / 100.0;",
  ):
    _require(required_source in canonical_function, f"AstroTime algo5 consumer contract differs: {required_source}")

  coefficients = expected_outputs["observed_segment"]["coefficients"]
  expected_observed_expression = canonical_cpp(
    f"""return {coefficients[0]}
      {_cpp_term(coefficients[1], "/ u")}
      {_cpp_term(coefficients[2], "* u")}
      {_cpp_term(coefficients[3], "* std::pow(u, 2)")}
      {_cpp_term(coefficients[4], "* std::pow(u, 3)")}
      {_cpp_term(coefficients[5], "* std::pow(u, 4)")}
      {_cpp_term(coefficients[6], "* std::pow(u, 5)")}
      {_cpp_term(coefficients[7], "* std::pow(u, 6)")};"""
  )
  expected_future_expression = canonical_cpp(
    f"""return {expected_outputs["fitted_integration_constant"]}
      + (31.4115 * std::pow(t, 2))
      + (284.8435805251424 * std::cos(0.4487989505128276 * (t + 0.75)));"""
  )
  _require(expected_observed_expression in canonical_function, "AstroTime algo5 observed expression differs")
  _require(expected_future_expression in canonical_function, "AstroTime algo5 future expression differs")
  for coefficient in coefficients:
    _require(
      function_source.count(coefficient.removeprefix("-")) == 1,
      f"AstroTime algo5 consumer coefficient differs: {coefficient}",
    )
  _require(
    function_source.count(expected_outputs["fitted_integration_constant"].removeprefix("-")) == 1,
    "AstroTime algo5 integration constant differs",
  )
  _require(
    f"LAST_OBSERVATION_YEAR = {expected_outputs['last_observation_year']};" in algo5_source,
    "AstroTime algo5 boundary differs",
  )

  default_source = _cpp_block(
    delta_t[algo5_end:],
    "[[nodiscard]] constexpr auto compute(const double year) noexcept -> double",
  )
  _require(
    hashlib.sha256(canonical_cpp(default_source).encode()).hexdigest() == default_function_sha256,
    "AstroTime default Delta T function differs",
  )
  _require(
    canonical_cpp(default_source)
    == canonical_cpp(
      """[[nodiscard]] constexpr auto compute(const double year) noexcept -> double {
        return algo5::compute(year);
      }"""
    ),
    "AstroTime algo5 default dispatch differs",
  )


def verify_astrotime_delta_t_provenance(
  repo_root: Path = REPO_ROOT,
  astrotime_root: Path = ASTROTIME_ROOT,
  grant_sha256: str = ASTROTIME_GRANT_SHA256,
  record_sha256: str = ASTROTIME_RECORD_SHA256,
  algo4_function_sha256: str = ALGO4_FUNCTION_SHA256,
  algo5_root: Path | None = None,
  algo5_grant_sha256: str = ASTROTIME_ALGO5_GRANT_SHA256,
  algo5_record_sha256: str = ASTROTIME_ALGO5_RECORD_SHA256,
  algo5_function_sha256: str = ALGO5_FUNCTION_SHA256,
  default_function_sha256: str = DEFAULT_FUNCTION_SHA256,
) -> None:
  _require(astrotime_root.name == ASTROTIME_RECORD_COMMIT, "AstroTime record commit path differs")
  inventory = sorted(path.name for path in astrotime_root.iterdir())
  _require(inventory == ["GRANT.md", "record.json"], "AstroTime retained record inventory differs")

  grant = _read_pinned(astrotime_root / "GRANT.md", grant_sha256, "AstroTime grant").decode("utf-8")
  record = json.loads(_read_pinned(astrotime_root / "record.json", record_sha256, "AstroTime record"))
  grant_text = " ".join(grant.split())

  _require(
    set(record)
    == {
      "schema_version",
      "record_type",
      "scope",
      "historical_generation",
      "outputs",
      "consumer_contract",
      "reproducibility",
    },
    "AstroTime record schema inventory differs",
  )
  _require(record["schema_version"] == 1, "AstroTime record schema differs")
  _require(record["record_type"] == "historical_input_output_record", "AstroTime record type differs")
  _require(
    record["scope"]
    == {
      "description": "Delta T algo4 coefficient vectors consumed by 0xf3cd/celestial-calendar",
      "grant": "DeltaT/algo4/GRANT.md",
      "consumer_repository": "https://github.com/0xf3cd/celestial-calendar",
      "consumer_symbol": "astro::delta_t::algo4::compute",
    },
    "AstroTime grant scope record differs",
  )
  for required in (
    "as project-authored material",
    "GPL-3.0-or-later",
    "planned MIT licence",
    "no separate attribution, notice, or other condition",
    "does not apply to IERS Bulletin A",
    "USNO predictions, VSOP87 data, or any other input or third-party material",
    "does not license AstroTime-Analysis as a whole",
  ):
    _require(required in grant_text, f"AstroTime grant clause differs: {required}")
  _require(
    hashlib.sha256(grant_text.encode()).hexdigest() == ASTROTIME_GRANT_NORMALIZED_SHA256,
    "AstroTime grant scope differs",
  )

  historical = record["historical_generation"]
  _require(
    set(historical)
    == {
      "repository",
      "commit",
      "commit_date",
      "notebook",
      "analyzer",
      "inputs",
      "transformations",
      "observed_input_range",
      "environment",
    },
    "AstroTime historical generation inventory differs",
  )
  _require(
    historical["repository"] == "https://github.com/0xf3cd/AstroTime-Analysis"
    and historical["commit"] == ASTROTIME_GENERATION_COMMIT
    and historical["commit_date"] == "2024-07-12T13:46:18-07:00",
    "AstroTime historical generation identity differs",
  )
  _require(
    historical["notebook"]
    == {
      "path": "DeltaT/models.ipynb",
      "git_blob_sha1": "ed201d0ca1ff8f5fa885fef4ed61a65f44f3c3ac",
      "sha256": "3fd893bbbb90bcd85e693afa675a818eaaad931b0e75f32a1853e6672e93c567",
    },
    "AstroTime notebook identity differs",
  )
  _require(
    historical["analyzer"]
    == {
      "path": "DeltaT/analyzer.py",
      "git_blob_sha1": "47388e7dabadd5aa13710c785b1ef65d5a6bc6d3",
      "sha256": "df7a637305da0ea544c4d5a86cfd0b216ce3315038395544eafe846bab0b31a7",
    },
    "AstroTime analyzer identity differs",
  )
  _require(
    historical["inputs"]
    == {
      "iers_bulletin_a": {
        "source": "https://www.iers.org/IERS/EN/Publications/Bulletins/bulletins.html",
        "raw_tree_sha1": "03776616ea6c4821084ee57b717a1948620f9388",
        "raw_file_count": 1018,
        "latest_bulletin_date": "2024-07-11",
        "header_csv": {
          "path": "DeltaT/cvs/bulletin_a_header.csv",
          "git_blob_sha1": "1af825afd25887a7225110309fd00ce17afd243e",
          "sha256": "5aeda02ed40696fda4e281f17f2b5b82a1a78bedc4a5b903ae1bc3a938db3560",
          "data_rows": 1018,
        },
        "final_values_csv": {
          "path": "DeltaT/cvs/bulletin_a_final_values.csv",
          "git_blob_sha1": "fd907a4952efb70cbb987f5874c3d3e367eaeb81",
          "sha256": "678ea86e0d1da792d5450b2e383276e16c4933c0d66bdb897452bcba7fbc69d1",
          "data_rows": 7118,
          "mjd_range": [53315, 60462],
        },
      },
      "usno_predictions": {
        "source": "https://maia.usno.navy.mil/ser7/deltat.preds",
        "storage": "USNO_PREDICTION string in DeltaT/models.ipynb",
        "normalization": "Remove the initial newline from the triple-quoted string; retain all other bytes",
        "sha256": "5d864fddd30b2c64d2a86d3debbb25604eb5de44370c96bccf2abd5463f3db08",
        "data_rows": 46,
        "year_range": ["2022.50", "2033.75"],
        "later_exact_copy": {
          "path": "USNO_FINALS_DAILY/deltat.preds",
          "first_commit": "ddf3be1972e405ed02233837394cd3377226b65c",
          "git_blob_sha1": "92d32b72e26e9571c94f8fdf9b4458bdbf3a1959",
        },
      },
    },
    "AstroTime historical inputs differ",
  )
  _require(
    historical["transformations"]
    == [
      "Parse TAI-UTC effective dates from Bulletin A headers",
      "Discard (33.0, 1999-01-01) as an explicit project decision recorded by the notebook",
      "Parse final UT1-UTC rows and convert MJD from the 1858-11-17 epoch",
      "Compute decimal year with naive datetime.timestamp()",
      "Compute Delta T as TAI-UTC minus UT1-UTC plus 32.184 seconds",
      "Use degree-3 linear regression residuals and a two-standard-deviation threshold; the notebook reports zero rows "
      "removed",
      "Fit the observed and prediction model definitions with scipy.optimize.curve_fit",
    ],
    "AstroTime historical transformations differ",
  )
  _require(
    historical["observed_input_range"]
    == {
      "decimal_year_min": "2004.8469945355191",
      "decimal_year_max": "2024.4151867030967",
      "outliers_removed": 0,
    },
    "AstroTime observed input range differs",
  )
  _require(
    historical["environment"]
    == {
      "recorded": {"python": "3.12.3"},
      "unrecovered": [
        "Matplotlib version",
        "NumPy version",
        "SciPy version",
        "pandas version",
        "scikit-learn version",
        "BLAS implementation",
        "operating system and architecture",
        "timezone",
        "notebook execution command",
      ],
    },
    "AstroTime environment boundary differs",
  )

  expected_outputs = {
    "observed_segment": {
      "notebook_execution_count": 12,
      "consumer_interval": "2005.0 <= year < 2024.0",
      "fit_basis": "u = x - 1990",
      "consumer_basis": "u = year - 1990",
      "model": "a + b / u + c * u + d * u^2 + e * u^3 + f * u^4 + g * u^5 + h * u^6",
      "coefficients": [
        "-1539.5103964825782",
        "7305.087465383047",
        "116.17205714035308",
        "-1.1279910329686536",
        "-0.2754809577876994",
        "0.01542796862306066",
        "-0.0003332548091334704",
        "2.6541070013360904e-06",
      ],
      "metrics": {
        "r2": "0.9996631800017091",
        "mse": "0.000883173700993998",
        "mae": "0.023569581927838656",
        "mape": "0.000348269442798718",
        "max_error": "0.09903018031658917",
      },
    },
    "prediction_segment": {
      "notebook_execution_count": 16,
      "consumer_interval": "2024.0 <= year < 2035.0",
      "fit_basis": "u = x - 2020",
      "consumer_basis": "u = year - 2020",
      "model": "a + b / u + c * u + d * u^2 + e * u^3",
      "coefficients": [
        "73.38076003516039",
        "-4.199766017124573",
        "-1.3053623848472002",
        "0.14136771053009262",
        "-0.004086715638812636",
      ],
      "metrics": {
        "r2": "0.998487355243141",
        "max_error": "0.05866409260934802",
        "mse": "0.0007612049469331545",
        "mae": "0.02337720117426826",
        "mape": "0.000334747642232296",
      },
    },
  }
  _require(record["outputs"] == expected_outputs, "AstroTime generated outputs differ")
  _require(
    record["consumer_contract"]
    == {
      "pre_2005": "Delegate to astro::delta_t::algo2::compute",
      "non_finite_or_year_at_least_2035": "Throw std::out_of_range",
      "default_model": False,
    },
    "AstroTime consumer contract record differs",
  )
  _require(
    record["reproducibility"]
    == {
      "historical_input_output_identity": "pinned",
      "bit_for_bit_regeneration_environment": "unrecovered",
      "claim": (
        "This record identifies the historical inputs, procedure and stored outputs. It does not claim bit-for-bit "
        "regeneration of the unrecovered 2024 numerical environment."
      ),
    },
    "AstroTime reproducibility boundary differs",
  )

  delta_t = (repo_root / "src" / "astro" / "delta_t.hpp").read_text(encoding="utf-8")
  algo4_start = delta_t.index("namespace algo4 {")
  algo4_end = delta_t.index("} // namespace algo4", algo4_start)
  algo4_source = delta_t[algo4_start:algo4_end]
  _require(algo4_source.count(ASTROTIME_RECORD_URL) == 2, "AstroTime immutable source marking differs")
  _require("AstroTime-Analysis/blob/main/" not in algo4_source, "AstroTime source marking floats")

  function_source = _cpp_block(
    algo4_source,
    "[[nodiscard]] constexpr auto compute(const double year) -> double",
  )
  canonical_function = canonical_cpp(function_source)
  _require(
    hashlib.sha256(canonical_function.encode("utf-8")).hexdigest() == algo4_function_sha256,
    "AstroTime algo4 function differs",
  )
  for required_source in (
    "if (not std::isfinite(year) or year >= 2035)",
    "throw std::out_of_range",
    "if (year < 2005)",
    "return algo2::compute(year);",
    "if (year >= 2005 and year < 2024)",
  ):
    _require(required_source in canonical_function, f"AstroTime consumer contract differs: {required_source}")

  observed_coefficients = expected_outputs["observed_segment"]["coefficients"]
  prediction_coefficients = expected_outputs["prediction_segment"]["coefficients"]
  expected_observed_expression = canonical_cpp(
    f"""return {observed_coefficients[0]}
      {_cpp_term(observed_coefficients[1], "/ u")}
      {_cpp_term(observed_coefficients[2], "* u")}
      {_cpp_term(observed_coefficients[3], "* std::pow(u, 2)")}
      {_cpp_term(observed_coefficients[4], "* std::pow(u, 3)")}
      {_cpp_term(observed_coefficients[5], "* std::pow(u, 4)")}
      {_cpp_term(observed_coefficients[6], "* std::pow(u, 5)")}
      {_cpp_term(observed_coefficients[7], "* std::pow(u, 6)")};"""
  )
  expected_prediction_expression = canonical_cpp(
    f"""return {prediction_coefficients[0]}
      {_cpp_term(prediction_coefficients[1], "/ u")}
      {_cpp_term(prediction_coefficients[2], "* u")}
      {_cpp_term(prediction_coefficients[3], "* std::pow(u, 2)")}
      {_cpp_term(prediction_coefficients[4], "* std::pow(u, 3)")};"""
  )
  _require(expected_observed_expression in canonical_function, "AstroTime observed consumer expression differs")
  _require(expected_prediction_expression in canonical_function, "AstroTime prediction consumer expression differs")

  for output in expected_outputs.values():
    for coefficient in output["coefficients"]:
      _require(
        function_source.count(coefficient.removeprefix("-")) == 1,
        f"AstroTime consumer coefficient differs: {coefficient}",
      )
    _require(
      canonical_function.count(f"const double {output['consumer_basis']};") == 1,
      "AstroTime consumer basis differs",
    )

  if algo5_root is None:
    algo5_root = repo_root / ASTROTIME_ALGO5_ROOT_RELATIVE
  _verify_algo5(
    repo_root,
    algo5_root,
    algo5_grant_sha256,
    algo5_record_sha256,
    algo5_function_sha256,
    default_function_sha256,
  )


if __name__ == "__main__":
  verify_astrotime_delta_t_provenance()
