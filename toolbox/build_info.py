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
import sys
import json
import platform
import argparse

from pathlib import Path
from datetime import datetime, UTC
from dataclasses import dataclass, asdict

from typing import Optional, List, Sequence


# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import (
  run_cmd, ProcReturn, yellow_print, green_print
)


def silent_run(cmd: Sequence[str]) -> ProcReturn:
  '''Run a command without printing anything.'''
  return run_cmd(cmd, print_cmd=False, print_stdout=False, print_stderr=False)


@dataclass(frozen=True)
class BuildInfo:
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

  pack_time: str
  git_commit: str


def compiler_version(compiler: str) -> str:
  result = silent_run([compiler, '--version'])
  if result.retcode != 0:
    return ''
  return result.stdout


def pack_build_info(docker: Optional[str]) -> BuildInfo:
  cxx = os.environ.get('CXX', '')
  cxx_version = compiler_version(cxx) if cxx else ''

  cc = os.environ.get('CC', '')
  cc_version = compiler_version(cc) if cc else ''

  commit_hash = silent_run(['git', 'rev-parse', 'HEAD']).stdout.strip()

  return BuildInfo(
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
    pack_time=datetime.now(UTC).isoformat(),
    git_commit=commit_hash
  )


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--save-dir', type=Path, required=True, default=None)
  parser.add_argument('--filename', type=str, default='build_info.json')
  parser.add_argument('--docker', type=str, default='')

  args = parser.parse_args()

  build_info = pack_build_info(args.docker)

  if not args.save_dir.exists():
    yellow_print(f'> Creating directory: {args.save_dir}')
    args.save_dir.mkdir(parents=True)

  real_filename = args.filename
  if not real_filename.endswith('.json'):
    real_filename += '.json'

  with open(args.save_dir / real_filename, 'w') as f:
    json.dump(asdict(build_info), f, indent=2)

  green_print(f'> Build information saved to: {args.save_dir / args.filename}')
