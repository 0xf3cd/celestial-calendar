# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import subprocess
import sys

import pytest

from automation import gtest
from automation.gtest import parse_ctest_listing
from automation.utils import ProcReturn


# Sample rows verbatim from a real `ctest -N` listing (build/test, collected 2026-08-12):
# "  Test   #N:" single-digit rows carry three spaces, "  Test  #70" carries two -- the
# padding aligns on the widest id, and the parser keys keep it all.
CANONICAL = """\
Test project $REPO/build/test
  Test   #1: CoordTransform.EclipticToEquatorialMeeus13a
  Test   #2: CoordTransform.EclipticToEquatorialPoles
  Test  #70: Moon.CoordAndPpi
Total Tests: 3
"""


def test_canonical_listing():
  # Keys keep ctest's padding ("Test   #1" vs "Test  #70"): find_gtests' numeric match
  # relies on rpartition("#"), not on a cleaned-up key.
  assert parse_ctest_listing(CANONICAL) == {
    "Test   #1": "CoordTransform.EclipticToEquatorialMeeus13a",
    "Test   #2": "CoordTransform.EclipticToEquatorialPoles",
    "Test  #70": "Moon.CoordAndPpi",
  }


def test_non_entry_lines_are_ignored():
  # "Test" without "#" (header), "Tests" footer, "#" without "Test" -- none of them
  # name a test.
  assert parse_ctest_listing("Test project /x\nTotal Tests: 3\n#1: notatest\n") == {}


def test_empty_listing():
  assert parse_ctest_listing("") == {}


@pytest.fixture
def test_directory(tmp_path, monkeypatch):
  monkeypatch.setattr(gtest, "TEST_DIR", tmp_path)
  return tmp_path


@pytest.mark.parametrize(
  "listing",
  [
    "",
    "Total Tests: 0\n",
    "Total Tests: 3\n",
    CANONICAL.replace("Total Tests: 3", ""),
    CANONICAL.replace("Total Tests: 3", "Total Tests: 4"),
  ],
)
def test_discovery_rejects_empty_or_incomplete_listing(test_directory, monkeypatch, listing):
  monkeypatch.setattr(gtest, "run_cmd", lambda *args, **kwargs: ProcReturn(0, listing, ""))
  with pytest.raises(RuntimeError, match="configure|registration"):
    gtest.list_gtests()


@pytest.mark.parametrize("path_kind", ["missing", "file"])
@pytest.mark.parametrize("entry", ["list", "find", "single", "all"])
def test_invalid_directory_fails_explicitly(test_directory, monkeypatch, path_kind, entry):
  path = test_directory / path_kind
  if path_kind == "file":
    path.write_text("not a directory", encoding="utf-8")
  monkeypatch.setattr(gtest, "TEST_DIR", path)
  calls = {
    "list": gtest.list_gtests,
    "find": lambda: gtest.find_gtests([]),
    "single": lambda: gtest.run_gtest("Suite.Case"),
    "all": gtest.run_gtests,
  }
  with pytest.raises(RuntimeError, match="[Tt]est directory"):
    calls[entry]()


def test_discovery_command_failure(test_directory, monkeypatch):
  monkeypatch.setattr(gtest, "run_cmd", lambda *args, **kwargs: ProcReturn(8, CANONICAL, "discovery failed"))
  with pytest.raises(RuntimeError, match="ctest -N"):
    gtest.list_gtests()


@pytest.mark.parametrize("row", ["  Test #1: Other.Name\n", "  Test #3: Moon.CoordAndPpi\n"])
def test_duplicate_registration_is_rejected(test_directory, monkeypatch, row):
  listing = CANONICAL.replace("Total Tests: 3", row + "Total Tests: 4")
  monkeypatch.setattr(gtest, "run_cmd", lambda *args, **kwargs: ProcReturn(0, listing, ""))
  with pytest.raises((ValueError, RuntimeError), match="[Dd]uplicate"):
    gtest.list_gtests()


def test_run_reuses_one_listing(test_directory, monkeypatch):
  calls = []
  executed = []

  def listing():
    calls.append("list")
    if len(calls) > 1:
      raise RuntimeError("discovery changed")
    return parse_ctest_listing(CANONICAL)

  def run(test, *args):
    executed.append(test)
    return ProcReturn(0, "", "")

  monkeypatch.setattr(gtest, "list_gtests", listing)
  monkeypatch.setattr(gtest, "run_gtest", run)
  assert gtest.run_gtests(["70", "CoordTransform", "70"]) == 0
  assert calls == ["list"]
  assert executed == list(parse_ctest_listing(CANONICAL).values())


@pytest.mark.parametrize("keyword", ["does-not-exist", "7"])
def test_no_match_fails(test_directory, monkeypatch, keyword):
  monkeypatch.setattr(gtest, "list_gtests", lambda: parse_ctest_listing(CANONICAL))
  with pytest.raises(ValueError, match="[Nn]o tests|Did not find"):
    gtest.run_gtests([keyword])


def test_test_failure_stops_the_run(test_directory, monkeypatch):
  monkeypatch.setattr(gtest, "list_gtests", lambda: parse_ctest_listing(CANONICAL))
  executed = []

  def run(test, *args):
    executed.append(test)
    return ProcReturn(9, "test failed", "")

  monkeypatch.setattr(gtest, "run_gtest", run)
  assert gtest.run_gtests() == 9
  assert len(executed) == 1


@pytest.mark.parametrize("count", [0, 1, 2])
def test_single_run_checks_actual_gtest_count(test_directory, monkeypatch, count):
  calls = []

  def command(cmd, **kwargs):
    calls.append(cmd)
    return ProcReturn(0, f"1: [==========] {count} tests from 1 test suite ran. (0 ms total)\n", "")

  monkeypatch.setattr(gtest, "run_cmd", command)
  assert (gtest.run_gtest("Suite.Case").retcode == 0) == (count == 1)
  assert "--no-tests=error" in calls[0]
  assert "-V" in calls[0]
  assert "^Suite\\.Case$" in calls[0]


def test_success_without_gtest_output_is_rejected(test_directory, monkeypatch):
  monkeypatch.setattr(gtest, "run_cmd", lambda *args, **kwargs: ProcReturn(0, "100% tests passed\n", ""))
  assert gtest.run_gtest("Suite.Case").retcode != 0


def test_colored_gtest_summary_is_recognized(test_directory, monkeypatch):
  output = "1: \x1b[0;32m[==========]\x1b[m 1 test from 1 test suite ran. (0 ms total)\n"
  monkeypatch.setattr(gtest, "run_cmd", lambda *args, **kwargs: ProcReturn(0, output, ""))
  assert gtest.run_gtest("Suite.Case").retcode == 0


@pytest.mark.parametrize("optimized", [False, True])
def test_real_empty_ctest_directory_fails_with_or_without_optimization(test_directory, optimized):
  code = (
    "from pathlib import Path\n"
    "from automation import gtest\n"
    f"gtest.TEST_DIR = Path({str(test_directory)!r})\n"
    "raise SystemExit(gtest.run_gtests())\n"
  )
  command = [sys.executable, *(["-O"] if optimized else []), "-c", code]
  result = subprocess.run(command, capture_output=True, text=True, check=False)
  assert result.returncode != 0
  assert "configure" in result.stderr
