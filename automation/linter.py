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


from . import paths
from .environment import check_tool_exists
from .utils import run_cmd, yellow_print, red_print, green_print


def ensure_run_clang_tidy() -> int:
  '''Ensure that clang-tidy and the runner 'run-clang-tidy.py' is installed.'''
  if not check_tool_exists('clang-tidy'):
    red_print('clang-tidy not found!')
    return 1
  
  proj_root = paths.proj_root()
  run_clang_tidy = proj_root / 'run-clang-tidy.py'
  if run_clang_tidy.exists():
    return 0
  
  # Otherwise, download it.
  if not check_tool_exists('curl', ('--version',)):
    red_print('curl not found!')
    return 1

  yellow_print('Downloading run-clang-tidy.py...')
  ret = run_cmd(['curl', '-s', 'https://raw.githubusercontent.com/LLVM/llvm-project/main/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py', 
                 '-o', str(run_clang_tidy)])
  if ret.retcode != 0:
    red_print('Failed to download run-clang-tidy.py')
    return 1

  return 0


def run_ruff() -> int:
  '''Run ruff on the project source code.'''  
  print('#' * 60)
  
  if not check_tool_exists('ruff'):
    red_print('ruff not found!')
    return 1
  
  yellow_print('Running ruff...')
  proj_root = paths.proj_root()
  ret = run_cmd(['ruff', 'check', str(proj_root)])

  if ret.retcode == 0:
    green_print('ruff passed')
  else:
    red_print('ruff failed')

  return ret.retcode


def run_clang_tidy() -> int:
  '''Run clang-tidy on the project CPP source code.'''
  print('#' * 60)

  if ensure_run_clang_tidy() != 0:
    return 1

  build_dir = paths.build_dir()
  db_json_path = build_dir / 'compile_commands.json'

  if not db_json_path.exists():
    red_print('compile_commands.json not found')
    return 1

  yellow_print('Running clang-tidy...')
  # Ensure non-0 exit code on any warning or error
  ret = run_cmd(['python3', 'run-clang-tidy.py', '-p', str(build_dir)], 
                cwd=str(paths.proj_root()))

  if ret.retcode == 0:
    green_print('clang-tidy passed')
  else:
    red_print('clang-tidy failed')

  return ret.retcode


if __name__ == '__main__':
  ret = run_ruff()
  ret |= run_clang_tidy()
  exit(ret)
