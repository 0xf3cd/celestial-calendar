# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Run the complete installed-wheel acceptance suite."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> None:
  """Run each acceptance script with the active interpreter."""
  test_root = Path(__file__).resolve().parent
  scripts = (
    "abi/verify.py",
    "abi/raw_protocol.py",
    "consumer/smoke.py",
    "consumer/golden_replay.py",
    "consumer/events_replay.py",
    "consumer/chart_snapshot.py",
    "consumer/capabilities.py",
    "consumer/queries.py",
    "consumer/protocol_seams.py",
  )
  non_acceptance = {
    "run_all.py",
    "consumer/chart_replay.py",
    "types/consumer.py",
    "wheel/prepare.py",
    "wheel/verify.py",
    "wheel/verify_bootstrap.py",
    "wheel/verify_bootstrap_test.py",
  }
  discovered = {path.relative_to(test_root).as_posix() for path in test_root.rglob("*.py")} - non_acceptance
  assert discovered == set(scripts), f"acceptance inventory mismatch: {sorted(discovered ^ set(scripts))}"

  for script in scripts:
    result = subprocess.run(
      [sys.executable, str(test_root / script)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )
    print(result.stdout, end="", flush=True)
    result.check_returncode()
    assert any(line.startswith("PASS ") for line in result.stdout.splitlines()), f"no PASS evidence: {script}"


if __name__ == "__main__":
  main()
