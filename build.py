#! /usr/bin/env python3

import os
import argparse
import subprocess
import multiprocessing

green_print = lambda s: print(f'\033[92m{s}\033[0m')
red_print = lambda s: print(f'\033[91m{s}\033[0m')
yellow_print = lambda s: print(f'\033[93m{s}\033[0m')
blue_print = lambda s: print(f'\033[94m{s}\033[0m')

def cmd_print(cmd: str) -> None:
  blue_print('# Command:')
  for sub_cmd in cmd.split('&&'):
    sub_cmd = sub_cmd.strip()
    blue_print(f'# - {sub_cmd}')


PROJ_DIR: str = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR: str = os.path.join(PROJ_DIR, "build")
TEST_DIR: str = os.path.join(BUILD_DIR, "test")


def cmake() -> None:
  print('#' * 60)

  if not os.path.exists(BUILD_DIR):
    red_print('# Build directory not found')
    yellow_print(f'# Creating {BUILD_DIR}')
    os.mkdir(BUILD_DIR)

  cmd: str = f'cd {BUILD_DIR} && cmake ..'

  yellow_print('# Running cmake...')
  cmd_print(cmd)
  subprocess.run(cmd, shell=True).check_returncode()
  green_print('# cmake done!')
  print('#' * 60)


def make(cpu_cores: int=8) -> None:
  print('#' * 60)
  assert os.path.exists(BUILD_DIR), "Build directory not found"

  cmd: str = f'cd {BUILD_DIR} && make -j{cpu_cores}'

  yellow_print('# Building the C++ projects...')
  cmd_print(cmd)
  subprocess.run(cmd, shell=True).check_returncode()

  green_print('# make done!')
  print('#' * 60)


def list_test() -> dict[str, str]:
  assert os.path.exists(TEST_DIR), "Test directory not found"
  cmd: str = f'cd {TEST_DIR} && ctest -N'
  output: str = subprocess.run(cmd, shell=True, capture_output=True).stdout.decode('utf-8')
  
  d = dict()
  for line in output.splitlines():
    if 'Test' in line and '#' in line: 
      # Hope the format of output of 'ctest -N' not gonna change in the future...
      test_no = line.split(':')[0].strip()
      test_name = line.split(':')[1].strip()
      d[test_no] = test_name
  return d


def run_tests(
  given_tests: list[str] = [], # When empty, run all tests.
  verbose_level: int = 1,
  debug: bool = True,
  output_on_failure: bool = True,
) -> None:
  assert os.path.exists(TEST_DIR), "Test directory not found"

  def locate_test(given_name: str) -> list[str]:
    if given_name.strip().isnumeric(): # The test no is given.
      inferred_test_no = f'#{given_name}'
      for test_no in available_tests.keys():
        if inferred_test_no in test_no:
          # When specifing the test no, only expecting to run 1 test.
          return [available_tests[test_no]]

    located_tests = []
    for full_test_name in available_tests.values():
      if given_name.lower() in full_test_name.lower():
        located_tests.append(full_test_name)
    
    return located_tests
  
  available_tests = list_test() # {test_no: test_name}
  inverse_available_tests = {v: k for k, v in available_tests.items()} # {test_name: test_no}

  test_list: list[str] = []
  if len(given_tests) == 0: # If no test is given, run all tests.
    test_list = test_list + list(available_tests.values())
  else: # If test names are given, locate the full test names.
    for given_test_name in given_tests:
      full_test_names: list[str] = locate_test(given_test_name)
      for full_test_name in full_test_names:
        yellow_print(f'# keyword: {given_test_name} => {full_test_name}')
      if len(full_test_names) == 0:
        raise ValueError(f'Did not find any test associated with name {given_test_name}')
      test_list = test_list + full_test_names
  
  # Remove duplicates and sort the list by test id.
  test_list = list(set(test_list))
  test_list.sort(key=lambda test_name: inverse_available_tests[test_name])

  def make_cmd(test: str) -> str:
    cmd: str = f'cd {TEST_DIR} && ctest -R ^{test}$'
    if verbose_level == 1:
      cmd += ' -V'
    elif verbose_level == 2:
      cmd += ' -VV'
    if debug: cmd += ' --debug'
    if output_on_failure: cmd += ' --output-on-failure'
    return cmd
  
  print('#' * 60)
  blue_print(f'# *** {len(test_list)} tests to run ***:')
  for test in test_list:
    yellow_print(f'# test no: {inverse_available_tests[test]}, test name: {test}')
  
  for test in test_list:
    assert test in available_tests.values(), f'Invalid test {test}'
    cmd = make_cmd(test)

    print('#' * 60)
    yellow_print(f'# Running test {test} - {inverse_available_tests[test]}')
    cmd_print(cmd)
    subprocess.run(cmd, shell=True).check_returncode()
    green_print(f'# Test {test} done!')
    print('#' * 60)

  green_print(f'# All {len(test_list)} test(s) done!')
  print('#' * 60)


def parse_args() -> argparse.Namespace:
  cpu_count: int = multiprocessing.cpu_count()
  help_message: str = '''Examples:
    python3 build.py -cmk           # Run cmake only
    ./build.py -cmk -b              # Run cmake and build
    ./build.py -b -t                # Build and run all tests
    ./build.py -t -tv 1             # Run all tests with verbose level 1
    ./build.py -t -tl Util          # Run all Util tests
    ./build.py -t -tl 26 5 7        # Run tests 26, 5, 7
    ./build.py -t -tl 41 linkedlist # Run test 41, and tests whose name contains "linkedlist"
  '''
  
  parser = argparse.ArgumentParser(description='Build and test the C++ projects', 
                                   epilog=help_message, 
                                   formatter_class=argparse.RawTextHelpFormatter)
  
  # CMake configuration.
  parser.add_argument('-cmk', '--cmake', action='store_true', default=False,
                      help='whether to run cmake before build')
  
  # Build configuration.
  parser.add_argument('-b', '--build', action='store_true', default=False,
                      help=f'whether to build the C++ projects, should be >= 1 and <= {cpu_count}')
  parser.add_argument('-c', '--cpu-cores', type=int, default=max(int(cpu_count/2), 1),
                      help='number of CPU cores to use for building')
  
  # Test configuration.
  parser.add_argument('-t', '--test', action='store_true', default=False,
                      help='whether to run test')
  parser.add_argument('-tl','--test-list', nargs='+', 
                      help='list of test to run')
  parser.add_argument('-tv', '--test-verbose', type=int, default=0,
                      help='verbose level of test, ' +
                           '0 for no output, ' + 
                           '1 for brief output, ' + 
                           '2 for detailed output')
  parser.add_argument('-td', '--test-debug', action='store_true', default=False,
                      help='whether to run test in debug mode')
  
  args = parser.parse_args()
  assert args.cpu_cores > 0, 'Invalid number of CPU cores, should be > 1'
  assert args.cpu_cores <= cpu_count, f'Invalid number of CPU cores, should be <= {cpu_count}'
  assert args.test_verbose in [0, 1, 2], f'Invalid verbose level {args.test_verbose_level}'

  return args


def run() -> None:
  args = parse_args()
  print(args)

  if args.cmake:
    cmake()
  
  if args.build:
    make(args.cpu_cores)

  if args.test:
    run_tests(given_tests=args.test_list if args.test_list else [], 
              verbose_level=args.test_verbose,
              debug=args.test_debug,
              output_on_failure=True)


if __name__ == "__main__":
  run()
