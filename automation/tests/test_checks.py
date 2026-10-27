# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import sys

import pytest

import checks


REQUESTS = (
  ("ruff", "run_ruff"),
  ("signature-layout", "check_cpp_signature_layout"),
  ("clang-tidy", "run_clang_tidy"),
  ("self-contained", "check_self_contained"),
  ("abi-layout", "check_abi_layout"),
  ("ai-workflows", "check_ai_workflows"),
  ("action-pins", "check_action_pins"),
  ("provenance", "check_provenance"),
  ("ctypes-smoke", "check_ctypes_smoke"),
  ("jieqi-table", "check_jieqi_table"),
  ("export-surface", "check_export_surface"),
  ("pytest", "run_pytest"),
  ("features", "probe_features"),
)


def dispatch(monkeypatch, argv, failure=None):
  calls = []
  for _flag, name in REQUESTS:

    def spy(*parameters, name=name):
      calls.append((name, parameters))
      return 9 if name == failure else 0

    monkeypatch.setattr(checks, name, spy)
  monkeypatch.setattr(sys, "argv", ["checks.py", *argv])
  result = checks.run_checks(checks.parse_args())
  return result, calls


@pytest.mark.parametrize("flag,name", REQUESTS)
def test_each_check_dispatches_once(monkeypatch, flag, name):
  result, calls = dispatch(monkeypatch, ["--" + flag, "--" + flag])
  assert result == 0
  assert calls == [(name, (None,) if flag == "features" else ())]


@pytest.mark.parametrize("leg", ["libc++", "libstdc++", "msvc-stl"])
def test_feature_leg_forwarding_with_all(monkeypatch, leg):
  result, calls = dispatch(monkeypatch, ["--all", "--features", leg])
  assert result == 0
  assert calls == [(name, (leg,) if flag == "features" else ()) for flag, name in REQUESTS]


@pytest.mark.parametrize("flag,name", REQUESTS)
def test_dispatch_preserves_first_failure_code_and_order(monkeypatch, flag, name):
  result, calls = dispatch(monkeypatch, ["--all"], failure=name)
  assert result == 9
  end = REQUESTS.index((flag, name)) + 1
  assert calls == [(owner, (None,) if option == "features" else ()) for option, owner in REQUESTS[:end]]


def test_unselected_checks_are_not_run(monkeypatch):
  assert dispatch(monkeypatch, []) == (0, [])


def test_retired_seed_gate_and_abbreviations_are_rejected(monkeypatch):
  for flag in ("--seed-reconcile", "--proven", "--signature-lay"):
    monkeypatch.setattr(sys, "argv", ["checks.py", flag])
    with pytest.raises(SystemExit) as error:
      checks.parse_args()
    assert error.value.code == 2
