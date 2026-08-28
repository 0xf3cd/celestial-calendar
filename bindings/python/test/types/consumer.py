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

"""Verify that mypy consumes the clean-installed package's inline annotations."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

import celestial_calendar


def run_mypy(source: Path) -> subprocess.CompletedProcess[str]:
  """Run the pinned type checker without reusing cached import metadata."""
  return subprocess.run(
    [sys.executable, "-m", "mypy", "--strict", "--no-incremental", "--show-error-codes", str(source)],
    cwd=source.parent,
    text=True,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    check=False,
  )


def main() -> None:
  """Exercise typed, mistyped, and missing-marker consumers."""
  package = Path(celestial_calendar.__file__).resolve().parent
  marker = package / "py.typed"
  hidden_marker = package / "py.typed.hidden"
  assert marker.read_bytes() == b""
  assert not hidden_marker.exists()

  with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    positive = root / "positive.py"
    positive.write_text(
      "from celestial_calendar import Jieqi, JieqiMoment, jieqi_moment\n"
      "moment: JieqiMoment = jieqi_moment(2026, Jieqi.LICHUN)\n",
      encoding="utf-8",
    )
    result = run_mypy(positive)
    assert result.returncode == 0, result.stdout

    negative = root / "negative.py"
    negative.write_text(
      "from celestial_calendar import Jieqi, jieqi_moment\nmoment: str = jieqi_moment(2026, Jieqi.LICHUN)\n",
      encoding="utf-8",
    )
    result = run_mypy(negative)
    assert result.returncode == 1, result.stdout
    assert 'expression has type "JieqiMoment", variable has type "str"' in result.stdout
    assert "[assignment]" in result.stdout

    marker.replace(hidden_marker)
    try:
      result = run_mypy(positive)
    finally:
      hidden_marker.replace(marker)
    assert result.returncode == 1, result.stdout
    assert 'Skipping analyzing "celestial_calendar"' in result.stdout
    assert "[import-untyped]" in result.stdout


if __name__ == "__main__":
  main()
