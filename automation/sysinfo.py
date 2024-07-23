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
import sys
import shutil
import platform
from datetime import datetime
from .utils import run_cmd, ProcReturn

def print_system_info() -> None:
  """Print system time and other system information."""
  cmake_version: ProcReturn = run_cmd(['cmake', '--version'], print_cmd=False, print_stdout=False)
  assert cmake_version.retcode == 0

  cmake_version_str: str = ' | '.join(
    filter(lambda s: len(s.strip()) > 0, cmake_version.stdout.splitlines())
  )
  this_moment: datetime = datetime.now()

  print(60 * '#')

  print(f'-- system time: {this_moment.astimezone()} ({this_moment.astimezone().tzinfo})')
  print(f'-- cwd: {os.getcwd()}')
  print(f'-- default encoding: {sys.getdefaultencoding()}')
  print(f'-- python executable: {sys.executable}')
  print(f'-- python version: {sys.version}')

  print(f'-- cmake path: {shutil.which("cmake")}')
  print(f'-- cmake version: {cmake_version_str}')
  print(f'-- os env CXX: {os.environ.get("CXX", None)}')
  print(f'-- os env CC: {os.environ.get("CC", None)}')
  print(f'-- clang++: {shutil.which("clang++")}')
  print(f'-- g++: {shutil.which("g++")}')

  print(f'-- node: {platform.node()}')
  print(f'-- system: {platform.system()}')
  print(f'-- platform: {platform.platform()}')
  print(f'-- release: {platform.release()}')
  print(f'-- version: {platform.version()}')
  print(f'-- machine: {platform.machine()}')
  print(f'-- processor: {platform.processor()}')
  print(f'-- architecture: {platform.architecture()}')
  print(f'-- cpu cores: {os.cpu_count()}')

  print(60 * '#')
