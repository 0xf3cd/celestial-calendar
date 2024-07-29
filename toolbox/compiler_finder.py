#!/usr/bin/env python3
#
# Helper to check the satisfying compilers for C and C++
#
#########################################################################################
#
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
import re
import sys
import shutil
import argparse

from pathlib import Path
from typing import Dict, List

# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import (
  check_c_support, check_cpp_support, CompilerArgs, make_compiler_args,
)


def c_compilers(standards: List[str]) -> Dict[CompilerArgs, bool]:
  '''Find the C compilers in `PATH`, and find out if they support the given standards.'''
  c_compilers_pattern = re.compile(r'^(gcc|clang|icc)(\d*)$')
  compilers = set()

  for path in os.environ["PATH"].split(os.pathsep):
    dir_path = Path(path)
    if dir_path.is_dir():
      for entry in dir_path.iterdir():
        try:
          if entry.is_file() and os.access(entry, os.X_OK) and c_compilers_pattern.match(entry.name):
            compilers.add(entry.name)
        except PermissionError:
          pass

  args = make_compiler_args(list(compilers), standards)
  results = map(lambda a: check_c_support(a, silent=True), args)
  return dict(zip(args, results))


def cpp_compilers(standards: List[str]) -> Dict[CompilerArgs, bool]:
  '''Find the C++ compilers in `PATH`, and find out if they support the given standards.'''
  cpp_compilers_pattern = re.compile(r'^(g\+\+|clang\+\+|icpc)(\d*)$')
  compilers = set()

  for path in os.environ["PATH"].split(os.pathsep):
    dir_path = Path(path)
    if dir_path.is_dir():
      for entry in dir_path.iterdir():
        try:
          if entry.is_file() and os.access(entry, os.X_OK) and cpp_compilers_pattern.match(entry.name):
            compilers.add(entry.name)
        except PermissionError:
          pass

  args = make_compiler_args(list(compilers), standards)
  results = map(lambda a: check_cpp_support(a, silent=True), args)
  return dict(zip(args, results))


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
    description='Find the C and C++ compilers in `PATH`',
    epilog=(
      'Examples of usage:\n'
      '  To find C compilers that support the C11 standard:\n'
      '    compiler_finder.py -c c11\n\n'
      '  To find C++ compilers that support the c++17 and c++20 standard:\n'
      '    compiler_finder.py -cpp c++17 c++20\n\n'
    ),
    formatter_class=argparse.RawTextHelpFormatter
  )
  parser.add_argument('-cpp', '--cpp', type=str, help='The C++ standards to check')
  parser.add_argument('-c', '--c', type=str, help='The C standards to check')
  parser.add_argument('--full-path', action='store_true', help='Print the full path of the compilers')
  args = parser.parse_args()

  # Ensure either `-c` or `-cpp` is specified, and only 1 is specified.
  find_c: bool = args.c is not None
  find_cpp: bool = args.cpp is not None

  if not find_c and not find_cpp:
    parser.error('Either `-c` or `-cpp` must be specified')
  if find_c and find_cpp:
    parser.error('Only one of `-c` or `-cpp` can be specified at a time')

  standard = args.c if find_c else args.cpp
  finder = c_compilers if find_c else cpp_compilers

  results = finder([standard])
  compilers = [k.compiler for k, v in results.items() if v]

  if args.full_path:
    compilers = [shutil.which(c) for c in compilers]

  compilers = [c for c in compilers if c]

  print('\n'.join(compilers))
