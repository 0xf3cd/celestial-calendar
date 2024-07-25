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
import shutil
import tempfile

from pprint import pformat
from pathlib import Path
from itertools import product, starmap

from typing import Tuple, Dict, List

from .utils import (
  yellow_print, red_print, green_print, run_cmd,
)

REQUIREMENTS_FILE = Path(__file__).parent.parent / 'Requirements.txt'


def install_dependencies() -> int:
  """Install the required Python dependencies listed in 'Requirements.txt'."""
  if not REQUIREMENTS_FILE.exists():
    return 0

  yellow_print('# Installing Python dependencies...')
  pip_executable = shutil.which('pip')
  if pip_executable is None:
    pip_executable = shutil.which('pip3')

  if not pip_executable:
    red_print('# pip not found!')
    return 1

  ret = run_cmd([pip_executable, 'install', '-r', str(REQUIREMENTS_FILE)])
  return ret.retcode


def check_tool_exists(name: str, args: Tuple[str, ...]) -> bool:
  """Check if a tool exists and can be executed with the given arguments."""
  tool_path = shutil.which(name)
  if tool_path is None:
    red_print(f'# {name} not found!')
    return False

  result = run_cmd([tool_path] + list(args), print_cmd=False, print_stdout=False, print_stderr=False)
  if result.retcode != 0:
    red_print(f'# {name} does not support required features!')
    return False

  return True


def check_cpp_support(compiler: str, cpp_version: str) -> bool:
  """Check if the given compiler supports the specified C++ version."""
  with tempfile.TemporaryDirectory() as tmpdir:
    tmp_cpp_file = Path(tmpdir) / 'test_cpp.cpp'
    tmp_cpp_file.write_text('''
      #include <print>
      #include <format>
      #include <vector>
      #include <ranges>
      #include <numeric>
                            
      template <typename T>
      concept Numeric = std::integral<T> || std::floating_point<T>;
                            
      auto stringify(const Numeric auto& x) -> std::string_view {
        return std::format("{}", x);                
      }

      int main() {
        std::println("Hello, C++23!");
        
        const std::vector<int> v { 1, 2, 3, 4, 5 };
        const auto sum = std::reduce(std::cbegin(v), std::cend(v));
                            
        const auto square = [](int x) { return x * x; };
        const auto squared = v | std::views::transform(square);
        const auto squared_sum = std::reduce(std::cbegin(squared), std::cend(squared));

        std::println("Sum: {}", sum);
        std::println("Sum of squares: {}", stringify(squared_sum));

        return 0;
      }
    ''')

    try:
      cxx_test_command = [compiler, f'--std={cpp_version}', str(tmp_cpp_file), '-o', str(Path(tmpdir) / 'test_cpp')]
      proc_ret = run_cmd(cxx_test_command)
      return proc_ret.retcode == 0
    
    except Exception as e:
      red_print(f'# Failed to check C++ support: {str(e)}')
      return False


def setup_environment() -> int:
  """Set up the environment by installing dependencies and checking tool availability and C++ support."""
  
  # Install dependencies
  retcode = install_dependencies()
  if retcode != 0:
    return retcode

  # Check compiler availability
  tools = [
    ('cmake', ('--version',)),
    ('make',  ('--version',)),
  ]

  tool_check_results = starmap(check_tool_exists, tools)
  tool_availability = dict(zip(tools, tool_check_results))

  if not all(tool_availability.values()):
    red_print(f'Some tools are not available: {", ".join(tool_availability.keys())}.')
    return 1

  green_print('All tools are available.')

  # Check for C++ support
  cxx_standards = ['c++2b'] # Currently CMakeLists.txt is using c++2b
  cxx_compilers = ['clang-cl', 'clang++-18', 'clang++', 'g++-14', 'g++']
  cxx_env = os.environ.get('CXX')
  if cxx_env:
    yellow_print(f'# CXX environment variable set to: {cxx_env}')
    cxx_compilers.append(cxx_env)
  else:
    yellow_print('# CXX environment variable not set.')

  compiler_args = tuple(product(set(cxx_compilers), cxx_standards))
  check_returns = starmap(check_cpp_support, compiler_args)

  compiler_support = dict(zip(compiler_args, check_returns))

  if not any(compiler_support.values()):
    red_print('None of the compilers support the specified C++ features.')
    red_print(f'Checked: {pformat(compiler_support)}')
    return 1
  
  yellow_print(60 * '-')
  yellow_print('C++ support check:')
  for args, support in compiler_support.items():
    compiler_path = shutil.which(args[0])
    if support:
      green_print(f'# {compiler_path} supports {args[1]}')
    else:
      if compiler_path:
        red_print(f'# {compiler_path} does not support {args[1]}')
      else:
        red_print(f'# {args[0]} not found!')

  ok_compilers = set(map(lambda args: args[0], compiler_support.keys()))
  if cxx_env and (cxx_env not in ok_compilers):
    red_print(60 * '-')
    red_print('CXX environment variable set to an unsupported compiler. \n'
              'Building is likely to fail.')
    red_print(60 * '-')
    return 1
  
  if not cxx_env:
    yellow_print(60 * '-')
    yellow_print('CXX environment variable not set. \n'
                 'CXX is strongly recommended to be set to a compiler with C++23 support.')
    yellow_print(60 * '-')

  green_print('# Environment setup complete.')
  return 0
