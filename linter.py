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

#!/usr/bin/env python3
#
# Automation script for building and testing the CelestialCalendar C++ project.
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
  run_ruff, run_clang_tidy,
)


def parse_args() -> argparse.Namespace:
  '''Parse the command line arguments.'''
  parser = argparse.ArgumentParser(
    description='Build and Test Automation',
    epilog=(
      'Examples of usage:\n'
      '  To run ruff to check Python codes:\n'
      '    ./linter.py --ruff\n\n'
      '  To run clang-tidy to check C++ codes:\n'
      '    ./linter.py --clang-tidy\n\n'
      '  To run both linters:\n'
      '    ./linter.py -a/--all\n\n'
    ),
    formatter_class=argparse.RawTextHelpFormatter
  )

  parser.add_argument('-a', '--all', action='store_true', help='Clean the project')
  parser.add_argument('--ruff', action='store_true', help='Run ruff')
  parser.add_argument('--clang-tidy', action='store_true', help='Run clang-tidy')

  return parser.parse_args()


if __name__ == '__main__':
  args = parse_args()

  if args.ruff or args.all:
    ret_code = run_ruff()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.clang_tidy or args.all:
    ret_code = run_clang_tidy()
    if ret_code != 0:
      sys.exit(ret_code)

  sys.exit(0)
