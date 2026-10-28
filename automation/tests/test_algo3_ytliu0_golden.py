# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import importlib.util
import sys
from pathlib import Path

import pytest


@pytest.fixture
def golden_command(monkeypatch):
  path = Path(__file__).resolve().parents[2] / "statistics/algo3_ytliu0_golden.py"
  spec = importlib.util.spec_from_file_location("algo3_golden_command_test", path)
  module = importlib.util.module_from_spec(spec)
  monkeypatch.setitem(sys.modules, spec.name, module)
  spec.loader.exec_module(module)
  return module


def test_sample_command_dispatches_with_the_explicit_sample(golden_command, monkeypatch):
  received = []

  def compare(args):
    received.append(args.sample)
    return 7

  monkeypatch.setattr(golden_command, "cmd_compare_sample", compare)
  assert golden_command.main(["compare-sample", "--sample", "sample.md"]) == 7
  assert received == ["sample.md"]


def test_removed_full_range_alias_fails_before_sample_loading(golden_command, monkeypatch, capsys):
  def forbidden(*args):
    pytest.fail("removed alias dispatched a sample comparison")

  monkeypatch.setattr(golden_command, "cmd_compare_sample", forbidden)
  with pytest.raises(SystemExit) as error:
    golden_command.main(["compare-all"])
  assert error.value.code == 2
  assert "invalid choice: 'compare-all'" in capsys.readouterr().err
