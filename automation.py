#!/usr/bin/env python3

import argparse
import os
from sys import exit
from typing import Dict, List
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
  parser = argparse.ArgumentParser(description='Build and Test Automation')
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

  retcode = 0
  task_times: Dict[str, float] = {}

  if args.setup:
    ret, task_times['Setup'] = time_execution(setup_environment, 'Setup and Check')
    retcode |= ret

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

  print('#' * 60)
  green_print(f'# Task Times:')
  for task, duration in task_times.items():
    blue_print(f'# {task:<5} time: {duration:.5f} seconds')
  print('#' * 60)

  if retcode != 0:
    red_print(f'# Return code: {retcode}')
  else:
    green_print(f'# Return code: {retcode}')

  return retcode


if __name__ == '__main__':
  exit(main())
