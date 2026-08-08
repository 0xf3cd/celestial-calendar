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


# `run-clang-tidy.py` is vendored verbatim from this LLVM release, and the CI jobs install
# `clang-tidy==CLANG_TIDY_PIN`. The runner and the binary are one pin, not two -- the runner is
# that release's own driver, and pairing it with another clang-tidy is a combination nobody
# chose. Until 2026-08-07 the runner was gitignored and re-fetched from llvm-project `main` on
# every CI run, so the pinned binary was in fact driven by an unpinned script (#72, #73).
CLANG_TIDY_PIN = "18.1.8"
VENDORED_RUNNER = "run-clang-tidy.py"


def check_clang_tidy_pin() -> int:
  """Refuse to run unless the vendored runner, the installed binary and the CI pin all agree."""
  runner = paths.proj_root() / VENDORED_RUNNER
  if not runner.exists():
    red_print(f"{VENDORED_RUNNER} is missing from the repo root.")
    yellow_print(f"It is vendored from llvmorg-{CLANG_TIDY_PIN}, not downloaded -- restore it from git.")
    return 1

  ret = run_cmd(["clang-tidy", "--version"], print_cmd=False, print_stdout=False, print_stderr=False)
  installed = re.search(r"LLVM version (\S+)", ret.stdout)
  if installed is None:
    red_print("Cannot read the clang-tidy version.")
    return 1
  if installed.group(1) != CLANG_TIDY_PIN:
    red_print(f"clang-tidy is {installed.group(1)}, but {VENDORED_RUNNER} is vendored from {CLANG_TIDY_PIN}.")
    yellow_print(f"Install the pin with `pip install clang-tidy=={CLANG_TIDY_PIN}`, or re-vendor the runner.")
    return 1

  # The workflow is what actually installs the binary in CI. If its pin drifts from ours, CI would
  # run a pair this gate never saw -- so the gate reads the workflow instead of trusting a comment.
  workflow = paths.proj_root() / ".github" / "workflows" / "core_tests.yml"
  declared = re.search(r"clang-tidy==(\S+)", workflow.read_text(encoding="utf-8"))
  if declared is None or declared.group(1) != CLANG_TIDY_PIN:
    found = "no clang-tidy pin at all" if declared is None else declared.group(1)
    red_print(f"core_tests.yml installs {found}, but {VENDORED_RUNNER} is vendored from {CLANG_TIDY_PIN}.")
    yellow_print("Bump the workflow pin and re-vendor the runner together, or neither.")
    return 1

  return 0


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
    yellow_print(f"Install clang-tidy by `pip install clang-tidy=={CLANG_TIDY_PIN}`")
    return 1

  if check_clang_tidy_pin() != 0:
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
