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

from pathlib import Path
from typing import List, Dict
from .utils import run_cmd, yellow_print, blue_print, red_print, green_print, ProcReturn

BUILD_DIR = Path(__file__).parent.parent / 'build'
TEST_DIR = BUILD_DIR / 'test'

def list_tests() -> Dict[str, str]:
  """List all available tests in the build directory."""
  assert TEST_DIR.exists(), 'Test directory not found'
  assert TEST_DIR.is_dir(), 'Test directory is not a directory'

  proc_ret = run_cmd(['ctest', '-N'], print_stdout=False, cwd=TEST_DIR)
  assert proc_ret.retcode == 0, 'Failed to run "ctest -N"'
  
  d: Dict[str, str] = {}
  for line in proc_ret.stdout.splitlines():
    if 'Test' in line and '#' in line: 
      test_no = line.split(':')[0].strip()
      test_name = line.split(':')[1].strip()
      d[test_no] = test_name

  return d

def find_tests(keywords: List[str]) -> List[str]:
  """Find tests that match the given keywords."""
  assert TEST_DIR.exists(), 'Test directory not found'
  assert TEST_DIR.is_dir(), 'Test directory is not a directory'

  no_to_name = list_tests() # {test_no: test_name}
  name_to_no = { v: k for k, v in no_to_name.items() } # {test_name: test_no}

  def __find_test_no(keyword: str) -> List[str]:
    for test_no, test_name in no_to_name.items():
      if f'#{keyword}' in test_no:
        return [test_name]
    raise ValueError(f'No tests found for keyword: {keyword}')

  def __find_test_name(keyword: str) -> List[str]:
    return [test_name for test_name in name_to_no.keys() if keyword.lower() in test_name.lower()]

  def __do_find(keyword: str) -> List[str]:
    if keyword.strip().isnumeric(): # The test no is given.
      return __find_test_no(keyword)
    return __find_test_name(keyword)

  test_list: List[str] = []
  if len(keywords) == 0: # If no test is selected, run all tests.
    test_list.extend(name_to_no.keys())
  else: # If test names are given, locate the full test names.
    for keyword in keywords:
      found: List[str] = __do_find(keyword)
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
  verbose_level: int = 0,
  debug: bool = False,
  output_on_failure: bool = True,
) -> ProcReturn:
  """Run a single test."""
  assert TEST_DIR.exists(), 'Test directory not found'
  assert TEST_DIR.is_dir(), 'Test directory is not a directory'

  cmd: List[str] = ['ctest', '-R', f'^{test}$']
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
  keywords: List[str] = [], # When empty, run all tests.
  verbose_level: int = 0,
  debug: bool = False,
  output_on_failure: bool = True,
) -> int:
  """Find tests based on keywords and run them."""
  assert TEST_DIR.exists(), 'Test directory not found'
  assert TEST_DIR.is_dir(), 'Test directory is not a directory'

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
