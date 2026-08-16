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

"""Require one wheel, write its exact SHA-256 sidecar, and expose both paths to Actions."""

from __future__ import annotations

import hashlib
import sys
from pathlib import Path


def main() -> None:
  """Prepare one wheelhouse for artifact upload."""
  assert len(sys.argv) == 3, "usage: prepare.py <wheelhouse> <github-output>"
  wheelhouse = Path(sys.argv[1]).resolve()
  wheels = list(wheelhouse.glob("*.whl"))
  assert len(wheels) == 1, f"expected one wheel in {wheelhouse}, found {len(wheels)}"
  wheel = wheels[0]
  sidecar = wheel.with_name(f"{wheel.name}.sha256")
  assert not sidecar.exists(), f"refusing to overwrite {sidecar}"
  digest = hashlib.sha256(wheel.read_bytes()).hexdigest()
  sidecar.write_text(f"{digest}  {wheel.name}\n", encoding="ascii", newline="\n")

  github_output = Path(sys.argv[2])
  with github_output.open("a", encoding="utf-8", newline="\n") as output:
    output.write(f"wheel={wheel}\n")
    output.write(f"sidecar={sidecar}\n")
    output.write(f"filename={wheel.name}\n")
  print(f"PASS prepared {wheel.name} with SHA-256 {digest}")


if __name__ == "__main__":
  main()
