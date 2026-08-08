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


import re

from . import paths
from .env import Tool, check_tool
from .utils import run_cmd, yellow_print, red_print, green_print


# Tracked, not fetched: until 2026-08-07 this file was gitignored and pulled from llvm-project
# `main` on every CI run, so a mutable third-party script drove the pinned clang-tidy (#72, #73).
VENDORED_RUNNER = "run-clang-tidy.py"


def run_ruff() -> int:
  """Run ruff on the project source code."""  
  print("#" * 60)

  if not check_tool(Tool("ruff")):
    red_print("ruff not found!")
    yellow_print("Install ruff by `pip install ruff`")
    return 1

  yellow_print("Running ruff...")
  proj_root = paths.proj_root()
  ret = run_cmd(["ruff", "check", str(proj_root)])

  if ret.retcode == 0:
    green_print("ruff passed")
  else:
    red_print("ruff failed")

  return ret.retcode


def run_clang_tidy() -> int:
  """Run clang-tidy on the project CPP source code."""
  print("#" * 60)

  if not check_tool(Tool("clang-tidy")):
    red_print("clang-tidy not found!")
    yellow_print("Install clang-tidy by `pip install clang-tidy` -- the version CI pins is in core_tests.yml")
    return 1

  if not (paths.proj_root() / VENDORED_RUNNER).exists():
    red_print(f"{VENDORED_RUNNER} is missing from the repo root.")
    yellow_print(f"It is tracked -- restore it with `git checkout -- {VENDORED_RUNNER}`.")
    return 1

  build_dir = paths.build_dir()
  db_json_path = build_dir / "compile_commands.json"

  if not db_json_path.exists():
    red_print("compile_commands.json not found")
    return 1

  yellow_print("Running clang-tidy...")
  # Ensure non-0 exit code on any warning or error
  ret = run_cmd(["python3", VENDORED_RUNNER, "-p", str(build_dir), "-header-filter=src/"],
                cwd=str(paths.proj_root()))

  if ret.retcode == 0:
    green_print("clang-tidy passed")
  else:
    red_print("clang-tidy failed")

  # Save stderr and stdout to files.
  def clean_text(text: str) -> str:
    control_chars = re.compile(r"[\x00-\x09\x0B\x0C\x0E-\x1F\x7F]")
    return control_chars.sub("", text)

  stdout_log = build_dir / "clang-tidy-stdout.log"
  stderr_log = build_dir / "clang-tidy-stderr.log"

  # Explicit UTF-8: Windows' locale default (cp1252) chokes on non-ASCII in analyzed sources.
  with stdout_log.open("w", encoding="utf-8") as f:
    f.write(clean_text(ret.stdout))

  with stderr_log.open("w", encoding="utf-8") as f:
    f.write(clean_text(ret.stderr))

  return ret.retcode
