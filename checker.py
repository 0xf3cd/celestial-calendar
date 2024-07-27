#!/usr/bin/env python3
#
# Automation script for linting the CelestialCalendar C++ project.
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
  run_ruff, run_clang_tidy
)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(
    description='Linter Automation',
    epilog=(
      'Examples of usage:\n'
      '  To run ruff:\n'
      '    ./checker.py --ruff\n\n'
      '  To run clang-tidy:\n'
      '    ./checker.py --clang-tidy\n\n'
    )
  )
  parser.add_argument(
    '--ruff', action='store_true', help='Run ruff on the project source code.'
  )
  parser.add_argument(
    '--clang-tidy', action='store_true', help='Run clang-tidy on the project CPP source code.'
  )

  args = parser.parse_args()

  retcode = 0
  if args.ruff:
    retcode |= run_ruff()
  if args.clang_tidy:
    retcode |= run_clang_tidy()

  sys.exit(retcode)
