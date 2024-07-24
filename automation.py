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
import sys
import argparse

from operator import or_
from functools import reduce
from dataclasses import dataclass

from typing import List, Callable, Sequence

from automation import (
  run_cmake,
  build_project,
  clean_build,
  run_tests,
  print_system_info,
)
from automation.environment import setup_environment
from automation.utils import time_execution, red_print, green_print, blue_print


def parse_args() -> argparse.Namespace:
  """Parse the command line arguments."""
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
      '    ./automation.py --clean --setup --cmake --build --cores all --test\n'
    ),
    formatter_class=argparse.RawTextHelpFormatter
  )

  def parse_cores(value):
    """Custom type function for parsing the --cores argument."""
    if value == 'all':
      return os.cpu_count()
    try:
      cores = int(value)
      if cores < 1 or cores > os.cpu_count():
        raise argparse.ArgumentTypeError(f'Invalid number of CPU cores specified: {value}. Must be between 1 and {os.cpu_count()}.')
      return cores
    except ValueError:
      raise argparse.ArgumentTypeError(f'Invalid value for --cores: {value}. Must be an integer or "all".')

  parser.add_argument('--setup', action='store_true', help='Set up and install dependencies')
  parser.add_argument('--clean', action='store_true', help='Clean build')
  parser.add_argument('-cmk', '--cmake', action='store_true', help='Run CMake')
  parser.add_argument('-b', '--build', action='store_true', help='Build the project')
  parser.add_argument('-t', '--test', action='store_true', help='Run tests')
  parser.add_argument('-k', '--keyword', nargs='*', help='Keywords to filter tests', default=[])
  parser.add_argument('-v', '--verbosity', type=int, choices=[0, 1, 2], default=0, help='Verbosity level of tests')
  parser.add_argument('--cores', type=parse_cores, default=max(1, os.cpu_count() // 2), help='Number of CPU cores to use for building the project (integer or "all")')

  return parser.parse_args()


def print_steps(args: argparse.Namespace) -> None:
  """Print the steps to be taken based on the parsed arguments."""
  # Print the steps to be taken based on the parsed arguments
  print(60 * '#')
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


@dataclass
class Task:
  name: str
  func: Callable[[], int]


@dataclass
class TaskResult:
  task:    Task
  time:    float
  retcode: int


def run_task(task: Task) -> TaskResult:
  """Run a task and return the result."""
  retcode, time = time_execution(task.func, task.name)
  return TaskResult(task, time, retcode)


def run_tasks(tasks: Sequence[Task]) -> List[TaskResult]:
  """Run a list of tasks and return the results. Early stop at the first failure."""
  results = []
  for task in tasks:
    results.append(run_task(task))
    if results[-1].retcode != 0:
      break
  return results


def build_tasks(args: argparse.Namespace) -> List[Task]:
  """Build a list of tasks based on the parsed arguments."""
  tasks = []
  if args.setup:
    tasks.append(Task('Set up and ensure dependencies', setup_environment))
  if args.clean:
    tasks.append(Task('Clean build', clean_build))
  if args.cmake:
    tasks.append(Task('Run CMake', run_cmake))
  if args.build:
    tasks.append(Task(f'Build the project using {args.cores} CPU cores', lambda: build_project(args.cores)))
  if args.test:
    tasks.append(Task('Run tests', lambda: run_tests(args.keyword, args.verbosity)))
  return tasks


def main() -> int:
  """Main function to parse arguments and run the appropriate automation tasks."""

  # Parse the command line arguments
  args = parse_args()
  
  # Print system information
  print_system_info()

  # Print the steps to be taken
  print_steps(args)

  # Build a list of tasks based on the parsed arguments
  tasks = build_tasks(args)
  if not tasks:
    red_print('# No tasks to run')
    return 0

  # Run the tasks
  task_results = run_tasks(tasks)

  # Print the task times
  task_names = [t.task.name for t in task_results]
  task_times = [t.time for t in task_results]
  max_len = max(map(len, task_names))

  print(60 * '#')
  green_print(f'# Task Times:')
  for task_name, duration in zip(task_names, task_times):
    blue_print(f'# {task_name:<{max_len}} time: {duration:.5f} seconds')
  print(60 * '#')

  # Resolve the return code
  retcode = reduce(or_, [t.retcode for t in task_results])

  if retcode != 0:
    red_print(f'# Return code: {retcode}')
  else:
    green_print(f'# Return code: {retcode}')

  return retcode


if __name__ == '__main__':
  sys.exit(main())
