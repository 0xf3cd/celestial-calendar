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
import re

from dataclasses import dataclass
from decimal import Decimal
from pathlib import Path
from typing import Final


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
ERFA_VERSION: Final[str] = "2.0.1"
# The annotated liberfa/erfa v2.0.1 tag object peels (`v2.0.1^{commit}`) to ERFA_COMMIT.
ERFA_TAG_OBJECT: Final[str] = "944bc0956f1d236e5982ee63930e060e60ec85f9"
ERFA_COMMIT: Final[str] = "9915ba38c9365f8b0738269b8c2ac1fdd5f8dee3"
ERFA_RAW_BASE: Final[str] = f"https://raw.githubusercontent.com/liberfa/erfa/{ERFA_COMMIT}"
ERFA_ROOT: Final[Path] = REPO_ROOT / "src" / "test" / "provenance" / "erfa" / f"v{ERFA_VERSION}"
NIST_SP330_URL: Final[str] = "https://doi.org/10.6028/NIST.SP.330-2019"
NIST_SP330_PDF_SHA256: Final[str] = "57fadf9d4d086bd167634afc02ed9cb71278d16a676f36bc89f7148cbe64bf78"
DECIMAL: Final[str] = r"[+-]?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?"
MILLION: Final[Decimal] = Decimal(1_000_000)


@dataclass(frozen=True)
class PinnedInput:
  path: Path
  sha256: str


@dataclass(frozen=True)
class IdentityCounts:
  perturbation_terms: int
  obliquity_coefficients: int
  precession_coefficients: int
  lunar_distance_baselines: int
  definition_sites: int
  rounded_relations: int


ERFA_INPUTS: Final[tuple[PinnedInput, ...]] = (
  PinnedInput(Path("LICENSE"), "b1858f9a263f22c438a455a32945da51a31a0ae25a21055da13bb7ed57cc3b51"),
  PinnedInput(Path("src/cal2jd.c"), "95d13243cfa6ed019cd4e1737388d97cc6be3a5e9bd99a35ea9938934d439a0f"),
  PinnedInput(Path("src/jd2cal.c"), "f1f15f0295348efc51c3f0a43608b46cbe514d65846935b3f609406f731b1961"),
  PinnedInput(Path("src/moon98.c"), "8633f78b8ec33132743596700fd3ecfbb2ebcb4e886894e377556de9655d1237"),
  PinnedInput(Path("src/obl80.c"), "74e53b23c209f9101615015db7a4e7097c8d0ce0d0c8fa005476bf9e32a882fe"),
  PinnedInput(Path("src/prec76.c"), "809c200e6cd70c3eaabfed9c5d8a9dec573b67bc8e7d623cc5818550993bc684"),
  PinnedInput(Path("src/erfa.h"), "9549553b95ca2fbbcdcabcc69d82fb3cdd376c28b03c2793200138ff55727088"),
  PinnedInput(Path("src/erfam.h"), "0da6313033aacf9c64533a7e74be49ecda97b4aaa87e134f98287e4ff279886f"),
)
NIST_EVIDENCE: Final[PinnedInput] = PinnedInput(
  Path("src/test/provenance/nist/sp-330-2019/definitions.txt"),
  "553a419ab0691c0f14abf4656adc35cc4a9762a1ed938375418c2d41e61ef7e2",
)


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _block(text: str, start: str, end: str, label: str) -> str:
  start_at = text.find(start)
  _require(start_at >= 0, f"{label}: start marker not found")
  end_at = text.find(end, start_at)
  _require(end_at >= 0, f"{label}: end marker not found")
  return text[start_at : end_at + len(end)]


def _single_decimal(pattern: str, text: str, label: str) -> Decimal:
  matches = re.findall(pattern, text)
  _require(len(matches) == 1, f"{label}: parsed {len(matches)} values, expected 1")
  return Decimal(matches[0])


def _decimal_tokens(text: str) -> list[Decimal]:
  return [Decimal(value) for value in re.findall(rf"(?<![\w.])({DECIMAL})(?![\w.])", text)]


def _assigned_decimal(text: str, name: str, label: str) -> Decimal:
  return _single_decimal(rf"\b{name}\s*=\s*({DECIMAL})", text, label)


def _macro_decimal(text: str, name: str) -> Decimal:
  return _single_decimal(rf"#define\s+{name}\s+\(?({DECIMAL})\)?", text, name)


def _read_pinned_inputs(erfa_root: Path) -> dict[Path, str]:
  inputs: dict[Path, str] = {}
  for source in ERFA_INPUTS:
    data = (erfa_root / source.path).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    _require(digest == source.sha256, f"ERFA input hash mismatch for {source.path}: {digest}")
    inputs[source.path] = data.decode("ascii")
  return inputs


def _read_nist_evidence(repo_root: Path) -> str:
  data = (repo_root / NIST_EVIDENCE.path).read_bytes()
  digest = hashlib.sha256(data).hexdigest()
  _require(digest == NIST_EVIDENCE.sha256, f"NIST evidence hash mismatch: {digest}")
  text = data.decode("ascii")
  _require(NIST_SP330_URL in text, "NIST SP 330 DOI differs")
  _require(f"PDF SHA-256: {NIST_SP330_PDF_SHA256}" in text, "NIST SP 330 PDF hash differs")
  return text


def _require_values_equal(label: str, source: list[Decimal], target: list[Decimal]) -> None:
  _require(len(source) == len(target), f"{label}: source values={len(source)}, target values={len(target)}")
  for index, (source_value, target_value) in enumerate(zip(source, target, strict=True), 1):
    _require(
      source_value == target_value,
      f"{label} {index} differs: source={source_value}, target={target_value}",
    )


def _moon_perturbations(source: str, target: str) -> int:
  source_names = ("al1", "al2", "al3", "ab1", "ab2", "ab3", "ab4", "ab5", "ab6")
  source_patterns = (
    r"al1\s*\*\s*sin\(a1\)",
    r"al2\s*\*\s*sin\(elpmf\)",
    r"al3\s*\*\s*sin\(a2\)",
    r"ab1\s*\*\s*sin\(elp\)",
    r"ab2\s*\*\s*sin\(a3\)",
    r"ab3\s*\*\s*sin\(a1mf\)",
    r"ab4\s*\*\s*sin\(a1pf\)",
    r"ab5\s*\*\s*sin\(dlpmp\)",
    r"ab6\s*\*\s*sin\(slpmp\)",
  )
  for pattern in source_patterns:
    _require(re.search(pattern, source) is not None, f"moon98.c perturbation expression differs: {pattern}")
  source_values = [_assigned_decimal(source, name, f"moon98.c {name}") * MILLION for name in source_names]

  target_patterns = (
    (rf"\(\s*({DECIMAL})\s*\*\s*std::sin\(ctx\.A1\.rad\(\)\)\s*\)", Decimal(1)),
    (rf"\(\s*({DECIMAL})\s*\*\s*std::sin\(ctx\.Lp\.rad\(\)\s*-\s*ctx\.F\.rad\(\)\)\s*\)", Decimal(1)),
    (rf"\(\s*({DECIMAL})\s*\*\s*std::sin\(ctx\.A2\.rad\(\)\)\s*\)", Decimal(1)),
    (rf"\(\s*({DECIMAL})\s*\*\s*std::sin\(ctx\.Lp\.rad\(\)\)\s*\)", Decimal(1)),
    (rf"\(\s*({DECIMAL})\s*\*\s*std::sin\(ctx\.A3\.rad\(\)\)\s*\)", Decimal(1)),
    (rf"\(\s*({DECIMAL})\s*\*\s*std::sin\(ctx\.A1\.rad\(\)\s*-\s*ctx\.F\.rad\(\)\)\s*\)", Decimal(1)),
    (rf"\(\s*({DECIMAL})\s*\*\s*std::sin\(ctx\.A1\.rad\(\)\s*\+\s*ctx\.F\.rad\(\)\)\s*\)", Decimal(1)),
    (rf"\(\s*({DECIMAL})\s*\*\s*std::sin\(ctx\.Lp\.rad\(\)\s*-\s*ctx\.Mp\.rad\(\)\)\s*\)", Decimal(1)),
    (rf"-\s*\(\s*({DECIMAL})\s*\*\s*std::sin\(ctx\.Lp\.rad\(\)\s*\+\s*ctx\.Mp\.rad\(\)\)\s*\)", Decimal(-1)),
  )
  target_values = [
    _single_decimal(pattern, target, f"repository moon perturbation term {index}") * sign
    for index, (pattern, sign) in enumerate(target_patterns, 1)
  ]
  _require_values_equal("moon perturbation term", source_values, target_values)
  return len(target_values)


def _obliquity_coefficients(source: str, target: str) -> int:
  source_values = _decimal_tokens(_block(source, "eps0 = ERFA_DAS2R *", "return eps0;", "obl80.c eps0"))
  target_values = _decimal_tokens(
    _block(target, "const double ε0_arcsec =", "return toolbox::AngleDeg::from_arcsec", "repository obliquity")
  )
  _require(len(source_values) == 4, f"obl80.c coefficients={len(source_values)}, expected 4")
  _require_values_equal("obliquity coefficient", source_values, target_values)
  return len(target_values)


def _precession_coefficients(source: str, target: str) -> int:
  source_block = _block(source, "w = 2306.2181", "/* Finished. */", "prec76.c angles")
  source_values = [abs(value) for value in _decimal_tokens(source_block)]
  target_values = [
    abs(value)
    for value in _decimal_tokens(
      _block(target, "const double lead_ζz =", "return {", "repository equatorial precession")
    )
  ]
  _require(len(source_values) == 15, f"prec76.c coefficient occurrences={len(source_values)}, expected 15")
  _require(len(set(source_values)) == 14, f"prec76.c distinct coefficients={len(set(source_values))}, expected 14")
  _require_values_equal("precession coefficient occurrence", source_values, target_values)
  return len(set(target_values))


def _spaced_integer(pattern: str, text: str, label: str) -> Decimal:
  matches = re.findall(pattern, text)
  _require(len(matches) == 1, f"{label}: parsed {len(matches)} values, expected 1")
  return Decimal(matches[0].replace(" ", ""))


def verify_erfa_identities(
  repo_root: Path = REPO_ROOT,
  erfa_root: Path = ERFA_ROOT,
) -> IdentityCounts:
  # This pin is runtime-source lineage only. Existing pyerfa-generated outputs retain their test-local revisions.
  inputs = _read_pinned_inputs(erfa_root)
  moon98 = inputs[Path("src/moon98.c")]
  obl80 = inputs[Path("src/obl80.c")]
  prec76 = inputs[Path("src/prec76.c")]
  erfam = inputs[Path("src/erfam.h")]
  nist = _read_nist_evidence(repo_root)

  moon = (repo_root / "src/astro/moon.hpp").read_text(encoding="utf-8")
  earth = (repo_root / "src/astro/earth.hpp").read_text(encoding="utf-8")
  precession = (repo_root / "src/astro/earth/precession.hpp").read_text(encoding="utf-8")
  julian_day = (repo_root / "src/astro/julian_day.hpp").read_text(encoding="utf-8")
  toolbox = (repo_root / "src/astro/toolbox.hpp").read_text(encoding="utf-8")

  perturbation_terms = _moon_perturbations(moon98, moon)
  obliquity_coefficients = _obliquity_coefficients(obl80, earth)
  precession_coefficients = _precession_coefficients(prec76, precession)

  source_r0_km = _assigned_decimal(moon98, "r0", "moon98.c r0") / Decimal(1000)
  target_r0_km = _single_decimal(
    rf"DistanceKm\s+r\s*\{{\s*({DECIMAL})\s*\+",
    moon,
    "repository lunar distance baseline",
  )
  _require(
    source_r0_km == target_r0_km,
    f"lunar distance baseline differs: source={source_r0_km}, target={target_r0_km}",
  )

  nist_c = _spaced_integer(r"exact relation c = ([0-9 ]+) m s-1", nist, "NIST speed of light")
  nist_day_seconds = _spaced_integer(r"1 d = 24 h = ([0-9 ]+) s", nist, "NIST day")
  nist_au_m = _spaced_integer(r"1 au = ([0-9 ]+) m", nist, "NIST astronomical unit")
  erfa_c = _macro_decimal(erfam, "ERFA_CMPS")
  erfa_day_seconds = _macro_decimal(erfam, "ERFA_DAYSEC")
  erfa_au_m = _macro_decimal(erfam, "ERFA_DAU")
  _require(erfa_c == nist_c, f"ERFA_CMPS differs from NIST: ERFA={erfa_c}, NIST={nist_c}")
  _require(
    erfa_day_seconds == nist_day_seconds,
    f"ERFA_DAYSEC differs from NIST: ERFA={erfa_day_seconds}, NIST={nist_day_seconds}",
  )
  _require(erfa_au_m == nist_au_m, f"ERFA_DAU differs from NIST: ERFA={erfa_au_m}, NIST={nist_au_m}")

  target_au_km = _single_decimal(rf"AU_KM_SCALE\s*=\s*({DECIMAL})", toolbox, "repository AU_KM_SCALE")
  _require(target_au_km * Decimal(1000) == erfa_au_m, f"AU_KM_SCALE differs: source={erfa_au_m}, target={target_au_km}")

  definitions = (
    (
      "J2000",
      _macro_decimal(erfam, "ERFA_DJ00"),
      _single_decimal(rf"J2000\s*=\s*({DECIMAL})", julian_day, "repository J2000"),
    ),
    (
      "DAYS_PER_JULIAN_CENTURY",
      _macro_decimal(erfam, "ERFA_DJC"),
      _single_decimal(
        rf"DAYS_PER_JULIAN_CENTURY\s*=\s*({DECIMAL})",
        julian_day,
        "repository DAYS_PER_JULIAN_CENTURY",
      ),
    ),
    (
      "jde_to_jm divisor",
      _macro_decimal(erfam, "ERFA_DJM"),
      _single_decimal(rf"\(jde\s*-\s*J2000\)\s*/\s*({DECIMAL})", julian_day, "repository jde_to_jm divisor"),
    ),
    (
      "jm_to_jde multiplier",
      _macro_decimal(erfam, "ERFA_DJM"),
      _single_decimal(rf"jm\s*\*\s*({DECIMAL})", julian_day, "repository jm_to_jde multiplier"),
    ),
  )
  for label, source_value, target_value in definitions:
    _require(source_value == target_value, f"{label} differs: source={source_value}, target={target_value}")

  target_light_time = _single_decimal(
    rf"LIGHT_TIME_DAYS_PER_AU\s*=\s*({DECIMAL})",
    earth,
    "repository LIGHT_TIME_DAYS_PER_AU",
  )
  rounded_light_time = round(erfa_au_m / erfa_c / erfa_day_seconds, 10)
  _require(
    target_light_time == rounded_light_time,
    f"LIGHT_TIME_DAYS_PER_AU rounding differs: source={rounded_light_time}, target={target_light_time}",
  )

  return IdentityCounts(
    perturbation_terms=perturbation_terms,
    obliquity_coefficients=obliquity_coefficients,
    precession_coefficients=precession_coefficients,
    lunar_distance_baselines=1,
    definition_sites=1 + len(definitions),
    rounded_relations=1,
  )
