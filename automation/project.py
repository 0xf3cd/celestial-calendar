from pathlib import Path
import shutil
from .utils import run_cmd, yellow_print, red_print, green_print, ProcReturn

BUILD_DIR = Path(__file__).parent.parent / 'build'


def run_cmake() -> int:
  print('#' * 60)

  if not BUILD_DIR.exists():
    red_print('# Build directory not found')
    yellow_print(f'# Creating {BUILD_DIR}')
    BUILD_DIR.mkdir()

  yellow_print('# Running cmake...')
  ret: ProcReturn = run_cmd(['cmake', '..'], cwd=BUILD_DIR)

  print('#' * 60)
  return ret.retcode


def build_project(cpu_cores: int = 8) -> int:
  print('#' * 60)

  assert BUILD_DIR.exists(), 'Build directory not found'
  assert BUILD_DIR.is_dir(), 'Build directory is not a directory'

  yellow_print('# Building the C++ projects...')
  ret: ProcReturn = run_cmd(['make', '-j', str(cpu_cores)], cwd=BUILD_DIR)

  print('#' * 60)
  return ret.retcode


def clean_build() -> int:
  print('#' * 60)

  if BUILD_DIR.exists():
    yellow_print(f'# Build dir exists.')
    red_print(f'# Removing {BUILD_DIR}')
    shutil.rmtree(BUILD_DIR)

  green_print(f'# Build cleaned...')
  print('#' * 60)
  return 0
