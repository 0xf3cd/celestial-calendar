# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import subprocess
import sys
from pathlib import Path

import pytest

from toolbox.release_validation import validate_release_source


SHA = "a" * 40


@pytest.mark.parametrize(
  "tag,commit,version",
  [
    ("main", SHA, "1.2.0"),
    ("v1.2.1", SHA, "1.2.0"),
    ("v1.2.0\n", SHA, "1.2.0"),
    ("v1.2.0", "main", "1.2.0"),
  ],
)
def test_release_source_rejects_malformed_or_mismatched_identity_before_git(
  tmp_path, monkeypatch, tag, commit, version
):
  def forbidden(*args, **kwargs):
    pytest.fail("malformed release identity reached git")

  monkeypatch.setattr(subprocess, "run", forbidden)
  with pytest.raises(ValueError):
    validate_release_source(tmp_path, tag, commit, version)


@pytest.mark.parametrize("ancestor", [True, False])
def test_release_source_uses_exact_git_ancestry_and_propagates_failure(tmp_path, monkeypatch, ancestor):
  calls = []

  def run(command, **kwargs):
    calls.append((command, kwargs))
    if not ancestor:
      raise subprocess.CalledProcessError(1, command)

  monkeypatch.setattr(subprocess, "run", run)
  if ancestor:
    validate_release_source(tmp_path, "v1.2.0", SHA, "1.2.0")
  else:
    with pytest.raises(subprocess.CalledProcessError):
      validate_release_source(tmp_path, "v1.2.0", SHA, "1.2.0")
  assert calls == [
    (
      ["git", "merge-base", "--is-ancestor", SHA, "origin/main"],
      {"cwd": tmp_path, "check": True, "capture_output": True, "text": True},
    )
  ]


def test_release_source_can_check_real_fetched_history_without_third_party_dependencies(tmp_path):
  root = Path(__file__).resolve().parents[2]
  clone = tmp_path / "source"
  subprocess.run(["git", "clone", "--shared", "--no-checkout", str(root), str(clone)], check=True, capture_output=True)
  commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
  subprocess.run(["git", "update-ref", "refs/remotes/origin/main", commit], cwd=clone, check=True)
  subprocess.run(
    [
      sys.executable,
      "-S",
      "-c",
      "import sys; from pathlib import Path; "
      "from toolbox.release_validation import validate_release_source; "
      "validate_release_source(Path(sys.argv[1]), 'v1.2.0', sys.argv[2], '1.2.0')",
      str(clone),
      commit,
    ],
    cwd=root,
    check=True,
    capture_output=True,
    text=True,
  )
