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

# `paths` is exported as the submodule, not as its individual functions: every caller -- inside
# the package and in `toolbox/` -- reaches for `paths.build_dir()` rather than the bare name.
from . import paths
from .env import Tool, SetupPlan, setup_environment
from .build import run_cmake, build_project, clean_build
from .gtest import run_gtests
from .sysinfo import print_system_info
from .utils import (
  green_print, red_print, yellow_print, blue_print,
  run_cmd, ProcReturn, time_execution
)
from .linter import run_ruff, run_clang_tidy, run_pytest
from .self_contained import check_self_contained
from .feature_probe import probe_features
from .abi_layout import check_abi_layout
from .ctypes_smoke import check_ctypes_smoke
from .export_surface import check_export_surface
from .log_names import check_log_names
from .ai_workflows import check_ai_workflows
from .action_pins import check_action_pins
from .jieqi_table import check_jieqi_table
from .seed_reconcile import check_seed_reconcile
from .bench import build_benchmarks, run_benchmarks

__all__ = [
  "paths",
  "Tool", "SetupPlan", "setup_environment",
  "run_cmake", "build_project", "clean_build",
  "run_gtests", "print_system_info",
  "green_print", "red_print", "yellow_print", "blue_print",
  "run_cmd", "ProcReturn", "time_execution",
  "run_ruff", "run_clang_tidy", "run_pytest", "check_self_contained", "probe_features",
  "check_abi_layout", "check_ctypes_smoke", "check_export_surface", "check_log_names",
  "check_ai_workflows", "check_action_pins", "check_jieqi_table", "check_seed_reconcile",
  "build_benchmarks", "run_benchmarks"
]
