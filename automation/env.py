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
import shutil
import tempfile

from pathlib import Path
from pprint import pformat
from dataclasses import dataclass
from itertools import product, starmap

from typing import Tuple, Sequence, List

from . import paths
from .utils import (
  yellow_print, red_print, green_print, run_cmd,
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


#region C++ Compiler Checks

@dataclass(frozen=True)
class CompilerArgs:
  compiler: str
  standard: str


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


def make_compiler_args(compilers: Sequence[str], standards: Sequence[str]) -> List[CompilerArgs]:
  '''Create a list of C++ compiler arguments.'''
  compiler_args = product(compilers, standards)
  return list(starmap(CompilerArgs, compiler_args))

#endregion


#region Environment Setup

@dataclass(frozen=True)
class SetupPlan:
  # Whether to install the required dependencies
  install_dependencies: bool
  # The tools to check
  required_tools: List[Tool]
  # The C++ compilers and C++ standards to check
  cpp_compilers: List[str]
  cpp_standards: List[str]
  # Whether to check environment variable "CXX"
  check_env_cxx: bool

def setup_environment(plan: SetupPlan) -> int:
  '''Set up the environment by installing dependencies and checking tool availability and C++ support.'''
  
  # Install dependencies
  print(60 * '#')
  yellow_print('# - Set up and install dependencies')
  
  if plan.install_dependencies:
    retcode = install_dependencies()
    if retcode != 0:
      return retcode

  # Check for tools
  print(60 * '#')
  yellow_print('# - Check tool availability')

  tool_check_results = map(check_tool, plan.required_tools)
  tool_availability = dict(zip(plan.required_tools, tool_check_results))
  if not all(tool_availability.values()):
    red_print(f'Some tools are not available: {", ".join(tool.name for tool in tool_availability.keys())}.')
    return 1

  green_print('All tools are available.')

  # Check for C++ support
  print(60 * '#')
  yellow_print('# - C++ support check')

  assert len(plan.cpp_compilers) > 0
  assert len(plan.cpp_standards) > 0
  cpp_compilers = plan.cpp_compilers.copy()

  compiler_args = make_compiler_args(cpp_compilers, plan.cpp_standards)
  check_returns = map(check_cpp_support, compiler_args)
  compiler_support = dict(zip(compiler_args, check_returns))
  
  yellow_print(60 * '-')
  yellow_print('# - C++ support check:')
  for args, support in compiler_support.items():
    compiler_path = shutil.which(args.compiler)
    if support:
      green_print(f'# {compiler_path} supports {args.standard}')
    else:
      if compiler_path:
        red_print(f'# {compiler_path} does not support {args.standard}')
      else:
        red_print(f'# {args.compiler} not found!')
  
  if not any(compiler_support.values()):
    red_print('None of the compilers support the specified C++ features.')
    red_print(f'Checked: {pformat(compiler_support)}')
    return 1
  
  if not plan.check_env_cxx:
    return 0
  
  # Otherwise, check environment variable "CXX"
  yellow_print(60 * '-')
  yellow_print('# - CXX environment variable check:')

  cxx_env = os.environ.get('CXX')
  if cxx_env:
    yellow_print(f'# CXX environment variable set to: {cxx_env}')
  else:
    yellow_print(60 * '-')
    yellow_print('CXX environment variable not set. \n'
                 'CXX is strongly recommended to be set to a compiler with C++23 support.')
    yellow_print(60 * '-')
    return 0

  cxx_env_args = make_compiler_args([cxx_env], plan.cpp_standards)
  check_returns = map(check_cpp_support, cxx_env_args)
  if not any(check_returns):
    red_print(60 * '-')
    red_print('CXX environment variable set to an unsupported compiler. \n'
              'Building is likely to fail.')
    red_print(60 * '-')
    return 1
  
  green_print('# Environment setup complete.')
  return 0

#endregion
