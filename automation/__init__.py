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

from .environment import check_compilers, setup_environment
from .build import run_cmake, build_project, clean_build
from .test import run_tests, find_tests, list_tests
from .sysinfo import print_system_info
from .utils import (
  green_print, red_print, yellow_print, blue_print, 
  run_cmd, ProcReturn, time_execution
)
from .paths import (
  proj_root, build_dir, cpp_src_dir, python_requirements, cpp_test_dir
)
from .github import GitHub

__all__ = [
  'check_compilers',
  'setup_environment',
  'run_cmake',
  'build_project',
  'clean_build',
  'run_tests',
  'find_tests',
  'list_tests',
  'print_system_info',
  'green_print',
  'red_print',
  'yellow_print',
  'blue_print',
  'run_cmd',
  'ProcReturn',
  'time_execution',
  'proj_root',
  'build_dir',
  'cpp_src_dir',
  'python_requirements',
  'cpp_test_dir',
  'GitHub',
]
