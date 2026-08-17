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

"""Replay the shared native golden dataset through an exact installed wheel."""

from __future__ import annotations

import json
import struct
from dataclasses import dataclass
from pathlib import Path

import celestial_calendar as celestial


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
GOLDEN = REPO / "toolbox" / "bindings_golden.json"
SECTION_COUNTS = {"jieqi": 204, "moon": 41, "sidereal": 43, "moon_position_angle": 41, "phases": 60}
PHASES = (
  celestial.MoonPhase.NEW,
  celestial.MoonPhase.FIRST_QUARTER,
  celestial.MoonPhase.FULL,
  celestial.MoonPhase.LAST_QUARTER,
)


def float_from_bits(bits: str) -> float:
  """Decode one hexadecimal IEEE-754 binary64 pattern."""
  return struct.unpack("<d", struct.pack("<Q", int(bits, 16)))[0]


def bits_from_float(value: float) -> str:
  """Encode one float as a hexadecimal IEEE-754 binary64 pattern."""
  return f"0x{struct.unpack('<Q', struct.pack('<d', value))[0]:016x}"


@dataclass
class Residual:
  """Accumulate one output column before the exact-bit verdict."""

  points: int = 0
  mismatches: int = 0
  maximum: float = -1.0
  worst: str = ""

  def observe(self, actual: float, expected_bits: str, coordinate: str) -> None:
    """Record one comparison without failing early."""
    expected = float_from_bits(expected_bits)
    difference = abs(actual - expected)
    self.points += 1
    self.mismatches += bits_from_float(actual) != expected_bits
    if difference > self.maximum:
      self.maximum = difference
      self.worst = coordinate

  def report(self, name: str) -> None:
    """Print the measurement before exact equality is asserted."""
    print(
      f"RESIDUAL {name} points={self.points} bit_mismatches={self.mismatches} "
      f"max_abs={self.maximum:.17g} worst={self.worst}"
    )


def main() -> None:
  """Replay every encoded point and report all residual columns before asserting."""
  golden = json.loads(GOLDEN.read_text(encoding="utf-8"))
  assert golden["schema"] == "celestial-calendar/bindings-golden@2"
  section_counts = {name: len(section["entries"]) for name, section in golden["sections"].items()}
  assert section_counts == SECTION_COUNTS

  residuals = {
    "jieqi_fraction_days": Residual(),
    "moon_illumination": Residual(),
    "moon_elongation_deg": Residual(),
    "sidereal_deg": Residual(),
    "moon_position_angle_deg": Residual(),
    "phase_jde": Residual(),
  }
  exact_failures = []
  replayed = 0

  for point in golden["sections"]["jieqi"]["entries"]:
    value = celestial.jieqi_moment(point["year"], celestial.Jieqi(point["idx"]))
    moment = value.moment_ut1
    actual_fields = (value.jieqi.value, moment.year, moment.month, moment.day)
    expected_fields = (point["idx"], point["y"], point["m"], point["d"])
    if actual_fields != expected_fields:
      exact_failures.append(f"jieqi {point['year']}:{point['idx']} {actual_fields} != {expected_fields}")
    residuals["jieqi_fraction_days"].observe(
      moment.fraction, point["frac_bits"], f"year={point['year']} idx={point['idx']}"
    )
    replayed += 1

  for point in golden["sections"]["moon"]["entries"]:
    jde = float_from_bits(point["jde_bits"])
    value = celestial.moon_illumination(jde)
    residuals["moon_illumination"].observe(value.fraction, point["illumination_bits"], f"jde={jde:.17g}")
    residuals["moon_elongation_deg"].observe(
      value.elongation_deg, point["elongation_deg_bits"], f"jde={jde:.17g}"
    )
    replayed += 1

  for point in golden["sections"]["sidereal"]["entries"]:
    jd_ut1 = float_from_bits(point["jd_ut1_bits"])
    value = celestial.local_apparent_sidereal_time(jd_ut1, point["longitude"])
    residuals["sidereal_deg"].observe(
      value, point["value_bits"], f"jd_ut1={jd_ut1:.17g} longitude={point['longitude']:.17g}"
    )
    replayed += 1

  for point in golden["sections"]["moon_position_angle"]["entries"]:
    jde = float_from_bits(point["jde_bits"])
    value = celestial.moon_bright_limb_position_angle(jde)
    residuals["moon_position_angle_deg"].observe(value, point["angle_deg_bits"], f"jde={jde:.17g}")
    replayed += 1

  for point in golden["sections"]["phases"]["entries"]:
    values = celestial.moon_phase_moments(point["year"], PHASES[point["phase_kind"]])
    value = values[point["index"]]
    residuals["phase_jde"].observe(
      value,
      point["jde_bits"],
      f"year={point['year']} phase_kind={point['phase_kind']} index={point['index']}",
    )
    replayed += 1

  for name, residual in residuals.items():
    residual.report(name)
  print(f"EXACT_FIELDS mismatches={len(exact_failures)}")

  assert replayed == sum(SECTION_COUNTS.values()) == 389
  assert not exact_failures, "; ".join(exact_failures)
  assert all(residual.mismatches == 0 for residual in residuals.values()), "non-zero wheel residuals reported above"
  print(f"PASS exact installed-wheel golden replay {replayed}/389")


if __name__ == "__main__":
  main()
