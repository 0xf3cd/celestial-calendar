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
import shutil

from . import paths
from .env import Tool, check_tool
from .utils import run_cmd, yellow_print, red_print, green_print, blue_print


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

  # `CLANG_TIDY` names the binary, never the version: the version is pinned in the workflow
  # (gotcha 9) and a copy here would be a second one with no gate reconciling them. The default
  # keeps the bare name so an unset variable behaves as it always has -- but an empty value is a
  # typo rather than a request for the default, and `os.environ.get(name, default)` hands back
  # that empty string, so the fallback has to be `or`.
  binary = os.environ.get("CLANG_TIDY") or "clang-tidy"

  if not check_tool(Tool(binary)):
    red_print(f"{binary} not found!")
    yellow_print("Install clang-tidy, or point CLANG_TIDY at the one to use")
    return 1

  build_dir = paths.build_dir()
  db_json_path = build_dir / "compile_commands.json"

  if not db_json_path.exists():
    red_print("compile_commands.json not found")
    return 1

  # Which binary answered, spelled out: a runner one major ahead of its clang-tidy analyses a
  # subset in silence rather than failing, and 11 findings where CI reports 20 is otherwise a
  # mystery. Nothing here compares the version against anything -- it only says what ran.
  version = run_cmd([binary, "--version"], print_cmd=False, print_stdout=False, print_stderr=False)
  blue_print(f"# clang-tidy: {shutil.which(binary) or binary}")
  blue_print(f"# {(version.stdout or '').splitlines()[0] if version.stdout else 'version unknown'}")

  yellow_print("Running clang-tidy...")
  # Ensure non-0 exit code on any warning or error
  ret = run_cmd(["python3", "run-clang-tidy.py", "-p", str(build_dir), "-header-filter=src/",
                 "-clang-tidy-binary", binary],
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
