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


def verify_astrotime_delta_t_provenance(
  repo_root: Path = REPO_ROOT,
  astrotime_root: Path = ASTROTIME_ROOT,
  grant_sha256: str = ASTROTIME_GRANT_SHA256,
  record_sha256: str = ASTROTIME_RECORD_SHA256,
  algo4_function_sha256: str = ALGO4_FUNCTION_SHA256,
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


if __name__ == "__main__":
  verify_astrotime_delta_t_provenance()
