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

from pathlib import Path
from shutil import copy2

import pytest

from automation.erfa_identity import (
  ERFA_COMMIT,
  ERFA_INPUTS,
  ERFA_ROOT,
  ERFA_TAG_OBJECT,
  ERFA_VERSION,
  NIST_EVIDENCE,
  NIST_SP330_PDF_SHA256,
  NIST_SP330_URL,
  REPO_ROOT,
  IdentityCounts,
  verify_erfa_identities,
)


TARGET_FILES = (
  Path("src/astro/moon.hpp"),
  Path("src/astro/earth.hpp"),
  Path("src/astro/earth/precession.hpp"),
  Path("src/astro/julian_day.hpp"),
  Path("src/astro/toolbox.hpp"),
)


def materialize_inputs(destination: Path) -> Path:
  erfa_root = destination / ERFA_ROOT.relative_to(REPO_ROOT)
  for source in ERFA_INPUTS:
    target = erfa_root / source.path
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(ERFA_ROOT / source.path, target)
  for relative_path in (*TARGET_FILES, NIST_EVIDENCE.path):
    target = destination / relative_path
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / relative_path, target)
  return erfa_root


def replace_once(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  assert text.count(old) == 1
  path.write_text(text.replace(old, new), encoding="utf-8")


def test_erfa_inputs_nist_evidence_and_repository_values_are_pinned():
  assert ERFA_VERSION == "2.0.1"
  assert ERFA_TAG_OBJECT == "944bc0956f1d236e5982ee63930e060e60ec85f9"
  assert ERFA_COMMIT == "9915ba38c9365f8b0738269b8c2ac1fdd5f8dee3"
  assert len(ERFA_INPUTS) == 8
  input_hashes = {source.path: source.sha256 for source in ERFA_INPUTS}
  assert input_hashes[Path("src/cal2jd.c")] == "95d13243cfa6ed019cd4e1737388d97cc6be3a5e9bd99a35ea9938934d439a0f"
  assert input_hashes[Path("src/jd2cal.c")] == "f1f15f0295348efc51c3f0a43608b46cbe514d65846935b3f609406f731b1961"
  assert NIST_SP330_URL == "https://doi.org/10.6028/NIST.SP.330-2019"
  assert NIST_SP330_PDF_SHA256 == "57fadf9d4d086bd167634afc02ed9cb71278d16a676f36bc89f7148cbe64bf78"
  assert verify_erfa_identities() == IdentityCounts(9, 4, 14, 1, 5, 1)


def test_erfa_runtime_source_markings_are_pinned_and_distinct_from_generated_outputs():
  required_markings = {
    Path("src/astro/moon.hpp"): (
      "ERFA v2.0.1 `moon98.c`, Meeus additive longitude terms",
      "ERFA v2.0.1 `moon98.c`, Meeus additive latitude terms",
      "ERFA v2.0.1 moon98.c r0, converted from meters to kilometers",
    ),
    Path("src/astro/earth.hpp"): (
      "ERFA v2.0.1 `obl80.c`",
      "fixed ten-decimal rounding of",
      "`ERFA_DAU / ERFA_CMPS / ERFA_DAYSEC`",
      "not a digit copy of `ERFA_AULT`",
    ),
    Path("src/astro/earth/precession.hpp"): ("ERFA v2.0.1 `prec76.c`",),
    Path("src/astro/julian_day.hpp"): ("ERFA v2.0.1 `erfam.h`", "`ERFA_DJ00`", "`ERFA_DJC`", "`ERFA_DJM`"),
    Path("src/astro/toolbox.hpp"): ("NIST SP 330 (2019), Table 8", "ERFA v2.0.1 `erfam.h`, `ERFA_DAU`"),
  }
  for path, markings in required_markings.items():
    source = " ".join(line.strip().removeprefix("//").strip() for line in (REPO_ROOT / path).read_text().splitlines())
    for marking in markings:
      assert marking in source

  generated_output_provenance = (REPO_ROOT / "src/test/astro/precession_test.cpp").read_text(encoding="utf-8")
  assert "erfa 2.0.1.5.dev2+gd4d4fd5" in generated_output_provenance
  assert ERFA_COMMIT not in generated_output_provenance


@pytest.mark.parametrize(
  ("mutation", "message"),
  [
    ("moon", "moon perturbation term 1 differs"),
    ("obliquity", "obliquity coefficient 1 differs"),
    ("precession", "precession coefficient occurrence 1 differs"),
    ("distance", "lunar distance baseline differs"),
    ("au", "AU_KM_SCALE differs"),
    ("j2000", "J2000 differs"),
    ("century", "DAYS_PER_JULIAN_CENTURY differs"),
    ("millennium", "jde_to_jm divisor differs"),
    ("light-time", "LIGHT_TIME_DAYS_PER_AU rounding differs"),
  ],
)
def test_erfa_repository_value_mutations_fail(tmp_path, mutation, message):
  erfa_root = materialize_inputs(tmp_path)
  replacements = {
    "moon": (Path("src/astro/moon.hpp"), "3958.0 * std::sin", "3959.0 * std::sin"),
    "obliquity": (Path("src/astro/earth.hpp"), "84381.448 +", "84381.449 +"),
    "precession": (Path("src/astro/earth/precession.hpp"), "2306.2181 +", "2306.2182 +"),
    "distance": (Path("src/astro/moon.hpp"), "385000.56 +", "385000.57 +"),
    "au": (Path("src/astro/toolbox.hpp"), "149597870.700;", "149597870.701;"),
    "j2000": (Path("src/astro/julian_day.hpp"), "2451545.0;", "2451545.1;"),
    "century": (Path("src/astro/julian_day.hpp"), "36525.0;", "36525.1;"),
    "millennium": (Path("src/astro/julian_day.hpp"), "/ 365250.0;", "/ 365250.1;"),
    "light-time": (Path("src/astro/earth.hpp"), "0.0057755183;", "0.0057755184;"),
  }
  path, old, new = replacements[mutation]
  replace_once(tmp_path / path, old, new)

  with pytest.raises(RuntimeError, match=message):
    verify_erfa_identities(repo_root=tmp_path, erfa_root=erfa_root)


def test_erfa_source_mutation_fails_the_hash_pin(tmp_path):
  erfa_root = materialize_inputs(tmp_path)
  moon98 = erfa_root / "src/moon98.c"
  moon98.write_bytes(moon98.read_bytes() + b"changed")

  with pytest.raises(RuntimeError, match="ERFA input hash mismatch for src/moon98.c"):
    verify_erfa_identities(repo_root=tmp_path, erfa_root=erfa_root)


def test_nist_evidence_mutation_fails_the_hash_pin(tmp_path):
  erfa_root = materialize_inputs(tmp_path)
  evidence = tmp_path / NIST_EVIDENCE.path
  evidence.write_bytes(evidence.read_bytes() + b"changed")

  with pytest.raises(RuntimeError, match="NIST evidence hash mismatch"):
    verify_erfa_identities(repo_root=tmp_path, erfa_root=erfa_root)
