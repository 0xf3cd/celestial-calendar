# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""The installed-wheel runner must not accept an omitted or unregistered script."""

import importlib.util
import sys
from pathlib import Path

import pytest


SCRIPTS = {
  "abi/verify.py",
  "abi/raw_protocol.py",
  "consumer/smoke.py",
  "consumer/golden_replay.py",
  "consumer/chart_snapshot.py",
}


@pytest.mark.parametrize("fault", [None, "missing", "unregistered"])
def test_installed_acceptance_runner_reconciles_scripts_before_execution(tmp_path, monkeypatch, fault):
  source = Path(__file__).resolve().parents[2] / "bindings/python/test/run_all.py"
  spec = importlib.util.spec_from_file_location("acceptance_inventory_control", source)
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  module.__file__ = str(tmp_path / "run_all.py")
  for relative in SCRIPTS | {"consumer/chart_replay.py", "types/consumer.py", "wheel/verify.py"}:
    path = tmp_path / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.touch()
  if fault == "missing":
    (tmp_path / "consumer/chart_snapshot.py").unlink()
  elif fault == "unregistered":
    (tmp_path / "consumer/unregistered.py").touch()
  calls = []
  monkeypatch.setattr(module.subprocess, "run", lambda arguments, **keywords: calls.append((arguments, keywords)))
  if fault:
    with pytest.raises(AssertionError, match="acceptance inventory mismatch"):
      module.main()
    assert not calls
  else:
    module.main()
    assert len(calls) == len(SCRIPTS)
    assert {Path(arguments[1]).relative_to(tmp_path).as_posix() for arguments, _ in calls} == SCRIPTS
    assert all(arguments[0] == sys.executable and keywords == {"check": True} for arguments, keywords in calls)
