#!/usr/bin/env python3

import os
import sys
import shlex
import shutil
import argparse
import platform
import subprocess

from pathlib import Path
from functools import partial
from enum import Enum, unique
from datetime import datetime
from dataclasses import dataclass

from typing import Final, Sequence


@unique
class Color(Enum):
  GREEN  = 1
  RED    = 2
  YELLOW = 3
  BLUE   = 4


def colored_print(cmd: str, color: Color) -> None:
  color_mapping: dict[Color, str] = {
    Color.GREEN : '\033[92m',
    Color.RED   : '\033[91m',
    Color.YELLOW: '\033[93m',
    Color.BLUE  : '\033[94m',
  }

  c: str = color_mapping[color]
  print(f'{c}{cmd}\033[0m')


green_print  = partial(colored_print, color=Color.GREEN)
red_print    = partial(colored_print, color=Color.RED)
yellow_print = partial(colored_print, color=Color.YELLOW)
blue_print   = partial(colored_print, color=Color.BLUE)


PROJ_DIR:  Final[Path] = Path(__file__).parent
BUILD_DIR: Final[Path] = PROJ_DIR / 'build'
TEST_DIR:  Final[Path] = BUILD_DIR / 'test'


@dataclass
class ProcReturn:
  retcode: int
  stdout: str
  stderr: str

def run_cmd(cmd: Sequence[str], print_stdout: bool = True, print_stderr: bool = True, **kwargs) -> ProcReturn:
  blue_print(f'# Command: {shlex.join(cmd)}')
  
  # Run a subprocess and capture its stdout and stderr.
  proc = subprocess.run(cmd, capture_output=True, env=os.environ, **kwargs)

  stdout = proc.stdout.decode('utf-8')
  stderr = proc.stderr.decode('utf-8')

  if print_stdout:
    print(stdout)
  if print_stderr:
    print(stderr)

  retcode: int = proc.returncode
  if retcode != 0:
    red_print(f'# Command failed:\n'
              f'# {shlex.join(cmd)}')
  else:
    green_print(f'# Command succeeded:\n'
                f'# {shlex.join(cmd)}')

  return ProcReturn(retcode, stdout, stderr)


def cmake() -> ProcReturn:
  print('#' * 60)

  if not BUILD_DIR.exists():
    red_print('# Build directory not found')
    yellow_print(f'# Creating {BUILD_DIR}')
    BUILD_DIR.mkdir()

  yellow_print('# Running cmake...')
  ret: ProcReturn = run_cmd(['cmake', '..'], cwd=BUILD_DIR)

  print('#' * 60)
  return ret


def make(cpu_cores: int=8) -> ProcReturn:
  print('#' * 60)

  assert BUILD_DIR.exists(), "Build directory not found"
  assert BUILD_DIR.is_dir(), "Build directory is not a directory"

  yellow_print('# Building the C++ projects...')
  retcode: ProcReturn = run_cmd(['make', '-j', str(cpu_cores)], cwd=BUILD_DIR)

  print('#' * 60)
  return retcode


def list_tests() -> dict[str, str]:
  assert TEST_DIR.exists(), "Test directory not found"
  assert TEST_DIR.is_dir(), "Test directory is not a directory"

  proc_ret = run_cmd(['ctest', '-N'], cwd=TEST_DIR)
  assert proc_ret.retcode == 0, "Failed to run 'ctest -N'"
  
  d: dict[str, str] = {}
  for line in proc_ret.stdout.splitlines():
    if 'Test' in line and '#' in line: 
      # Hope the format of the output of 'ctest -N' is not gonna change in the future...
      test_no = line.split(':')[0].strip()
      test_name = line.split(':')[1].strip()
      d[test_no] = test_name

  return d


def find_tests(keywords: list[str]) -> list[str]:
  assert TEST_DIR.exists(), "Test directory not found"
  assert TEST_DIR.is_dir(), "Test directory is not a directory"

  no_to_name = list_tests() # {test_no: test_name}
  name_to_no = { v: k for k, v in no_to_name.items() } # {test_name: test_no}

  def __find_test_no(keyword: str) -> list[str]:
    for test_no, test_name in no_to_name.items():
      if f'#{keyword}' in test_no:
        # When specifing the test no, only expecting to run 1 test.
        return [test_name]
    assert False, f'No tests found for keyword: {keyword}'

  def __find_test_name(keyword: str) -> list[str]:
    tests: list[str] = []
    for test_name in name_to_no.keys():
      if keyword.lower() in test_name.lower():
        tests.append(test_name)
    return tests

  def __do_find(keyword: str) -> list[str]:
    if keyword.strip().isnumeric(): # The test no is given.
      return __find_test_no(keyword)
    return __find_test_name(keyword)

  test_list: list[str] = []
  if len(keywords) == 0: # If no test is selected, run all tests.
    test_list.extend(name_to_no.keys())
  else: # If test names are given, locate the full test names.
    for keyword in keywords:
      found: list[str] = __do_find(keyword)
      if len(found) == 0:
        raise ValueError(f'Did not find any test associated with keyword {keyword}')

      for full_test_name in found:
        yellow_print(f'# keyword: {keyword} => {full_test_name}')
      test_list.extend(found)

  # Remove duplicates and sort the list by test id.
  test_list = list(set(test_list))
  return sorted(test_list, key=lambda test_name: name_to_no[test_name])


def run_test(
  test: str,
  verbose_level: int = 1,
  debug: bool = True,
  output_on_failure: bool = True,
) -> ProcReturn:
  '''Run a single test.'''
  assert TEST_DIR.exists(), "Test directory not found"
  assert TEST_DIR.is_dir(), "Test directory is not a directory"

  cmd: list[str] = ['ctest', '-R', f'^{test}$']
  if verbose_level == 1:
    cmd.append('-V')
  elif verbose_level == 2:
    cmd.append('-VV')
  if debug:
    cmd.append('--debug')
  if output_on_failure:
    cmd.append('--output-on-failure')

  return run_cmd(cmd, cwd=TEST_DIR)


def run_tests(
  keywords: list[str] = [], # When empty, run all tests.
  verbose_level: int = 1,
  debug: bool = True,
  output_on_failure: bool = True,
) -> int:
  '''Find tests based on keywords and run them.'''
  assert TEST_DIR.exists(), "Test directory not found"
  assert TEST_DIR.is_dir(), "Test directory is not a directory"

  name_to_no = { v: k for k, v in list_tests().items() } # {test_name: test_no}
  test_list = find_tests(keywords)
  
  print('#' * 60)
  blue_print(f'# *** {len(test_list)} tests to run ***:')
  for test in test_list:
    yellow_print(f'# test no: {name_to_no[test]}, test name: {test}')
  
  for test in test_list:
    assert test in name_to_no.keys(), f'Invalid test {test}'

    print('#' * 60)
    yellow_print(f'# Running test {test} - {name_to_no[test]}')
    
    test_proc_ret = run_test(test, verbose_level, debug, output_on_failure)
    if test_proc_ret.retcode != 0:
      red_print(f'# Test {test} failed!')
      return test_proc_ret.retcode

    green_print(f'# Test {test} done!')
    print('#' * 60)

  green_print(f'# All {len(test_list)} test(s) done!')
  print('#' * 60)

  return 0


def parse_args() -> argparse.Namespace:
  help_message: str = '''Examples:
    python3 build.py -cmk           # Run cmake only
    ./build.py -cmk -b              # Run cmake and build
    ./build.py -b -t                # Build and run all tests
    ./build.py -t -v 1              # Run all tests with verbose level 1
    ./build.py -t -k Util           # Run all Util tests
    ./build.py -t -k 26 5 7         # Run tests 26, 5, 7
    ./build.py -t -k 41 datetime    # Run test 41, and tests whose name contains "datetime"
  '''
  
  parser = argparse.ArgumentParser(description='Build and test the C++ projects', 
                                   epilog=help_message, 
                                   formatter_class=argparse.RawTextHelpFormatter)
  
  # CMake configuration.
  parser.add_argument('-cmk', '--cmake', action='store_true', default=False,
                      help='whether to run cmake before build')
  
  # Build configuration.
  cpu_count: int = os.cpu_count()
  parser.add_argument('-b', '--build', action='store_true', default=False,
                      help=f'whether to build the C++ projects, should be >= 1 and <= {cpu_count}')
  parser.add_argument('-c', '--cpu-cores', type=int, default=max(int(cpu_count/2), 1),
                      help='number of CPU cores to use for building')
  
  # Test configuration.
  parser.add_argument('-t', '--test', action='store_true', default=False,
                      help='whether to run test')
  parser.add_argument('-k','--test-keyword', nargs='+', 
                      help='list of test to run')
  parser.add_argument('-v', '--verbose', type=int, default=0,
                      help='verbose level of test, ' +
                           '0 for no output, ' + 
                           '1 for brief output, ' + 
                           '2 for detailed output')
  parser.add_argument('-d', '--debug', action='store_true', default=False,
                      help='whether to run test in debug mode')
  
  args = parser.parse_args()
  assert args.cpu_cores > 0, 'Invalid number of CPU cores, should be > 1'
  assert args.cpu_cores <= cpu_count, f'Invalid number of CPU cores, should be <= {cpu_count}'
  assert args.verbose in [0, 1, 2], f'Invalid verbose level {args.verbose}'

  return args


def print_sysinfo() -> None:
  '''Print system time and other info.'''
  cmake_version: ProcReturn = run_cmd(['cmake', '--version'], print_stdout=False)
  assert cmake_version.retcode == 0

  this_moment: datetime = datetime.now()

  print(60 * '#')
  print(f'-- system time: {this_moment.astimezone()} ({this_moment.astimezone().tzinfo})')
  print(f'-- cwd: {os.getcwd()}')

  print(f'-- default encoding: {sys.getdefaultencoding()}')
  print(f'-- python executable: {sys.executable}')
  print(f'-- python version: {sys.version}')

  print(f'-- cmake path: {shutil.which("cmake")}')
  print(f'-- cmake version: {" | ".join(
    filter(lambda s: len(s.strip()) > 0, cmake_version.stdout.splitlines())
  )}')

  print(f'-- os env CXX: {os.environ.get("CXX", None)}')
  print(f'-- os env CC: {os.environ.get("CC", None)}')
  print(f'-- clang++: {shutil.which("clang++")}')
  print(f'-- g++: {shutil.which("g++")}')

  print(f'-- node: {platform.node()}')
  print(f'-- system: {platform.system()}')
  print(f'-- platform: {platform.platform()}')
  print(f'-- release: {platform.release()}')
  print(f'-- version: {platform.version()}')
  print(f'-- machine: {platform.machine()}')
  print(f'-- processor: {platform.processor()}')
  print(f'-- architecture: {platform.architecture()}')
  print(f'-- cpu cores: {os.cpu_count()}')


def main() -> int:
  retcode: int = 0
  args = parse_args()
  print(args)
  print_sysinfo()

  cmake_start_moment = datetime.now()
  if args.cmake:
    cmake_ret = cmake()
    retcode |= cmake_ret.retcode

  build_start_moment = datetime.now()
  if args.build:
    make_ret = make(args.cpu_cores)
    retcode |= make_ret.retcode

  test_start_moment = datetime.now()
  if args.test:
    retcode |= run_tests(keywords=args.test_keyword if args.test_keyword else [], 
                         verbose_level=args.verbose,
                         debug=args.debug,
                         output_on_failure=True)

  test_end_moment = datetime.now()

  if args.cmake:
    blue_print(f'# CMake time: {build_start_moment - cmake_start_moment}')
  if args.build:
    blue_print(f'# Build time: {test_start_moment - build_start_moment}')
  if args.test:
    blue_print(f'# Test time:  {test_end_moment - test_start_moment}')
  green_print(f'# Total time: {test_end_moment - cmake_start_moment}')

  return retcode


if __name__ == "__main__":
  sys.exit(main())
