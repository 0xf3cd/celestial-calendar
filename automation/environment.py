import os
import shutil
import tempfile
from itertools import product
from pathlib import Path
from typing import List
from .utils import yellow_print, red_print, green_print, run_cmd, blue_print

REQUIREMENTS_FILE = Path(__file__).parent.parent / 'Requirements.txt'


def install_dependencies() -> int:
  if REQUIREMENTS_FILE.exists():
    yellow_print('# Installing Python dependencies...')
    pip_executable = shutil.which('pip')
    if pip_executable is None:
      pip_executable = shutil.which('pip3')

    if pip_executable:
      ret = run_cmd([pip_executable, 'install', '-r', str(REQUIREMENTS_FILE)])
      if ret.retcode != 0:
        return ret.retcode
    else:
      red_print('# pip not found!')
      return 1
  return 0


def check_tool_exists(tool_name: str, args: List[str] = ['--version']) -> bool:
  tool_path = shutil.which(tool_name)
  if tool_path is None:
    red_print(f'# {tool_name} not found!')
    return False

  result = run_cmd([tool_path] + args, print_cmd=False, print_stdout=False, print_stderr=False)
  if result.retcode != 0:
    red_print(f'# {tool_name} does not support required features!')
    return False

  return True


def check_cpp_support(compiler: str, cpp_version: str) -> bool:
  with tempfile.TemporaryDirectory() as tmpdir:
    tmp_cpp_file = Path(tmpdir) / 'test_cpp.cpp'
    tmp_cpp_file.write_text('#include <print>\n#include <ranges>\nint main() { std::println("Hello, World!"); return 0; }')

    cxx_test_command = [compiler, f'--std={cpp_version}', str(tmp_cpp_file), '-o', str(Path(tmpdir) / 'test_cpp')]
    proc_ret = run_cmd(cxx_test_command, print_cmd=False, print_stdout=False, print_stderr=False)

    return proc_ret.retcode == 0


def setup_environment() -> int:
  retcode = install_dependencies()
  if retcode != 0:
    return retcode

  tools = [
    ('cmake', ['--version']),
    ('make', ['--version']),
  ]

  for tool_name, args in tools:
    if not check_tool_exists(tool_name, args):
      return 1

  # Check for C++ support
  cxx_compilers = ['clang++', 'g++', 'g++-14', 'clang++-18']
  cxx_env = os.environ.get('CXX')
  if cxx_env:
    cxx_compilers.append(cxx_env)
  
  cpp_versions = ['c++23', 'c++2b']
  supporting_compilers = []

  for compiler, cpp_version in product(cxx_compilers, cpp_versions):
    if check_cpp_support(compiler, cpp_version):
      compiler_path = shutil.which(compiler)
      supporting_compilers.append((compiler_path, cpp_version))
      green_print(f'# {compiler} at {compiler_path} supports {cpp_version}.')
    else:
      red_print(f'# {compiler} at {shutil.which(compiler)} does not support {cpp_version}.')

  if not supporting_compilers:
    red_print('None of the compilers support the specified C++ features.')
    return 1

  blue_print('Supported compilers and C++ versions:')
  for compiler_path, cpp_version in supporting_compilers:
    blue_print(f'# {compiler_path} supports {cpp_version}')

  green_print('# Environment setup complete.')
  return 0
