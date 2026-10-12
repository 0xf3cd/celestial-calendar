# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import hashlib

from shutil import copy2

import pytest

from automation.vsop87d_import import (
  CHECK_GENERATED_PATH,
  CHECK_JDS,
  PLANETS,
  REPO_ROOT,
  ImportCounts,
  PlanetSpec,
  parse_check_source,
  parse_source,
  render_header,
  verify_repository,
)
from automation import vsop87d_import


PLANET_NAMES = ("MERCURY", "VENUS", "EARTH", "MARS", "JUPITER", "SATURN", "URANUS", "NEPTUNE")


def _sha256(data: bytes) -> str:
  return hashlib.sha256(data).hexdigest()


def _term(index: int, A: str = "0.00000000001", B: str = "1.00000000000", C: str = "2.00000000000") -> str:
  line = [" "] * 132
  line[5:10] = f"{index:5d}"
  line[79:97] = f"{A:>18}"
  line[97:111] = f"{B:>14}"
  line[111:132] = f"{C:>21}"
  return "".join(line)


def _series_header(planet: str, variable: int, power: int, terms: int) -> str:
  suffix = "TERM" if terms == 1 else "TERMS"
  return (
    f" VSOP87 VERSION D4    {planet:<9} VARIABLE {variable} (LBR)       *T**{power} "
    f"{terms:6d} {suffix:<8} HELIOCENTRIC DYNAMICAL ECLIPTIC AND EQUINOX OF THE DATE"
  )


def _synthetic_source(header_terms: int = 1) -> bytes:
  lines = []
  for variable in (1, 2, 3):
    lines.append(_series_header("TEST", variable, 0, header_terms))
    lines.append(_term(1))
  return ("\n".join(lines) + "\n").encode("ascii")


def _synthetic_spec(data: bytes) -> PlanetSpec:
  return PlanetSpec(
    key="test",
    display_name="TEST",
    enum_name="TEST",
    source_name="VSOP87D.test",
    source_sha256=_sha256(data),
    tables=(("L0", 1), ("B0", 1), ("R0", 1)),
    generated_sha256="unused",
  )


def _synthetic_check_source(omit_last: bool = False) -> bytes:
  lines = []
  for planet in PLANET_NAMES:
    for jd in CHECK_JDS:
      lines.extend(
        (
          f" VSOP87D  {planet:<11} JD{jd}  01/01/2000 12h TDB",
          " l   1.0000000000 rad       b   -.1000000000 rad       r    .5000000000  au",
          " l'   .0100000000 rad/d     b'   .0000000000 rad/d     r'   .0000000000  au/d",
          "",
        )
      )
  if omit_last:
    del lines[-4:]
  return "\n".join(lines).encode("ascii")


def test_repository_coefficient_and_check_values_are_pinned():
  assert verify_repository() == ImportCounts(planets=7, series=123, terms=29152, check_rows=80)


def test_source_parser_preserves_decimal_digits_and_real_series_shape():
  data = _synthetic_source()
  spec = _synthetic_spec(data)

  series = parse_source(data, spec)

  assert tuple(table.name for table in series) == ("L0", "B0", "R0")
  assert all(table.terms[0].A == "0.001" for table in series)
  assert all(table.terms[0].B == "1.00000000000" for table in series)
  assert b"{ 0.001, 1.00000000000, 2.00000000000, }" in render_header(spec, series)


def test_source_hash_mutation_fails_before_parsing():
  data = _synthetic_source()
  spec = _synthetic_spec(data)

  with pytest.raises(RuntimeError, match="source hash mismatch"):
    parse_source(data + b" ", spec)


def test_source_term_count_mismatch_fails():
  data = _synthetic_source(header_terms=2)
  spec = _synthetic_spec(data)

  with pytest.raises(RuntimeError, match="L0 term count differs"):
    parse_source(data, spec)


def test_check_parser_requires_all_eight_planets_and_ten_epochs():
  data = _synthetic_check_source()

  rows = parse_check_source(data, source_sha256=_sha256(data))

  assert len(rows) == 80
  assert rows[0].planet == "MER"
  assert rows[-1].planet == "NEP"
  assert rows[0].β == "-.1000000000"


def test_check_parser_rejects_an_incomplete_planet_block():
  data = _synthetic_check_source(omit_last=True)

  with pytest.raises(RuntimeError, match="block counts differ"):
    parse_check_source(data, source_sha256=_sha256(data))


def test_generated_header_mutation_fails(tmp_path):
  spec = PLANETS[0]
  coefficient = tmp_path / spec.generated_path
  coefficient.parent.mkdir(parents=True)
  copy2(REPO_ROOT / spec.generated_path, coefficient)
  check_data = tmp_path / CHECK_GENERATED_PATH
  check_data.parent.mkdir(parents=True)
  copy2(REPO_ROOT / CHECK_GENERATED_PATH, check_data)
  text = coefficient.read_text(encoding="utf-8")
  coefficient.write_text(text.replace("440250710.144", "440250710.145", 1), encoding="utf-8")
  assert coefficient.read_text(encoding="utf-8") != text

  with pytest.raises(RuntimeError, match="generated data hash mismatch"):
    verify_repository(repo_root=tmp_path, specs=(spec,))


def test_replay_accepts_header_prose_but_rejects_changed_coefficients(tmp_path, monkeypatch):
  data = _synthetic_source()
  spec = _synthetic_spec(data)
  check = _synthetic_check_source()
  sources = tmp_path / "sources"
  sources.mkdir()
  (sources / spec.source_name).write_bytes(data)
  check_file = sources / "vsop87.chk"
  check_file.write_bytes(check)
  output = tmp_path / "output"
  (output / spec.generated_path).parent.mkdir(parents=True)
  (output / CHECK_GENERATED_PATH).parent.mkdir(parents=True)
  original_planets = vsop87d_import.PLANETS
  monkeypatch.setattr(vsop87d_import, "PLANETS", (spec,))

  def parse_checks(raw):
    with monkeypatch.context() as checker:
      checker.setattr(vsop87d_import, "PLANETS", original_planets)
      return parse_check_source(raw, source_sha256=_sha256(check))

  monkeypatch.setattr(vsop87d_import, "parse_check_source", parse_checks)
  expected = ImportCounts(1, 3, 3, 80)
  assert vsop87d_import.replay(sources, check_file, output, write=True) == expected
  header = output / spec.generated_path
  header.write_text(header.read_text(encoding="utf-8") + "\n// Equivalent project header note.\n", encoding="utf-8")
  assert vsop87d_import.replay(sources, check_file, output) == expected
  text = header.read_text(encoding="utf-8")
  assert "0.001" in text
  header.write_text(text.replace("0.001", "0.002", 1), encoding="utf-8")
  with pytest.raises(RuntimeError, match="data differ from the deterministic replay"):
    vsop87d_import.replay(sources, check_file, output)
