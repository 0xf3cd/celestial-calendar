#!/usr/bin/env python3
#
# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
# 
# Author: Ningqi Wang (0xf3cd)
# Email : nq.maigre@gmail.com
# Repo  : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
# 
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import os
import argparse
from sys import exit
from typing import Dict

from automation import (
  run_cmake,
  build_project,
  clean_build,
  run_tests,
  print_system_info,
)
from automation.environment import setup_environment
from automation.utils import time_execution, red_print, green_print, blue_print

def main() -> int:
  """Main function to parse arguments and run the appropriate automation tasks."""
  parser = argparse.ArgumentParser(
    description='Build and Test Automation',
    epilog=(
      'Examples of usage:\n'
      '  To set up and install dependencies:\n'
      '    ./automation.py --setup\n\n'
      '  To clean the build directory:\n'
      '    ./automation.py --clean\n\n'
      '  To run CMake:\n'
      '    ./automation.py --cmake\n\n'
      '  To build the project using the default number of CPU cores:\n'
      '    ./automation.py --build\n\n'
      '  To run tests with verbosity level 1:\n'
      '    ./automation.py --test --verbosity 1\n\n'
      '  To run specific tests filtered by keywords:\n'
      '    ./automation.py --test --keyword keyword1 keyword2\n\n'
      '  To build the project using 4 CPU cores:\n'
      '    ./automation.py --build --cores 4\n\n'
      '  To clean, set up, run CMake, build, and test the project in one command:\n'
      '    ./automation.py --clean --setup --cmake --build --test\n\n'
      '  To clean, run CMake, and build the project in one command:\n'
      '    ./automation.py --clean --cmake --build\n\n'
      '  To set up and build the project using 8 CPU cores:\n'
      '    ./automation.py --setup --build --cores 8\n\n'
      '  To clean, set up, run CMake, build the project using all CPU cores, and run tests:\n'
      '    ./automation.py --clean --setup --cmake --build --cores $(nproc) --test\n'
    ),
    formatter_class=argparse.RawTextHelpFormatter
  )

  parser.add_argument('--setup', action='store_true', help='Set up and install dependencies')
  parser.add_argument('--clean', action='store_true', help='Clean build')
  parser.add_argument('-cmk', '--cmake', action='store_true', help='Run CMake')
  parser.add_argument('-b', '--build', action='store_true', help='Build the project')
  parser.add_argument('-t', '--test', action='store_true', help='Run tests')
  parser.add_argument('-k', '--keyword', nargs='*', help='Keywords to filter tests', default=[])
  parser.add_argument('-v', '--verbosity', type=int, choices=[0, 1, 2], default=0, help='Verbosity level of tests')
  parser.add_argument('--cores', type=int, default=max(1, os.cpu_count() // 2), help='Number of CPU cores to use for building the project')

  args = parser.parse_args()

  # Validate CPU cores
  if args.cores < 1 or args.cores > os.cpu_count():
    red_print(f'Invalid number of CPU cores specified: {args.cores}. Must be between 1 and {os.cpu_count()}.')
    return 1
  
  print_system_info()

  # Print the steps to be taken based on the parsed arguments
  blue_print('# Steps to be taken:')
  if args.setup:
    green_print('# - Set up and install dependencies')
  if args.clean:
    green_print('# - Clean build directory')
  if args.cmake:
    green_print('# - Run CMake')
  if args.build:
    green_print(f'# - Build the project using {args.cores} CPU cores')
  if args.test:
    green_print('# - Run tests')
    if args.keyword:
      green_print(f'# - Filter tests with keywords: {", ".join(args.keyword)}')
    green_print(f'# - Verbosity level: {args.verbosity}')

  print(60 * '#')

  retcode = 0
  task_times: Dict[str, float] = {}

  if args.setup:
    ret, task_times['Setup'] = time_execution(setup_environment, 'Setup and Check')
    # retcode |= ret

  if args.clean and not retcode:
    ret, task_times['Clean'] = time_execution(clean_build, 'Clean')
    retcode |= ret

  if args.cmake and not retcode:
    ret, task_times['CMake'] = time_execution(run_cmake, 'CMake')
    retcode |= ret

  if args.build and not retcode:
    ret, task_times['Build'] = time_execution(lambda: build_project(args.cores), 'Build')
    retcode |= ret

  if args.test and not retcode:
    test_args = {
      'keywords': args.keyword,
      'verbose_level': args.verbosity,
      'debug': False,
      'output_on_failure': True
    }
    ret, task_times['Test'] = time_execution(lambda: run_tests(**test_args), 'Test')
    retcode |= ret

  print(60 * '#')
  green_print(f'# Task Times:')
  for task, duration in task_times.items():
    blue_print(f'# {task:<5} time: {duration:.5f} seconds')
  print(60 * '#')

  if retcode != 0:
    red_print(f'# Return code: {retcode}')
  else:
    green_print(f'# Return code: {retcode}')

  return retcode

if __name__ == '__main__':
  exit(main())