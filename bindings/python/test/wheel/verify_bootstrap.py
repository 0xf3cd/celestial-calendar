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

import hashlib
import re
import sys
import tomllib
import zipfile
from pathlib import Path, PurePosixPath

from packaging.utils import canonicalize_name, parse_wheel_filename

from cibuildwheel.util import resources
from cibuildwheel.venv import _ensure_virtualenv, _parse_pip_constraint_for_virtualenv


def main():
  selector = Path(sys.argv[1]).resolve()
  selector_lines = [
    line for line in selector.read_text(encoding="utf-8").splitlines() if line and not line.startswith("#")
  ]
  lock = selector.parent / selector_lines[0].removeprefix("-c ")
  selected_pip = _parse_pip_constraint_for_virtualenv(selector)
  if selected_pip == "embed":
    raise RuntimeError(f"Cibuildwheel cannot read the pip pin in {selector}")

  with resources.VIRTUALENV.open("rb") as source:
    configs = tomllib.load(source)
  expected = configs.get("py311", configs["default"])
  pyz, version = _ensure_virtualenv("3.11")
  digest = hashlib.sha256(pyz.read_bytes()).hexdigest()
  if str(version) != expected["version"] or digest != expected["sha256"]:
    raise RuntimeError(f"Cibuildwheel bootstrap virtualenv does not match {resources.VIRTUALENV}")

  with zipfile.ZipFile(pyz) as archive:
    embedded_pip = {
      str(wheel_version): hashlib.sha256(archive.read(member)).hexdigest()
      for member in archive.namelist()
      if member.startswith("virtualenv/seed/wheels/embed/pip-")
      for name, wheel_version, _build, _tags in [parse_wheel_filename(PurePosixPath(member).name)]
      if canonicalize_name(name) == "pip"
    }
  if selected_pip not in embedded_pip:
    raise RuntimeError(f"pip {selected_pip} is not embedded in {pyz.name}: {sorted(embedded_pip)}")

  lock_text = lock.read_text(encoding="utf-8")
  start = re.search(rf"(?m)^pip=={re.escape(selected_pip)} \\$", lock_text)
  if start is None:
    raise RuntimeError(f"pip {selected_pip} is not pinned in {lock}")
  end = re.search(r"(?m)^[A-Za-z0-9][A-Za-z0-9_.-]*==", lock_text[start.end() :])
  block_end = start.end() + end.start() if end else len(lock_text)
  lock_hashes = set(re.findall(r"--hash=sha256:([0-9a-f]{64})", lock_text[start.start() : block_end]))
  if embedded_pip[selected_pip] not in lock_hashes:
    raise RuntimeError(f"The embedded pip {selected_pip} wheel is not hash-locked in {lock}")

  print(f"PASS cibuildwheel bootstrap uses hash-locked embedded pip {selected_pip} from {pyz.name}")


if __name__ == "__main__":
  main()
