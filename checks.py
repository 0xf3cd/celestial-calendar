#!/usr/bin/env python3
#
# Automation entry point for the repo's checks: linters (ruff, clang-tidy), invariant gates,
# the toolchain feature probe, and the automation layer's own tests.
#
#########################################################################################
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

import sys
import argparse

from automation import (
  run_ruff, run_clang_tidy, run_pytest, check_self_contained, probe_features, check_abi_layout,
  check_ctypes_smoke, check_export_surface, check_log_names, check_ai_workflows,
  check_action_pins, check_jieqi_table, check_seed_reconcile,
)


def parse_args() -> argparse.Namespace:
  """Parse the command line arguments."""
  parser = argparse.ArgumentParser(
    description="Repo checks: lint, gates, probes, and the automation layer's tests",
    epilog=(
      "Examples of usage:\n"
      "  To run ruff to check Python codes:\n"
      "    ./checks.py --ruff\n\n"
      "  To run clang-tidy to check C++ codes:\n"
      "    ./checks.py --clang-tidy\n\n"
      "  To check that every header is self-contained:\n"
      "    ./checks.py --self-contained\n\n"
      "  To hold the ctypes mirror to the real ABI layout in celestial.h:\n"
      "    ./checks.py --abi-layout\n\n"
      "  To run the ctypes wrappers against the built library (needs ./project.py --build first):\n"
      "    ./checks.py --ctypes-smoke\n\n"
      "  To hold the built library's export surface to the celestial.h entry points\n"
      "  (needs ./project.py --build first):\n"
      "    ./checks.py --export-surface\n\n"
      "  To hold the lib_*.cpp log strings to the celestial.h entry-point names:\n"
      "    ./checks.py --log-names\n\n"
      "  To run the automation layer's own unit tests:\n"
      "    ./checks.py --pytest\n\n"
      "  To hold the four CELESTIAL_TEST_SEED copies to each other:\n"
      "    ./checks.py --seed-reconcile\n\n"
      "  To hold the AI workflows to the settings of theirs that fail silently:\n"
      "    ./checks.py --ai-workflows\n\n"
      "  To require immutable refs for third-party GitHub Actions:\n"
      "    ./checks.py --action-pins\n\n"
      "  To hold the exported jieqi table to its invariants and the HKO anchors\n"
      "  (needs ./project.py --build first):\n"
      "    ./checks.py --jieqi-table\n\n"
      "  To report which awaited C++ features this toolchain can compile:\n"
      "    ./checks.py --features\n\n"
      "  To hold a CI leg to the feature state this repo recorded for it:\n"
      "    ./checks.py --features libc++\n\n"
      "  To run every check (ruff, clang-tidy, self-containment, ABI layout, ctypes smoke,\n"
      "  export surface, log names, pytest, seed reconcile, AI workflows, Action pins, jieqi table, feature report):\n"
      "    ./checks.py -a/--all\n\n"
    ),
    formatter_class=argparse.RawTextHelpFormatter
  )

  parser.add_argument("-a", "--all", action="store_true", help="Run every check")

  lint = parser.add_argument_group("lint")
  lint.add_argument("--ruff", action="store_true", help="Run ruff")
  lint.add_argument("--clang-tidy", action="store_true", help="Run clang-tidy")

  gate = parser.add_argument_group(
    "gate", "repo invariants; ctypes-smoke, export-surface and jieqi-table need ./project.py --build first"
  )
  gate.add_argument("--self-contained", action="store_true",
                    help="Compile every header alone to prove it is self-contained")
  gate.add_argument("--abi-layout", action="store_true",
                    help="Hold the ctypes mirror in statistics/common.py to the real ABI layout")
  gate.add_argument("--ctypes-smoke", action="store_true",
                    help="Run the ctypes wrappers against the built library (needs a build)")
  gate.add_argument("--export-surface", action="store_true",
                    help="Hold the built library's export surface to the celestial.h entry points")
  gate.add_argument("--jieqi-table", action="store_true",
                    help="Hold the exported jieqi table to its invariants (needs a build)")
  gate.add_argument("--log-names", action="store_true",
                    help="Hold the lib_*.cpp log strings to the celestial.h entry-point names")
  gate.add_argument("--seed-reconcile", action="store_true",
                    help="Hold the four CELESTIAL_TEST_SEED copies to each other")
  gate.add_argument("--ai-workflows", action="store_true",
                    help="Hold the AI workflows to the settings of theirs that fail silently")
  gate.add_argument("--action-pins", action="store_true",
                    help="Require immutable refs for third-party GitHub Actions")

  probe = parser.add_argument_group("probe")
  probe.add_argument("--features", nargs="?", const="", default=None, metavar="LEG",
                     help="Probe the awaited C++ features; with a CI leg name, hold it to the recorded state")

  test = parser.add_argument_group("test")
  test.add_argument("--pytest", action="store_true",
                    help="Run the automation layer's own unit tests (automation/tests/)")

  return parser.parse_args()


if __name__ == "__main__":
  args = parse_args()

  if args.ruff or args.all:
    ret_code = run_ruff()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.clang_tidy or args.all:
    ret_code = run_clang_tidy()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.self_contained or args.all:
    ret_code = check_self_contained()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.abi_layout or args.all:
    ret_code = check_abi_layout()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.ai_workflows or args.all:
    ret_code = check_ai_workflows()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.action_pins or args.all:
    ret_code = check_action_pins()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.ctypes_smoke or args.all:
    ret_code = check_ctypes_smoke()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.jieqi_table or args.all:
    ret_code = check_jieqi_table()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.export_surface or args.all:
    ret_code = check_export_surface()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.log_names or args.all:
    ret_code = check_log_names()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.pytest or args.all:
    ret_code = run_pytest()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.seed_reconcile or args.all:
    ret_code = check_seed_reconcile()
    if ret_code != 0:
      sys.exit(ret_code)

  if args.features is not None or args.all:
    # `--all` reports without judging: only a CI leg has a recorded state to be held to.
    ret_code = probe_features(args.features or None)
    if ret_code != 0:
      sys.exit(ret_code)

  sys.exit(0)
