# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

from pathlib import Path
from shutil import copy2

import pytest

from automation.sofa_identity import (
  REPO_ROOT,
  SOFA_ARCHIVE_SHA256,
  SOFA_ARCHIVE_URL,
  SOFA_INPUTS,
  SOFA_ROOT,
  IdentityCounts,
  verify_sofa_identities,
)


TARGET_FILES = (
  Path("src/astro/leap_second.hpp"),
  Path("src/astro/elp2000_82b.hpp"),
  Path("src/astro/earth.hpp"),
)


def materialize_inputs(destination: Path) -> Path:
  sofa_root = destination / SOFA_ROOT.relative_to(REPO_ROOT)
  for source in SOFA_INPUTS:
    target = sofa_root / source.path
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(SOFA_ROOT / source.path, target)
  for relative_path in TARGET_FILES:
    target = destination / relative_path
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / relative_path, target)
  return sofa_root


def replace_once(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  assert text.count(old) == 1
  path.write_text(text.replace(old, new), encoding="utf-8")


def test_sofa_inputs_and_repository_tables_are_pinned():
  assert SOFA_ARCHIVE_URL == "https://www.iausofa.org/s/sofa_c-20231011tar.gz"
  assert SOFA_ARCHIVE_SHA256 == "d9c10833cae8b4d9361a0ffda31ec361fd1262362025bec4d4e51a880150ace2"
  assert len(SOFA_INPUTS) == 5
  assert verify_sofa_identities() == IdentityCounts(28, 120, 106, 63, 12)


def test_sofa_derived_work_markings_are_pinned():
  required_markings = {
    Path("src/astro/leap_second.hpp"): (
      "These 28 post-1972 rows are unchanged from SOFA's user-replaceable `iauDat` table",
      "pre-1972 drift rows and routine body are not used",
      "This project is not SOFA software",
      "is not endorsed by SOFA",
    ),
    Path("src/astro/elp2000_82b.hpp"): (
      "These are the truncated lunar tables used by SOFA issue 2023-10-11 `moon98.c`",
      "angular amplitudes scaled by 1e6 to integer microdegrees",
      "They are not the full ELP2000-82B model",
      "This project is not SOFA software and is not endorsed by SOFA",
    ),
    Path("src/astro/earth.hpp"): (
      "This 63-row IAU 1980 truncation is derived from SOFA issue 2023-10-11 `nut80.c`",
      "terms with |longitude constant| >= 3",
      "permute (l,l',F,D,Om) to (D,M,M',F,Om)",
      "obliquity constants with magnitude below 3",
      "repository's original table order",
      "The complete 106-row IAU 1980 table is unchanged from SOFA issue 2023-10-11 `nut80.c`",
      "with arguments reordered from (l,l',F,D,Om) to (D,M,M',F,Om)",
      "software and is not endorsed by SOFA",
    ),
  }

  for path, markings in required_markings.items():
    source = " ".join(
      line.strip().removeprefix("//").strip() for line in (REPO_ROOT / path).read_text(encoding="utf-8").splitlines()
    )
    for marking in markings:
      assert marking in source


@pytest.mark.parametrize("mutation", ["leap", "lunar", "iau1980", "meeus-order"])
def test_sofa_identity_mutations_fail(tmp_path, mutation):
  sofa_root = materialize_inputs(tmp_path)
  if mutation == "leap":
    replace_once(
      tmp_path / "src/astro/leap_second.hpp",
      "{ util::to_ymd(1972, 1, 1), 10.0 }",
      "{ util::to_ymd(1972, 1, 1), 10.5 }",
    )
    message = "leap-second row 1 differs"
  elif mutation == "lunar":
    replace_once(
      tmp_path / "src/astro/elp2000_82b.hpp",
      "{ 0,  0,  1,  0, 6288774, -20905355 }",
      "{ 0,  0,  1,  0, 6288775, -20905355 }",
    )
    message = "lunar LR row 1 differs"
  elif mutation == "iau1980":
    replace_once(
      tmp_path / "src/astro/earth.hpp",
      "{ { -2, -2,  0,  2,  1 }, {      -2.0,    0.0 }, {     1.0,  0.0 } },",
      "{ { -2, -2,  0,  2,  1 }, {      -1.0,    0.0 }, {     1.0,  0.0 } },",
    )
    message = "IAU 1980 nutation row 7 differs"
  else:
    earth = tmp_path / "src/astro/earth.hpp"
    first = "  { {  0,  0,  0,  0,  1 }, { -171996.0, -174.2 }, { 92025.0,  8.9 } },\n"
    second = "  { { -2,  0,  0,  2,  2 }, {  -13187.0,   -1.6 }, {  5736.0, -3.1 } },\n"
    text = earth.read_text(encoding="utf-8")
    assert text.count(first) == 2
    assert text.count(second) == 2
    marker = "std::array<NutationCoeffs, 63> MEEUS_NUTATION_COEFFS {{"
    before, table = text.split(marker, maxsplit=1)
    table = table.replace(first + second, second + first, 1)
    earth.write_text(before + marker + table, encoding="utf-8")
    message = "Meeus nutation row 1 differs"

  with pytest.raises(RuntimeError, match=message):
    verify_sofa_identities(repo_root=tmp_path, sofa_root=sofa_root)


def test_sofa_source_mutation_fails_the_hash_pin(tmp_path):
  sofa_root = materialize_inputs(tmp_path)
  nut80 = sofa_root / "src/nut80.c"
  nut80.write_bytes(nut80.read_bytes() + b"changed")

  with pytest.raises(RuntimeError, match="SOFA input hash mismatch for src/nut80.c"):
    verify_sofa_identities(repo_root=tmp_path, sofa_root=sofa_root)
