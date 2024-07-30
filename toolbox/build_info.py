#!/usr/bin/env python3
#
# Helper to get build information, including OS, platform, CPU arch, compiler info, and Docker version...
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
import json
import hashlib
import platform
import argparse

from pathlib import Path
from datetime import datetime, UTC
from dataclasses import dataclass, asdict

from typing import Optional, List, Sequence, Dict


# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import (
  paths, run_cmd, ProcReturn, 
  yellow_print, green_print, red_print,
)


def silent_run(cmd: Sequence[str], **kwargs) -> ProcReturn:
  '''Run a command without printing anything. Also check the return code. '''
  proc_ret = run_cmd(cmd, print_cmd=False, print_stdout=False, print_stderr=False, **kwargs)
  if proc_ret.retcode != 0:
    yellow_print('Command failed:', ' '.join(cmd))
    raise RuntimeError('Command failed')
  return proc_ret


@dataclass(frozen=True)
class BuildInfo:
  build_version: str

  os: str
  kernel: str
  docker: str

  platform: str
  machine: str
  architecture: List[str]
  endianness: str

  cpu_type: str
  libc_version: List[str]
  cxx: str
  cc: str
  cxx_version: str
  cc_version: str
  python_version: str

  sha256: Dict[str, str]
  additional_info: Dict[str, str]

  pack_time: str
  git_commit: str


def compiler_version(compiler: str) -> str:
  result = silent_run([compiler, '--version'])
  if result.retcode != 0:
    return ''
  return result.stdout


def get_cpu_info() -> Dict:
  '''Return the CPU information as a string. The returned str is ready to write to a file.'''
  def command() -> List[str]:
    if sys.platform == 'darwin':
      return ['system_profiler', 'SPHardwareDataType', '-json']
    elif sys.platform == 'linux':
      return ['lscpu', '--json']
    elif sys.platform == 'win32':
      return ['Get-WmiObject -Class Win32_Processor | ConvertTo-Json']
    else:
      raise OSError(f'Unsupported platform: {sys.platform}')

  cmd = command()
  proc_ret = silent_run(cmd)

  info = json.loads(proc_ret.stdout)
  info['cmd'] = ' '.join(cmd)
  return info


def find_built_shared_libs() -> List[Path]:
  '''Find all the built shared libraries in the 'shared_lib' directory.'''
  lib_dir = paths.shared_lib_dir()
  if not lib_dir.exists():
    red_print(f'> No shared libraries found in {lib_dir}')
    return []

  # Return the files with dylib, dll, or so in the filenames.
  shared_libs = []
  lib_pattern = re.compile(r'\.so(\.\d+)*$|\.dylib$|\.dll$')

  for file in lib_dir.iterdir():
    if file.is_file() and lib_pattern.search(file.name):
      shared_libs.append(file)

  return shared_libs


def calc_file_hash(file: Path, algo: str = 'sha256') -> str:
  '''Calculate the hash of a file.'''
  h = hashlib.new(algo)
  with open(file, 'rb') as f:
    for chunk in iter(lambda: f.read(4096), b''):
      h.update(chunk)
  return h.hexdigest()


def shared_lib_hashs() -> Dict[str, str]:
  '''Get the hash of each shared library in the 'shared_lib' directory.'''
  shared_libs = find_built_shared_libs()
  if not shared_libs:
    return {}

  return {
    p.name: calc_file_hash(p, 'sha256')
    for p in shared_libs
  }


def get_additional_info(lib: Path) -> str:
  '''Get the additional information about a shared library.'''
  def command() -> List[str]:
    if sys.platform == 'darwin':
      return ['otool', '-hLv', str(lib)]
    elif sys.platform == 'linux':
      return ['readelf', '-h', str(lib)]
    elif sys.platform == 'win32':
      return ['dumpbin', '/headers', str(lib)]
    else:
      raise OSError(f'Unsupported platform: {sys.platform}')
    
  cmd = command()
  proc_ret = silent_run(cmd)
  if proc_ret.retcode != 0:
    return 'Failed to get additional information'
  
  cmd_str = 'Command: ' + ' '.join(cmd) + '\n\n'
  return cmd_str + (60 * '#') + proc_ret.stdout


def shared_lib_additional_info() -> Dict[str, str]:
  '''Get the additional information about the build.'''
  shared_libs = find_built_shared_libs()
  if not shared_libs:
    return {}
  
  return {
    p.name: get_additional_info(p)
    for p in shared_libs
  }


def pack_build_info(docker: Optional[str]) -> BuildInfo:
  proj_dir = Path(__file__).parent.parent

  proj_py = proj_dir / 'project.py'
  if not proj_py.exists():
    raise ValueError('project.py not found!')

  build_version = silent_run([sys.executable, str(proj_py), '--version']).stdout.strip()

  cxx = os.environ.get('CXX', '')
  cxx_version = compiler_version(cxx) if cxx else ''

  cc = os.environ.get('CC', '')
  cc_version = compiler_version(cc) if cc else ''

  commit_hash = silent_run(['git', 'rev-parse', 'HEAD'], cwd=proj_dir).stdout.strip()

  return BuildInfo(
    build_version=build_version,
    os=platform.system(),
    kernel=platform.release(),
    docker=docker or '',
    platform=platform.platform(),
    machine=platform.machine(),
    architecture=list(platform.architecture()),
    endianness=sys.byteorder,
    cpu_type=platform.processor(),
    libc_version=list(platform.libc_ver()),
    cxx=cxx,
    cc=cc,
    cxx_version=cxx_version,
    cc_version=cc_version,
    python_version=platform.python_version(),
    sha256=shared_lib_hashs(),
    additional_info=shared_lib_additional_info(),
    pack_time=datetime.now(UTC).isoformat(),
    git_commit=commit_hash
  )


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--save-to', type=Path, required=True, default=None)
  parser.add_argument('--docker', type=str, default='')

  args = parser.parse_args()

  build_info = pack_build_info(args.docker)
  cpu_info = get_cpu_info()

  if not args.save_to.exists():
    yellow_print(f'> Creating directory: {args.save_to}')
    args.save_to.mkdir(parents=True, exist_ok=True)

  with open(args.save_to / 'build_info.json', 'w') as f:
    json.dump(asdict(build_info), f, indent=2)

  with open(args.save_to / 'cpu_info.json', 'w') as f:
    json.dump(cpu_info, f, indent=2)

  green_print(f'> Build information saved to: {args.save_to}')
