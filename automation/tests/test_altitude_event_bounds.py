# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Check the production altitude-certificate emitter and generator failure boundaries."""

import copy
import os
import runpy
import subprocess
import sys
import tempfile
import unittest
from fractions import Fraction as F
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "statistics"))
from altitude_bounds.certificate_encoding import encode  # noqa: E402


GENERATOR_PATH = ROOT / "statistics/altitude_event_bounds.py"
GENERATOR = runpy.run_path(str(GENERATOR_PATH))


def packet():
  values = (F(1), F(2), F(3), F(4), F(5), F(1, 2), F(1, 1000), F(0), F(2), F(0))
  return {"values": values, "codes": tuple(encode(value, index != 5) for index, value in enumerate(values))}


def near_fixture():
  return {
    "constants_exact": ("2", "1", "100"),
    "clock": {
      "tt_interval": ("2451545", "2451546"),
      "rotation": ("0", "1", "2", "3"),
      "year_error_upper": "1/1099511627776",
      "model_joins": [],
    },
    "proofs": {"pluto": {"observation_interval": ("2451545", "2451546")}},
    "records": [{"target": target, "ordinary": packet(), "collar": packet()} for target in GENERATOR["TARGETS"]],
  }


def local_fixture(target):
  fields = packet()
  return {
    "target": target,
    "interval": ("2451545", "9806181/4"),
    "rotation": ("6", "1/10"),
    "fields": fields["values"],
    "codes": fields["codes"],
  }


class AltitudeBoundEmitterTest(unittest.TestCase):
  def test_output_is_deterministic_and_does_not_mutate_the_input_records(self):
    near = near_fixture()
    before = copy.deepcopy(near)
    first = GENERATOR["emit"](near, [])
    self.assertEqual(first, GENERATOR["emit"](near, []))
    self.assertEqual(near, before)
    header, runtime = first
    self.assertIn("namespace astro::detail::altitude_bounds", header)
    self.assertNotIn("prototype", header)
    self.assertNotIn("feasibility", header)
    self.assertEqual(runtime["common"]["EARTH_RADIUS_AU"], "1/50")
    self.assertEqual(runtime["local_packets"], [])

  def test_local_targets_share_only_identical_complete_clock_rows(self):
    header, runtime = GENERATOR["emit"](near_fixture(), [local_fixture(4), local_fixture(5)])
    self.assertEqual(len(runtime["local_clocks"]), 1)
    self.assertEqual([row[:2] for row in runtime["local_packets"]], [(4, 0), (5, 0)])
    self.assertIn("std::array<LocalSource, 2>", header)
    self.assertIn("std::array<LocalClock, 1>", header)

  def test_all_ten_targets_and_all_ten_fields_are_required(self):
    for change in ("target", "field"):
      near = near_fixture()
      if change == "target":
        near["records"].pop()
      else:
        near["records"][0]["ordinary"]["codes"] = (0,)
      with self.subTest(change=change), self.assertRaises(ValueError):
        GENERATOR["emit"](near, [])

  def test_majorants_cannot_round_down_and_separation_lowers_cannot_round_up(self):
    for column, code in ((0, -1), (5, 0)):
      near = near_fixture()
      codes = list(near["records"][0]["ordinary"]["codes"])
      codes[column] = code
      near["records"][0]["ordinary"]["codes"] = codes
      with self.subTest(column=column), self.assertRaisesRegex(ArithmeticError, "inward"):
        GENERATOR["emit"](near, [])

  def test_collar_choice_cannot_change_a_source_derivative_field(self):
    near = near_fixture()
    codes = list(near["records"][0]["collar"]["codes"])
    codes[0] += 1
    near["records"][0]["collar"]["codes"] = codes
    with self.assertRaisesRegex(ArithmeticError, "Clock choice"):
      GENERATOR["emit"](near, [])

  def test_half_quarters_and_shifted_quarters_cannot_enter_the_runtime_table(self):
    for interval in (("2451545", "19612361/8"), ("19612361/8", "19612363/8")):
      row = local_fixture(6)
      row["interval"] = interval
      with self.subTest(interval=interval), self.assertRaisesRegex(ValueError, "complete canonical quarter"):
        GENERATOR["emit"](near_fixture(), [row])


class AltitudeGeneratorFailureTest(unittest.TestCase):
  def test_existing_output_directory_is_not_overwritten(self):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SCRATCH")) as directory:
      output = Path(directory) / "output"
      output.mkdir()
      marker = output / "marker"
      marker.write_text("unchanged")
      result = subprocess.run([sys.executable, str(GENERATOR_PATH), "--output", str(output)], capture_output=True)
      self.assertNotEqual(result.returncode, 0)
      self.assertEqual(list(output.iterdir()), [marker])
      self.assertEqual(marker.read_text(), "unchanged")

  def test_failed_source_derivation_does_not_publish_a_header_or_success_record(self):
    with tempfile.TemporaryDirectory(dir=os.environ.get("SCRATCH")) as directory:
      root = Path(directory) / "source"
      source = root / "src/astro"
      (source / "earth").mkdir(parents=True)
      for name in (
        "earth.hpp",
        "delta_t.hpp",
        "planet.hpp",
        "coord_transform.hpp",
        "observer.hpp",
        "toolbox.hpp",
        "pluto.hpp",
        "earth/precession.hpp",
      ):
        (source / name).write_text("")
      output = Path(directory) / "output"
      result = subprocess.run(
        [sys.executable, str(GENERATOR_PATH), "--root", str(root), "--output", str(output)], capture_output=True
      )
      self.assertNotEqual(result.returncode, 0)
      self.assertFalse((output / "altitude_event_bounds.hpp").exists())
      self.assertFalse((output / "derivation.json").exists())
      self.assertEqual((output / "progress.jsonl").read_text(), "")
