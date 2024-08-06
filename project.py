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

import os
import sys
import argparse

from operator import or_
from functools import reduce
from dataclasses import dataclass

from typing import List, Callable, Sequence, Final

from automation import (
  run_cmake, build_project, clean_build,
  run_gtests, print_system_info,
  time_execution, red_print, green_print, blue_print,
  setup_environment, Tool, SetupPlan
)


BUILD_VERSION: Final[str] = '0.0.0'

def parse_args() -> argparse.Namespace:
  '''Parse the command line arguments.'''
  parser = argparse.ArgumentParser(
    description='Build and Test Automation',
    epilog=(
      'Examples of usage:\n'
      '  To print build version:\n'
      '    ./project.py --version\n\n'
      '  To set up and install dependencies:\n'
      '    ./project.py --setup\n\n'
      '  To clean the build directory:\n'
      '    ./project.py --clean\n\n'
      '  To run CMake:\n'
      '    ./project.py --cmake\n\n'
      '  To build the project using the default number of CPU cores:\n'
      '    ./project.py --build\n\n'
      '  To run tests with verbosity level 1:\n'
      '    ./project.py --test --verbosity 1\n\n'
      '  To run specific tests filtered by keywords:\n'
      '    ./project.py --test --keyword keyword1 keyword2\n\n'
      '  To build the project using 4 CPU cores:\n'
      '    ./project.py --build --cores 4\n\n'
      '  To set up, run CMake, build, and test the project in one command:\n'
      '    ./project.py -a/--all\n\n'
      '  To clean, set up, run CMake, build the project using all CPU cores, and run tests:\n'
      '    ./project.py --clean --all --cores all\n\n'
    ),
    formatter_class=argparse.RawTextHelpFormatter
  )

  available_cpu_cores: int = os.cpu_count() or 1

  def cores(value):
    '''Custom type function for parsing the --cores argument.'''
    if value == 'all':
      return available_cpu_cores
    try:
      cores = int(value)
      if cores < 1 or cores > available_cpu_cores:
        raise argparse.ArgumentTypeError(f'Invalid number of CPU cores specified: {value}. Must be between 1 and {os.cpu_count()}.')
      return cores
    except ValueError:
      raise argparse.ArgumentTypeError(f'Invalid value for --cores: {value}. Must be an integer or "all".')

  def build_type(value):
    value_lower = value.lower()
    if value_lower == 'release':
      return 'Release'
    elif value_lower == 'debug':
      return 'Debug'
    else:
      raise argparse.ArgumentTypeError(f"Invalid build type: {value}")
    
  parser.add_argument('--version', action='store_true', help='Print build version')
  parser.add_argument('--setup', action='store_true', help='Set up and install dependencies')

  parser.add_argument('--clean', action='store_true', help='Clean build')
  parser.add_argument('-cmk', '--cmake', action='store_true', help='Run CMake')
  parser.add_argument('-b', '--build', action='store_true', help='Build the project')
  parser.add_argument('-bt', '--build-type', type=build_type, default='Release', help='Build type, either "Release" or "Debug"')
  parser.add_argument('-c', '--cores', type=cores, default=max(1, available_cpu_cores // 2), help='Number of CPU cores to use for building the project (integer or "all")')
  
  parser.add_argument('-t', '--test', action='store_true', help='Run tests')
  parser.add_argument('-k', '--keyword', nargs='*', help='Keywords to filter tests', default=[])
  parser.add_argument('-v', '--verbosity', type=int, choices=[0, 1, 2], default=0, help='Verbosity level of tests')

  parser.add_argument('-a', '--all', action='store_true', help='Set up, run CMake, build, and test the project')

  args = parser.parse_args()
  if args.all:
    args.setup = True
    args.cmake = True
    args.build = True
    args.test = True

  return args


def print_steps(args: argparse.Namespace) -> None:
  '''Print the steps to be taken based on the parsed arguments.'''
  # Print the steps to be taken based on the parsed arguments
  print(60 * '#')
  blue_print('# Steps to be taken:')
  if args.setup:
    green_print('# - Set up and install dependencies')
  if args.clean:
    green_print('# - Clean build directory')
  if args.cmake:
    green_print(f'# - Run CMake | Build Type: {args.build_type}')
  if args.build:
    green_print(f'# - Build the project using {args.cores} CPU cores')
  if args.test:
    green_print(f'# - Run tests with verbosity level {args.verbosity}')
    if args.keyword:
      green_print(f'# - Filter tests with keywords: {", ".join(args.keyword)}')
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
  '''Run a task and return the result.'''
  retcode, time = time_execution(task.func, task.name)
  return TaskResult(task, time, retcode)


def run_tasks(tasks: Sequence[Task]) -> List[TaskResult]:
  '''Run a list of tasks and return the results. Early stop at the first failure.'''
  results = []
  for task in tasks:
    results.append(run_task(task))
    if results[-1].retcode != 0:
      break
  return results


def create_tasks(args: argparse.Namespace) -> List[Task]:
  '''Create a list of tasks based on the parsed arguments.'''
  tasks = []
  if args.setup:
    plan = SetupPlan(
      install_dependencies=True,
      required_tools=[Tool('cmake'), Tool('make')],
      cpp_standards=['c++2b'],
    )

    tasks.append(Task('Set up and ensure dependencies', lambda: setup_environment(plan)))
  if args.clean:
    tasks.append(Task('Clean build', clean_build))
  if args.cmake:
    tasks.append(Task('Run CMake', lambda: run_cmake(BUILD_VERSION, build_type=args.build_type, export_compile_commands=True)))
  if args.build:
    tasks.append(Task(f'Build the project using {args.cores} CPU cores', lambda: build_project(args.cores)))
  if args.test:
    tasks.append(Task('Run tests', lambda: run_gtests(args.keyword, args.verbosity)))
  return tasks


def main() -> int:
  '''Main function to parse arguments and run the appropriate automation tasks.'''

  # Parse the command line arguments
  args = parse_args()

  # If `--version` is passed, print the version and exit
  if args.version:
    print(BUILD_VERSION)
    return 0

  # Print system information
  print_system_info()

  # Print the steps to be taken
  print_steps(args)

  # Create a list of tasks based on the parsed arguments
  tasks = create_tasks(args)
  if not tasks:
    red_print('# No tasks to run')
    return 0

  # Run the tasks
  task_results = run_tasks(tasks)

  # Print the task times
  task_names = [t.task.name for t in task_results]
  max_len = max(map(len, task_names))

  task_times = [t.time for t in task_results]

  print(60 * '#')
  green_print('# Task Times:')
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
