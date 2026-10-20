# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

from pathlib import Path

import pytest

from automation.c_header_pins import collect_pin_sets, verify_c_header_pins

REPO = Path(__file__).resolve().parents[2]


def test_live_tree_pins_cover_every_struct_and_export():
  sets = verify_c_header_pins(REPO)
  assert sets.structs <= sets.layout_pins
  assert sets.struct_only_pins <= sets.structs
  assert sets.exports == sets.pinned_exports


def materialize(tmp_path: Path) -> None:
  shared = tmp_path / "src/shared_lib"
  shared.mkdir(parents=True)
  (shared / "celestial.h").write_text(
    (REPO / "src/shared_lib/celestial.h").read_text(encoding="utf-8"), encoding="utf-8"
  )
  (shared / "c_header_check.c").write_text(
    (REPO / "src/shared_lib/c_header_check.c").read_text(encoding="utf-8"), encoding="utf-8"
  )


class Layout:
  def __init__(self, root: Path):
    self.root = root
    self.header_path = root / "src/shared_lib/celestial.h"
    self.pins_path = root / "src/shared_lib/c_header_check.c"

  @property
  def header(self) -> str:
    return self.header_path.read_text(encoding="utf-8")

  @property
  def pins(self) -> str:
    return self.pins_path.read_text(encoding="utf-8")

  def drop_first_struct_pin(self) -> None:
    sets = collect_pin_sets(self.root)
    struct = sorted(sets.structs)[0]
    lines = [
      line for line in self.pins.splitlines() if f"offsetof({struct}," not in line and f"sizeof({struct})" not in line
    ]
    assert len(lines) < len(self.pins.splitlines())
    self.pins_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

  def drop_first_export_pin(self) -> None:
    sets = collect_pin_sets(self.root)
    export = sorted(sets.exports)[0]
    lines = [line for line in self.pins.splitlines() if f"_Generic(&{export}," not in line]
    assert len(lines) < len(self.pins.splitlines())
    self.pins_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def test_unpinned_struct_fails(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  layout.drop_first_struct_pin()
  with pytest.raises(RuntimeError, match="structs without a layout pin"):
    verify_c_header_pins(tmp_path)


def test_stale_struct_pin_fails(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  sets = collect_pin_sets(tmp_path)
  struct = sorted(sets.structs)[0]
  mutated = layout.pins.replace(f"offsetof({struct},", f"offsetof(NotA{struct},", 1)
  layout.pins_path.write_text(mutated, encoding="utf-8")
  with pytest.raises(RuntimeError, match="pins name no celestial.h struct"):
    verify_c_header_pins(tmp_path)


def test_unpinned_export_fails(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  layout.drop_first_export_pin()
  with pytest.raises(RuntimeError, match="exports without a signature pin"):
    verify_c_header_pins(tmp_path)


def test_commented_out_pin_does_not_count(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  sets = collect_pin_sets(tmp_path)
  export = sorted(sets.exports)[0]
  needle = f"_Static_assert(_Generic(&{export},"
  assert needle in layout.pins
  commented = layout.pins.replace(needle, f"/* commented out: {needle}", 1).replace(
    f'"{export} signature drifted");', f'"{export} signature drifted"); */', 1
  )
  layout.pins_path.write_text(commented, encoding="utf-8")
  with pytest.raises(RuntimeError, match="exports without a signature pin"):
    verify_c_header_pins(tmp_path)


def test_indented_declaration_is_still_an_export(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  indented = layout.header.replace(
    "typedef struct JulianDay {", "  CELESTIAL_API double probe_export(double year);\n\ntypedef struct JulianDay {", 1
  )
  layout.header_path.write_text(indented, encoding="utf-8")
  with pytest.raises(RuntimeError, match="exports without a signature pin"):
    verify_c_header_pins(tmp_path)


def test_sizeof_only_new_struct_is_unpinned(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  header = layout.header.replace(
    "typedef struct JulianDay {",
    "typedef struct ProbeOnly { int32_t x; int32_t y; } ProbeOnly;\n\ntypedef struct JulianDay {",
    1,
  )
  layout.header_path.write_text(header, encoding="utf-8")
  pins = layout.pins + '_Static_assert(sizeof(ProbeOnly) == 8, "probe");\n'
  layout.pins_path.write_text(pins, encoding="utf-8")
  with pytest.raises(RuntimeError, match="structs without a layout pin"):
    verify_c_header_pins(tmp_path)


def test_parenthesized_generic_pin_counts(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  sets = collect_pin_sets(tmp_path)
  export = sorted(sets.exports)[0]
  original = layout.pins
  # Any parenthesization depth around the pinned name must keep working in both directions:
  # the existing pin is still recognized, and a same-shaped pin naming a non-export is stale.
  for depth in range(5):
    parens = "(" * depth + export + ")" * depth
    mutated = original.replace(f"_Generic(&{export},", f"_Generic(&{parens},", 1)
    layout.pins_path.write_text(mutated, encoding="utf-8")
    verify_c_header_pins(tmp_path)
    fake = (
      "_Static_assert(_Generic(&" + parens.replace(export, "not_an_export") + ",\n"
      "               uint32_t (*)(void): 1,\n"
      '               default: 0) != 0, "not_an_export signature drifted");\n'
    )
    layout.pins_path.write_text(mutated + fake, encoding="utf-8")
    with pytest.raises(RuntimeError, match="pins name no celestial.h export"):
      verify_c_header_pins(tmp_path)
  layout.pins_path.write_text(original, encoding="utf-8")


def test_field_address_generic_is_not_a_signature_pin(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  field_address_pin = (
    '_Static_assert(_Generic(&((ChartBodyV1*)0)->target_code,\n    uint32_t*: 1, default: 0), "field address type");\n'
  )
  layout.pins_path.write_text(layout.pins + field_address_pin, encoding="utf-8")
  verify_c_header_pins(tmp_path)


def test_generic_keyword_may_be_spaced_from_its_paren(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  sets = collect_pin_sets(tmp_path)
  export = sorted(sets.exports)[0]
  original = layout.pins
  for separator in (" ", "\n"):
    spaced = original.replace(f"_Generic(&{export},", f"_Generic{separator}(&{export},", 1)
    layout.pins_path.write_text(spaced, encoding="utf-8")
    verify_c_header_pins(tmp_path)
    stale = spaced + (
      f"_Static_assert(_Generic{separator}(&not_an_export,\n"
      "               uint32_t (*)(void): 1,\n"
      '               default: 0) != 0, "not_an_export signature drifted");\n'
    )
    layout.pins_path.write_text(stale, encoding="utf-8")
    with pytest.raises(RuntimeError, match="pins name no celestial.h export"):
      verify_c_header_pins(tmp_path)
  layout.pins_path.write_text(original, encoding="utf-8")


def test_stale_export_pin_fails(tmp_path):
  materialize(tmp_path)
  layout = Layout(tmp_path)
  fake_pin = (
    "_Static_assert(_Generic(&not_an_export,\n"
    "               uint32_t (*)(void): 1,\n"
    '               default: 0) != 0, "not_an_export signature drifted");\n'
  )
  layout.pins_path.write_text(layout.pins + fake_pin, encoding="utf-8")
  with pytest.raises(RuntimeError, match="pins name no celestial.h export"):
    verify_c_header_pins(tmp_path)
