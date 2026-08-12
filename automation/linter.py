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


import importlib.util
import os
import re
import sys

from . import paths
from .env import Tool, check_tool
from .utils import run_cmd, yellow_print, red_print, green_print


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

  if not check_tool(Tool(binary), report=True):
    yellow_print("Install clang-tidy, or point CLANG_TIDY at the one to use")
    return 1

  build_dir = paths.build_dir()
  db_json_path = build_dir / "compile_commands.json"

  if not db_json_path.exists():
    red_print("compile_commands.json not found")
    return 1

  yellow_print("Running clang-tidy...")
  # Ensure non-0 exit code on any warning or error
  ret = run_cmd(["python3", "run-clang-tidy.py", "-p", str(build_dir), "-header-filter=src/",
                 "-clang-tidy-binary", binary],
                cwd=str(paths.proj_root()))

  if ret.retcode == 0:
    green_print("clang-tidy passed")
  else:
    red_print("clang-tidy failed")
    # A local run without the pinned-toolchain recipe fails as a cascade of
    # 'xxx file not found' diagnostics (the system headers are not on the default
    # search path) — that is an environment failure, not code findings. Point at the
    # recipe instead of letting someone read the noise as signal.
    if "file not found" in ret.stdout:
      yellow_print(
        "note: 'file not found' diagnostics mean the local clang-tidy can't see the C++ "
        "standard headers — pass -isysroot (xcrun --show-sdk-path) plus the libstdc++ "
        "include dirs matching the CI leg's gcc major, e.g. via run-clang-tidy.py's "
        "-extra-arg. The diagnostics above are environment noise, not findings."
      )

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


def run_pytest() -> int:
  """Run the automation layer's own unit tests (automation/tests/)."""
  print("#" * 60)

  # `python -m pytest`, never the console script: only the -m form puts the project root
  # on sys.path, and the tests import `automation.*`.
  if importlib.util.find_spec("pytest") is None:
    red_print("pytest not found!")
    yellow_print("Install the pinned pytest from Requirements.txt")
    return 1

  yellow_print("Running pytest on automation/tests/...")
  tests_dir = paths.proj_root() / "automation" / "tests"
  ret = run_cmd([sys.executable, "-m", "pytest", str(tests_dir), "-q"], cwd=paths.proj_root())

  if ret.retcode == 0:
    green_print("pytest passed")
  else:
    red_print("pytest failed")

  return ret.retcode
