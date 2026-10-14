# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Compiler failures stay fatal; capability and TODO maintenance have distinct outcomes."""

from pathlib import Path

import pytest

from automation import feature_probe as features
from automation.utils import ProcReturn


@pytest.fixture
def compiler(monkeypatch):
  known = features.Feature("known", "known-token", "#1", "known-program")
  waiting = features.Feature("waiting", "waiting-token", "#2", "waiting-program")
  monkeypatch.setattr(features, "FEATURES", [known, waiting])
  monkeypatch.setattr(
    features,
    "EXPECTED",
    {
      "tested-leg": {"known": True, "waiting": False},
      "other-leg": {"known": True, "waiting": False},
    },
  )
  monkeypatch.setattr(features, "SITE_COUNTS", {"known-token": 0, "waiting-token": 1})
  monkeypatch.setattr(features, "todo_sites", lambda token: ["src/waiting.hpp:10"] if token == "waiting-token" else [])
  monkeypatch.setenv("CXX", "controlled-cxx")
  outcomes = {
    features.CANARY: ProcReturn(0, "", ""),
    features.ANTI_CANARY: ProcReturn(1, "", "error: anti-canary"),
    known.program: ProcReturn(0, "", ""),
    waiting.program: ProcReturn(1, "", "error: unsupported feature"),
    "--version": ProcReturn(0, "controlled compiler\n", ""),
  }
  calls = []

  def run(arguments, **keywords):
    calls.append(arguments)
    assert arguments[0] == "controlled-cxx"
    if arguments[1:] == ["--version"]:
      return outcomes["--version"]
    assert arguments[1:3] == ["-std=c++23", "-fsyntax-only"]
    source = Path(arguments[3]).read_text(encoding="utf-8")
    result = outcomes[source]
    if isinstance(result, Exception):
      raise result
    return result

  monkeypatch.setattr(features, "run_cmd", run)
  return outcomes, calls


def test_recorded_capabilities_match_even_with_idle_maintenance(compiler, capsys):
  _, calls = compiler
  assert features.probe_features("tested-leg") == 0
  assert len(calls) == 1 + 2 + len(features.FEATURES)
  assert "INFO: known has no waiting TODO sites" in capsys.readouterr().out


def test_unlock_is_information_not_a_failed_gate(compiler, capsys):
  outcomes, _ = compiler
  outcomes["waiting-program"] = ProcReturn(0, "", "")
  assert features.probe_features("tested-leg") == 0
  output = capsys.readouterr().out
  assert "INFO: waiting is now USABLE" in output and "src/waiting.hpp:10" in output
  assert "REGRESSION" not in output


def test_regression_remains_fatal_even_when_another_feature_unlocks(compiler, capsys):
  outcomes, _ = compiler
  outcomes["known-program"] = ProcReturn(1, "", "error: capability lost")
  outcomes["waiting-program"] = ProcReturn(0, "", "")
  assert features.probe_features("tested-leg") == 1
  output = capsys.readouterr().out
  assert "REGRESSION: known" in output and "INFO: waiting is now USABLE" in output


def test_adoption_and_count_drift_are_information(compiler, capsys):
  outcomes, _ = compiler
  outcomes["waiting-program"] = ProcReturn(0, "", "")
  for baseline in features.EXPECTED.values():
    baseline["waiting"] = True
  features.SITE_COUNTS["waiting-token"] = 2
  assert features.probe_features("tested-leg") == 0
  output = capsys.readouterr().out
  assert "INFO: TODO-site count drift" in output
  assert "INFO: waiting is usable on every recorded leg" in output


@pytest.mark.parametrize("leg", [None, "tested-leg"])
@pytest.mark.parametrize("failure", ["canary", "anti-canary", "signal", "silent", "ice", "missing", "version"])
def test_compiler_failures_do_not_become_maintenance_information(compiler, leg, failure):
  outcomes, _ = compiler
  if failure == "canary":
    outcomes[features.CANARY] = ProcReturn(1, "", "error: broken toolchain")
  elif failure == "anti-canary":
    outcomes[features.ANTI_CANARY] = ProcReturn(0, "", "")
  elif failure == "signal":
    outcomes["waiting-program"] = ProcReturn(-9, "", "killed")
  elif failure == "silent":
    outcomes["waiting-program"] = ProcReturn(1, "", "")
  elif failure == "ice":
    outcomes["waiting-program"] = ProcReturn(1, "", "PLEASE submit a bug report")
  elif failure == "missing":
    outcomes["waiting-program"] = FileNotFoundError("compiler disappeared")
  else:
    outcomes["--version"] = ProcReturn(1, "", "version query failed")
  assert features.probe_features(leg) == 1


def test_report_only_has_no_baseline_regression(compiler):
  outcomes, _ = compiler
  outcomes["known-program"] = ProcReturn(1, "", "error: absent locally")
  assert features.probe_features() == 0


@pytest.mark.parametrize("fault", ["missing", "extra", "nonbool", "other-leg", "count", "extra-count", "duplicate"])
def test_malformed_state_is_not_an_informational_reminder(compiler, fault):
  _, calls = compiler
  if fault == "missing":
    del features.EXPECTED["tested-leg"]["waiting"]
  elif fault == "extra":
    features.EXPECTED["tested-leg"]["unmeasured"] = False
  elif fault == "nonbool":
    features.EXPECTED["tested-leg"]["waiting"] = "False"
  elif fault == "other-leg":
    del features.EXPECTED["other-leg"]["waiting"]
  elif fault == "count":
    features.SITE_COUNTS["waiting-token"] = -1
  elif fault == "extra-count":
    features.SITE_COUNTS["unknown"] = 0
  else:
    features.FEATURES.append(features.FEATURES[0])
  assert features.probe_features("tested-leg") == 1
  assert len(calls) == 1


def test_unknown_leg_fails_before_compiling(compiler):
  _, calls = compiler
  assert features.probe_features("unknown-leg") == 1
  assert len(calls) == 1


def test_missing_compiler_version_query_is_fatal(monkeypatch):
  def missing(*arguments, **keywords):
    raise FileNotFoundError("no compiler")

  monkeypatch.setattr(features, "run_cmd", missing)
  assert features.probe_features() == 1


def test_todo_sites_derive_paths_and_counts_from_source(tmp_path, monkeypatch):
  source = tmp_path / "src"
  source.mkdir()
  (source / "kept.hpp").write_text("// TODO: generator\n// generator alone\n// TODO: another\n", encoding="utf-8")
  (source / "ignored.md").write_text("TODO: generator\n", encoding="utf-8")
  monkeypatch.setattr(features.paths, "cpp_src_dir", lambda: source)
  monkeypatch.setattr(features.paths, "proj_root", lambda: tmp_path)
  assert features.todo_sites("generator") == ["src/kept.hpp:1"]
