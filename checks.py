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
# SPDX-License-Identifier: MIT

import sys
import argparse

from automation import (
  run_ruff,
  run_clang_tidy,
  run_pytest,
  check_self_contained,
  probe_features,
  check_abi_layout,
  check_ctypes_smoke,
  check_export_surface,
  check_ai_workflows,
  check_action_pins,
  check_jieqi_table,
  check_provenance,
  check_cpp_signature_layout,
)


def parse_args() -> argparse.Namespace:
  """Parse the command line arguments."""
  parser = argparse.ArgumentParser(
    description="Repo checks: lint, gates, probes, and the automation layer's tests",
    # CI writes every flag in full; a typo must fail loudly, not resolve to a unique prefix.
    allow_abbrev=False,
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
      "  To run the automation layer's own unit tests:\n"
      "    ./checks.py --pytest\n\n"
      "  To verify retained sources, values, licenses and notices directly:\n"
      "    ./checks.py --provenance\n\n"
      "  To check C++ signature layout without clang-tidy:\n"
      "    ./checks.py --signature-layout\n\n"
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
      "  export surface, pytest, provenance, signature layout, AI workflows, Action pins,\n"
      "  jieqi table, feature report):\n"
      "    ./checks.py -a/--all\n\n"
    ),
    formatter_class=argparse.RawTextHelpFormatter,
  )

  parser.add_argument("-a", "--all", action="store_true", help="Run every check")

  lint = parser.add_argument_group("lint")
  lint.add_argument("--ruff", action="store_true", help="Run ruff")
  lint.add_argument("--clang-tidy", action="store_true", help="Run clang-tidy")
  lint.add_argument("--signature-layout", action="store_true", help="Check C++ signature layout")

  gate = parser.add_argument_group(
    "gate", "repo invariants; ctypes-smoke, export-surface and jieqi-table need ./project.py --build first"
  )
  gate.add_argument(
    "--self-contained", action="store_true", help="Compile every header alone to prove it is self-contained"
  )
  gate.add_argument(
    "--abi-layout", action="store_true", help="Hold the package's real ctypes declarations to the compiled C ABI"
  )
  gate.add_argument(
    "--ctypes-smoke", action="store_true", help="Run the ctypes wrappers against the built library (needs a build)"
  )
  gate.add_argument(
    "--export-surface",
    action="store_true",
    help="Hold the built library's export surface to the celestial.h entry points",
  )
  gate.add_argument(
    "--jieqi-table", action="store_true", help="Hold the exported jieqi table to its invariants (needs a build)"
  )
  gate.add_argument(
    "--provenance", action="store_true", help="Directly verify retained sources, values, licenses and notices"
  )
  gate.add_argument(
    "--ai-workflows", action="store_true", help="Hold the AI workflows to the settings of theirs that fail silently"
  )
  gate.add_argument("--action-pins", action="store_true", help="Require immutable refs for third-party GitHub Actions")

  probe = parser.add_argument_group("probe")
  probe.add_argument(
    "--features",
    nargs="?",
    const="",
    default=None,
    metavar="LEG",
    help="Probe the awaited C++ features; with a CI leg name, hold it to the recorded state",
  )

  test = parser.add_argument_group("test")
  test.add_argument(
    "--pytest", action="store_true", help="Run the automation layer's own unit tests (automation/tests/)"
  )

  return parser.parse_args()


def run_checks(args: argparse.Namespace) -> int:
  checks = (
    (args.ruff, run_ruff, ()),
    (args.signature_layout, check_cpp_signature_layout, ()),
    (args.clang_tidy, run_clang_tidy, ()),
    (args.self_contained, check_self_contained, ()),
    (args.abi_layout, check_abi_layout, ()),
    (args.ai_workflows, check_ai_workflows, ()),
    (args.action_pins, check_action_pins, ()),
    (args.provenance, check_provenance, ()),
    (args.ctypes_smoke, check_ctypes_smoke, ()),
    (args.jieqi_table, check_jieqi_table, ()),
    (args.export_surface, check_export_surface, ()),
    (args.pytest, run_pytest, ()),
    # `--all` reports without judging; only a named CI leg has a recorded state.
    (args.features is not None, probe_features, (args.features or None,)),
  )
  for selected, check, parameters in checks:
    if selected or args.all:
      result = check(*parameters)
      if result != 0:
        return result
  return 0


if __name__ == "__main__":
  sys.exit(run_checks(parse_args()))
