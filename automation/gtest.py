# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import re

from pathlib import Path
from typing import List, Dict, Optional

from . import paths
from .utils import (
  yellow_print,
  blue_print,
  red_print,
  green_print,
  ProcReturn,
  run_cmd,
)

BUILD_DIR = paths.build_dir()
TEST_DIR = paths.cpp_test_dir()


def find_test_binaries() -> List[Path]:
  """Every test binary under the test output directory. Filter details: `paths.find_executables`."""
  return paths.find_executables(TEST_DIR)


def clear_test_binaries() -> None:
  """Unlink every test binary, for a build to re-create right after (#155).

  Tests are run by scanning `build/test/`, so a binary left behind by a test file that has since
  been renamed or deleted keeps being run forever -- and keeps passing, from a source file that
  no longer exists. It skews acceptance in the *greener* direction, the one nobody goes looking for.

  `--bench` has had this step since #133 (`build_benchmarks` unlinks before building). This is
  the same step on the test side, which never got one.
  """
  stale = find_test_binaries()
  for binary in stale:
    binary.unlink()
  if stale:
    yellow_print(f"# Cleared {len(stale)} stale test binary/binaries before building")


def parse_ctest_listing(stdout: str) -> Dict[str, str]:
  """The {test_no: test_name} mapping from a `ctest -N` listing.

  Pure, so the parsing is unit-testable without a build (#87).
  """
  d: Dict[str, str] = {}
  numbers = set()
  names = set()
  for line in stdout.splitlines():
    match = re.fullmatch(r"\s*(Test\s+#([1-9][0-9]*)):\s*(\S.*?)\s*", line)
    if match is not None:
      test_no, number, test_name = match.groups()
      if int(number) in numbers or test_name in names:
        raise ValueError(f"Duplicate test registration: {line.strip()}")
      numbers.add(int(number))
      names.add(test_name)
      d[test_no] = test_name
  return d


def _check_test_directory() -> None:
  """Require a configured test directory, including under Python optimization."""
  if not TEST_DIR.is_dir():
    raise RuntimeError(f"Test directory is missing or is not a directory: {TEST_DIR}; run configure and build first")


def list_gtests() -> Dict[str, str]:
  """List all available tests in the build directory."""
  _check_test_directory()

  proc_ret = run_cmd(["ctest", "-N"], print_stdout=False, cwd=TEST_DIR)
  if proc_ret.retcode != 0:
    raise RuntimeError(f"ctest -N failed with exit {proc_ret.retcode}: {proc_ret.stderr}")

  listing = parse_ctest_listing(proc_ret.stdout)
  totals = re.findall(r"^Total Tests:\s*([0-9]+)\s*$", proc_ret.stdout, re.MULTILINE)
  if not listing or len(totals) != 1 or int(totals[0]) != len(listing):
    raise RuntimeError("No complete test registration found; check configure, build outputs and the test directory")
  return listing


def find_gtests(keywords: List[str], listing: Optional[Dict[str, str]] = None) -> List[str]:
  """Find matching tests, reusing the caller's discovery snapshot when supplied."""
  _check_test_directory()

  no_to_name = list_gtests() if listing is None else listing
  name_to_no = {v: k for k, v in no_to_name.items()}  # {test_name: test_no}

  def __find_test_no(keyword: str) -> List[str]:
    # Match the number exactly. A substring test happens to work while the ids run without gaps,
    # but the moment one is missing it silently picks a longer id - `-k 7` lands on `#70`.
    for test_no, test_name in no_to_name.items():
      if test_no.rpartition("#")[2] == keyword.strip():
        return [test_name]
    raise ValueError(f"No tests found for keyword: {keyword}")

  def __find_test_name(keyword: str) -> List[str]:
    return [test_name for test_name in name_to_no.keys() if keyword.lower() in test_name.lower()]

  def __do_find(keyword: str) -> List[str]:
    if keyword.strip().isnumeric():  # The test no is given.
      return __find_test_no(keyword)
    return __find_test_name(keyword)

  test_list: List[str] = []
  if len(keywords) == 0:  # If no test is selected, run all tests.
    test_list.extend(name_to_no.keys())
  else:  # If test names are given, locate the full test names.
    for keyword in keywords:
      found: List[str] = __do_find(keyword)
      if len(found) == 0:
        raise ValueError(f"Did not find any test associated with keyword {keyword}")

      for full_test_name in found:
        yellow_print(f"# keyword: {keyword} => {full_test_name}")
      test_list.extend(found)

  # Remove duplicates and sort the list by test id.
  # Compared as a number, so the order does not ride on ctest right-aligning the ids - text order
  # only agrees with numeric order because the padding spaces sort below '#'.
  test_list = list(set(test_list))
  return sorted(test_list, key=lambda test_name: int(name_to_no[test_name].rpartition("#")[2]))


def run_gtest(
  test: str,
  verbose_level: int = 0,
  debug: bool = False,
  output_on_failure: bool = True,
) -> ProcReturn:
  """Run one registered case and require one actual GoogleTest execution."""
  _check_test_directory()

  # `-R` takes a regex, and every test name carries dots - escape them, or `.` matches anything.
  cmd: List[str] = ["ctest", "-R", f"^{re.escape(test)}$", "--no-tests=error", "-VV" if verbose_level == 2 else "-V"]
  if debug:
    cmd.append("--debug")
  if output_on_failure:
    cmd.append("--output-on-failure")

  show_output = verbose_level > 0 or debug
  result = run_cmd(cmd, cwd=TEST_DIR, print_stdout=show_output)
  if result.retcode != 0:
    if output_on_failure and not show_output:
      print(result.stdout, end="")
    return result

  plain_output = re.sub(r"\x1b\[[0-9;]*m", "", result.stdout)
  counts = re.findall(r"\[==========\]\s+([0-9]+) tests? from [^\n]* ran\.", plain_output)
  if counts != ["1"]:
    message = f"Test {test} did not execute exactly one GoogleTest case (reported counts: {counts})"
    red_print(message)
    return ProcReturn(1, result.stdout, result.stderr + message + "\n")
  return result


def run_gtests(
  keywords: Optional[List[str]] = None,  # When None, run all tests.
  verbose_level: int = 0,
  debug: bool = False,
  output_on_failure: bool = True,
) -> int:
  """Find tests based on keywords and run them."""
  _check_test_directory()
  listing = list_gtests()
  name_to_no = {v: k for k, v in listing.items()}
  test_list = find_gtests([] if not keywords else keywords, listing)

  print("#" * 60)
  blue_print(f"# *** {len(test_list)} tests to run ***:")
  for test in test_list:
    yellow_print(f"# test no: {name_to_no[test]}, test name: {test}")

  for test in test_list:
    print("#" * 60)
    yellow_print(f"# Running test {test} - {name_to_no[test]}")

    test_proc_ret = run_gtest(test, verbose_level, debug, output_on_failure)
    if test_proc_ret.retcode != 0:
      red_print(f"# Test {test} failed!")
      return test_proc_ret.retcode

    green_print(f"# Test {test} done!")
    print("#" * 60)

  green_print(f"# All {len(test_list)} test(s) done!")
  print("#" * 60)

  return 0
