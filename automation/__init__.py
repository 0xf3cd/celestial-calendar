# automation/__init__.py
from .environment import setup_environment
from .project import run_cmake, build_project, clean_build
from .test import run_tests, find_tests, list_tests
from .sysinfo import print_system_info
from .utils import green_print, red_print, yellow_print, blue_print, run_cmd, ProcReturn, time_execution
