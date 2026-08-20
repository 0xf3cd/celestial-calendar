#!/usr/bin/env python3
#
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

import argparse
import sys

from pathlib import Path

sys.path.append(str(Path(__file__).parent.parent))

from toolbox.release_validation import RELEASE_DOCUMENTS, stage_release_candidate


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description="Stage one immutable registry and GitHub Release candidate.")
  parser.add_argument("--release-assets", type=Path, required=True)
  parser.add_argument("--source-manifest", type=Path, required=True)
  parser.add_argument("--save-to", type=Path, required=True)
  parser.add_argument("--tag", required=True)
  parser.add_argument("--commit", required=True)
  parser.add_argument("--release-notes", type=Path, default=RELEASE_DOCUMENTS[0])
  return parser.parse_args()


def main() -> None:
  args = parse_args()
  manifest = stage_release_candidate(
    args.release_assets.resolve(),
    args.source_manifest.resolve(),
    args.save_to.resolve(),
    args.tag,
    args.commit,
    args.release_notes.resolve(),
  )
  print(manifest)


if __name__ == "__main__":
  main()
