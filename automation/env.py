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
import time
import shutil
import tempfile

from pathlib import Path
from pprint import pformat
from dataclasses import dataclass

from operator import not_
from itertools import product, starmap, compress

from typing import Tuple, Sequence, List

from . import paths
from .utils import (
  yellow_print, red_print, green_print, blue_print, run_cmd,
)


#region Python Dependencies

def install_dependencies() -> int:
  '''Install the required Python dependencies listed in 'Requirements.txt'.'''
  req_txt = paths.python_requirements()
  if not req_txt.exists():
    return 0

  yellow_print('# Installing Python dependencies...')
  this_python = sys.executable
  ret = run_cmd([this_python, '-m', 'pip', 'install', '-r', str(req_txt)])
  return ret.retcode

#endregion


#region Tool Existence Checks

@dataclass(frozen=True)
class Tool:
  name: str
  args: Tuple[str, ...] = ('--version',)


def check_tool(tool: Tool) -> bool:
  '''Check if a tool exists and can be executed with the given arguments.'''
  tool_path = shutil.which(tool.name)
  if tool_path is None:
    red_print(f'# {tool.name} not found!')
    return False

  result = run_cmd([tool_path] + list(tool.args), print_cmd=False, print_stdout=False, print_stderr=False)
  if result.retcode != 0:
    red_print(f'# {tool.name} does not support required arguments!')
    return False

  return True

#endregion


#region C/C++ Compiler Checks

def find_c_compilers() -> List[str]:
  '''Find the C compilers in `PATH`.'''
  c_compilers_pattern = re.compile(r'^(gcc|clang|icc)(-\d+|\d*)')

  if 'PATH' not in os.environ:
    return []

  c_compilers = []
  for path in os.environ['PATH'].split(os.pathsep):
    dir_path = Path(path)
    if not dir_path.is_dir():
      continue

    for entry in dir_path.iterdir():
      try:
        if entry.is_file() and os.access(entry, os.X_OK) and c_compilers_pattern.match(entry.name):
          c_compilers.append(entry.name)
      except PermissionError:
        pass

  return c_compilers


def find_cpp_compilers() -> List[str]:
  '''Find the C++ compilers in `PATH`.'''
  cpp_compilers_pattern = re.compile(r'^(g\+\+|clang\+\+|icpc)(-\d+|\d*)')

  if 'PATH' not in os.environ:
    return []

  cpp_compilers = []
  for path in os.environ['PATH'].split(os.pathsep):
    dir_path = Path(path)
    if not dir_path.is_dir():
      continue

    for entry in dir_path.iterdir():
      try:
        if entry.is_file() and os.access(entry, os.X_OK) and cpp_compilers_pattern.match(entry.name):
          cpp_compilers.append(entry.name)
      except PermissionError:
        pass

  return cpp_compilers


@dataclass(frozen=True)
class CompilerArgs:
  compiler: str
  standard: str


def make_compiler_args(compilers: Sequence[str], standards: Sequence[str]) -> List[CompilerArgs]:
  '''Create a list of C++ compiler arguments.'''
  compiler_args = product(compilers, standards)
  return list(starmap(CompilerArgs, compiler_args))


def check_c_support(c_args: CompilerArgs, silent: bool = False) -> bool:
  '''Check if the given compiler supports the specified C version.'''
  with tempfile.TemporaryDirectory() as tmpdir:
    tmp_c_file = Path(tmpdir) / 'test_c.c'
    tmp_c_file.write_text('''
      #include <stdio.h>

      int main() {
        printf("Hello, World!");
        return 0;
      }
    ''')

    do_print = not silent

    try:
      compiler_command = [c_args.compiler, f'-std={c_args.standard}', str(tmp_c_file), '-o', str(Path(tmpdir) / 'test_c')]
      compiler_ret = run_cmd(compiler_command, print_cmd=do_print, print_stdout=do_print, print_stderr=do_print)

      if compiler_ret.retcode != 0:
        return False

      # Execute the compiled program
      program_command = [str(Path(tmpdir) / 'test_c')]
      program_ret = run_cmd(program_command, print_cmd=do_print, print_stdout=do_print, print_stderr=do_print)
      if program_ret.retcode != 0:
        return False
      
      return True
    
    except Exception as e:
      red_print(f'# Failed to check C support: {str(e)}')
      return False


def check_cpp_support(cpp_args: CompilerArgs, silent: bool = False) -> bool:
  '''Check if the given compiler supports the specified C++ version.'''
  with tempfile.TemporaryDirectory() as tmpdir:
    tmp_cpp_file = Path(tmpdir) / 'test_cpp.cpp'
    tmp_cpp_file.write_text('''
      #include <iostream>

      int main() {
        std::cout << "Hello, World!" << std::endl;
        return 0;
      }
    ''')

    do_print = not silent

    try:
      compiler_command = [cpp_args.compiler, f'-std={cpp_args.standard}', str(tmp_cpp_file), '-o', str(Path(tmpdir) / 'test_cpp')]
      compiler_ret = run_cmd(compiler_command, print_cmd=do_print, print_stdout=do_print, print_stderr=do_print)
      if compiler_ret.retcode != 0:
        return False
      
      # Execute the compiled program
      program_command = [str(Path(tmpdir) / 'test_cpp')]
      program_ret = run_cmd(program_command, print_cmd=do_print, print_stdout=do_print, print_stderr=do_print)
      if program_ret.retcode != 0:
        return False
      
      return True
    
    except Exception as e:
      red_print(f'# Failed to check C++ support: {str(e)}')
      return False

#endregion


#region Environment Setup

@dataclass(frozen=True)
class SetupPlan:
  # Whether to install the required dependencies
  install_dependencies: bool
  # The tools to check
  required_tools: List[Tool]
  # The C++ standards to check
  cpp_standards: List[str]

def setup_environment(plan: SetupPlan) -> int:
  '''Set up the environment by installing dependencies and checking tool availability and C++ support.'''
  
  # Install dependencies
  if plan.install_dependencies:
    print(60 * '#')
    yellow_print('# - Install dependencies')

    retcode = install_dependencies()
    if retcode != 0:
      return retcode

  # Check for tools
  if len(plan.required_tools) > 0:
    print(60 * '#')
    yellow_print('# - Check tool availability')

    tool_check_results = [check_tool(t) for t in plan.required_tools]
    if not all(tool_check_results):
      unavailable = compress(plan.required_tools, map(not_, tool_check_results))
      red_print(f'Some tools are not available: {", ".join([t.name for t in unavailable])}.')
      return 1

    green_print('All tools are available.')

  # Check for C++ support
  print(60 * '#')
  yellow_print('# - C++ support check')
  assert len(plan.cpp_standards) > 0, 'No C++ standards specified.'

  cxx_env = os.environ.get('CXX')

  # If CXX not set, return error.
  if cxx_env is None:
    red_print(60 * '-')
    red_print('CXX environment variable not set. \n'
              'Please set CXX environment variable and try again.')
    red_print(60 * '-')

    time.sleep(5) # Give the user time to read the warning

    cpp_compilers = find_cpp_compilers()
    if len(cpp_compilers) != 0:
      blue_print(f'Found C++ compilers from PATH: {pformat(cpp_compilers)}')

    return 1
  
  yellow_print(60 * '-')
  yellow_print(f'# CXX environment variable set to: {cxx_env}')

  cxx_env_args = make_compiler_args([cxx_env], plan.cpp_standards)
  cxx_env_supports = [check_cpp_support(args) for args in cxx_env_args]
  if not any(cxx_env_supports):
    red_print(60 * '-')
    red_print('CXX environment variable set to an unsupported compiler. \n'
              'Building is likely to fail. Early exiting...')
    red_print(60 * '-')

    time.sleep(5) # Give the user time to read the warning
    return 1
  
  green_print('# Environment setup complete.')
  return 0

#endregion
