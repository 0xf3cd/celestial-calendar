#!/usr/bin/env python3
#
# Automation script for running Python and C++ linters (ruff and clang-tidy, respectively).
#
#########################################################################################
# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
# 
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
# 
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import sys
import argparse

from automation import (
  run_ruff, run_clang_tidy, check_self_contained, probe_features,
)


def parse_args() -> argparse.Namespace:
  """Parse the command line arguments."""
  parser = argparse.ArgumentParser(
    description="Build and Test Automation",
    epilog=(
      "Examples of usage:\n"
      "  To run ruff to check Python codes:\n"
      "    ./linter.py --ruff\n\n"
      "  To run clang-tidy to check C++ codes:\n"
      "    ./linter.py --clang-tidy\n\n"
      "  To check that every header is self-contained:\n"
      "    ./linter.py --self-contained\n\n"
      "  To report which awaited C++ features this toolchain can compile:\n"
      "    ./linter.py --features\n\n"
      "  To hold a CI leg to the feature state this repo recorded for it:\n"
      "    ./linter.py --features libc++\n\n"
      "  To run every check (ruff, clang-tidy, self-containment, feature report):\n"
      "    ./linter.py -a/--all\n\n"
    ),
    formatter_class=argparse.RawTextHelpFormatter
  )

  parser.add_argument("-a", "--all", action="store_true", help="Run every check")
  parser.add_argument("--ruff", action="store_true", help="Run ruff")
  parser.add_argument("--clang-tidy", action="store_true", help="Run clang-tidy")
  parser.add_argument("--self-contained", action="store_true",
                      help="Compile every header alone to prove it is self-contained")
  parser.add_argument("--features", nargs="?", const="", default=None, metavar="LEG",
                      help="Probe the awaited C++ features; with a CI leg name, hold it to the recorded state")

  return parser.parse_args()


if __name__ == "__main__":
  args = parse_args()

  if args.ruff or args.all:
    ret_code = run_ruff()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.clang_tidy or args.all:
    ret_code = run_clang_tidy()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.self_contained or args.all:
    ret_code = check_self_contained()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.features is not None or args.all:
    # `--all` reports without judging: only a CI leg has a recorded state to be held to.
    ret_code = probe_features(args.features or None)
    if ret_code != 0:
      sys.exit(ret_code)

  sys.exit(0)
