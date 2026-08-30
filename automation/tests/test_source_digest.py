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

import subprocess
import sys

from pathlib import Path

import pytest

from automation.source_digest import canonical_cpp


REPO_ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
  ("source", "expected"),
  [
    ("int x = 1; // line comment\nreturn x;", "int x = 1; return x;"),
    ("int /* block\ncomment */ x =\t1;", "int x = 1;"),
  ],
  ids=["line-comment", "block-comment-and-whitespace"],
)
def test_canonical_cpp(source, expected):
  assert canonical_cpp(source) == expected


@pytest.mark.parametrize(
  ("script", "expected"),
  [
    (
      "automation/internal_provenance.py",
      "InternalProvenanceCounts(history_tables=8, horizons_inputs=42, julian_internal_rows=7)",
    ),
    (
      "automation/nasa_delta_t_provenance.py",
      "ProvenanceCounts(runtime_branches=15, nasa_v25_rows=11, usno_v25_rows=2, inherited_v25_rows=12, "
      "v27_relations=2)",
    ),
  ],
  ids=["internal-provenance", "nasa-delta-t-provenance"],
)
def test_provenance_direct_script(script, expected):
  result = subprocess.run(
    [sys.executable, script],
    cwd=REPO_ROOT,
    check=True,
    capture_output=True,
    text=True,
  )
  assert result.stdout.strip() == expected
