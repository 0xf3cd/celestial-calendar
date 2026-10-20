# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Replay the shared event fixtures through an exact installed wheel (#303)."""

from __future__ import annotations

import json
from pathlib import Path

import celestial_calendar as celestial

from golden_replay import bits_from_float

# The recorded direction/motion codes are the C ABI's EVENT_DIRECTION_* / EVENT_MOTION_*
# values; c_header_check.c pins their order, so index the public enums directly.
DIRECTIONS = (
  celestial.EventDirection.INCREASING,
  celestial.EventDirection.DECREASING,
  celestial.EventDirection.TANGENT,
)
MOTIONS = (
  celestial.StationMotion.DIRECT,
  celestial.StationMotion.RETROGRADE,
)


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
GOLDEN = REPO / "toolbox" / "bindings_golden.json"


def check_bits(label: str, actual: float, expected_bits: str) -> None:
  """Require one replayed double to match its recorded bit pattern exactly."""
  assert bits_from_float(actual) == expected_bits, (
    f"{label}: replayed {actual:.17g} ({bits_from_float(actual)}) != recorded {expected_bits}"
  )


def replay_longitude_events(section: dict) -> int:
  """Replay the crossing fixtures: counts, ordering, times, directions, longitudes, rates."""
  replayed = 0
  for entry in section["entries"]:
    events = celestial.longitude_events(
      celestial.EventTarget(entry["target_code"]),
      entry["target_longitude_deg"],
      entry["start_jde_tt"],
      entry["end_jde_tt"],
    )
    assert len(events) == len(entry["events"]), f"{entry['name']}: count drifted"
    for index, (actual, recorded) in enumerate(zip(events, entry["events"], strict=True)):
      label = f"{entry['name']}[{index}]"
      assert actual.direction == DIRECTIONS[recorded["direction_code"]], label
      check_bits(f"{label}.jde_tt", actual.jde_tt, recorded["jde_tt_bits"])
      check_bits(f"{label}.longitude_deg", actual.longitude_deg, recorded["longitude_deg_bits"])
      check_bits(
        f"{label}.longitude_rate_deg_per_tt_day",
        actual.longitude_rate_deg_per_tt_day,
        recorded["longitude_rate_deg_per_tt_day_bits"],
      )
      if index > 0:
        assert events[index - 1].jde_tt < actual.jde_tt, f"{label}: ordering drifted"
      replayed += 1
  return replayed


def replay_station_events(section: dict) -> int:
  """Replay the station fixture: both motion directions and the certified fields."""
  replayed = 0
  for entry in section["entries"]:
    events = celestial.longitude_stations(
      celestial.EventTarget(entry["target_code"]),
      entry["start_jde_tt"],
      entry["end_jde_tt"],
    )
    assert len(events) == len(entry["events"]), f"{entry['name']}: count drifted"
    motions = set()
    for index, (actual, recorded) in enumerate(zip(events, entry["events"], strict=True)):
      label = f"{entry['name']}[{index}]"
      assert actual.motion_before == MOTIONS[recorded["motion_before_code"]], label
      assert actual.motion_after == MOTIONS[recorded["motion_after_code"]], label
      motions.add((actual.motion_before, actual.motion_after))
      check_bits(f"{label}.jde_tt", actual.jde_tt, recorded["jde_tt_bits"])
      check_bits(f"{label}.longitude_deg", actual.longitude_deg, recorded["longitude_deg_bits"])
      check_bits(
        f"{label}.longitude_rate_deg_per_tt_day",
        actual.longitude_rate_deg_per_tt_day,
        recorded["longitude_rate_deg_per_tt_day_bits"],
      )
      replayed += 1
    assert motions == {
      (celestial.StationMotion.DIRECT, celestial.StationMotion.RETROGRADE),
      (celestial.StationMotion.RETROGRADE, celestial.StationMotion.DIRECT),
    }, f"{entry['name']}: both station directions must appear"
  return replayed


def replay_difference_events(section: dict) -> int:
  """Replay the pair fixtures: both orientations of the non-symmetric target."""
  replayed = 0
  for entry in section["entries"]:
    events = celestial.longitude_difference_events(
      celestial.EventTarget(entry["first_target_code"]),
      celestial.EventTarget(entry["second_target_code"]),
      entry["target_difference_deg"],
      entry["start_jde_tt"],
      entry["end_jde_tt"],
    )
    assert len(events) == len(entry["events"]), f"{entry['name']}: count drifted"
    for index, (actual, recorded) in enumerate(zip(events, entry["events"], strict=True)):
      label = f"{entry['name']}[{index}]"
      assert actual.direction == DIRECTIONS[recorded["direction_code"]], label
      check_bits(f"{label}.jde_tt", actual.jde_tt, recorded["jde_tt_bits"])
      check_bits(f"{label}.longitude_a_deg", actual.longitude_a_deg, recorded["longitude_a_deg_bits"])
      check_bits(f"{label}.longitude_b_deg", actual.longitude_b_deg, recorded["longitude_b_deg_bits"])
      check_bits(
        f"{label}.longitude_rate_a_deg_per_tt_day",
        actual.longitude_rate_a_deg_per_tt_day,
        recorded["longitude_rate_a_deg_per_tt_day_bits"],
      )
      check_bits(
        f"{label}.longitude_rate_b_deg_per_tt_day",
        actual.longitude_rate_b_deg_per_tt_day,
        recorded["longitude_rate_b_deg_per_tt_day_bits"],
      )
      check_bits(f"{label}.signed_difference_deg", actual.signed_difference_deg, recorded["signed_difference_deg_bits"])
      replayed += 1
  return replayed


def main() -> None:
  """Replay every recorded event and require bit-exact agreement with the wheel."""
  golden = json.loads(GOLDEN.read_text(encoding="utf-8"))
  assert golden["schema"] == "celestial-calendar/bindings-golden@2"
  # Native wheels replay the recorded 2^-30-day reporting lattice bit-for-bit — the exactness
  # the phases section already holds across the four platforms. If a platform ever diverges,
  # measure the spread and pin a margin here like golden_replay.py does, never loosen silently.
  replayed = 0
  replayed += replay_longitude_events(golden["sections"]["longitude_events"])
  replayed += replay_station_events(golden["sections"]["longitude_stations"])
  replayed += replay_difference_events(golden["sections"]["longitude_difference_events"])
  assert replayed == 10, f"event replay count {replayed}"
  print(f"PASS installed-wheel event replay {replayed}/10 bit-exact across six fixtures")


if __name__ == "__main__":
  main()
