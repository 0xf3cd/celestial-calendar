#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import argparse
import sys

from pathlib import Path

sys.path.append(str(Path(__file__).parent.parent))

from toolbox.registry_validation import classify_npm_candidate, wait_for_candidate_registries


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description="Validate exact npm and PyPI release artifacts.")
  parser.add_argument("command", choices=("classify-npm", "verify"))
  parser.add_argument("--candidate", type=Path, required=True)
  parser.add_argument("--version", required=True)
  parser.add_argument("--commit", required=True)
  parser.add_argument("--github-output", type=Path)
  parser.add_argument("--github-summary", type=Path)
  return parser.parse_args()


def append_line(path: Path, line: str) -> None:
  with path.open("a", encoding="utf-8", newline="\n") as output:
    output.write(f"{line}\n")


def main() -> None:
  args = parse_args()
  candidate = args.candidate.resolve()
  if args.command == "classify-npm":
    if args.github_output is None:
      raise RuntimeError("classify-npm requires --github-output")
    publish_required = classify_npm_candidate(candidate, args.version, args.commit)
    value = str(publish_required).lower()
    append_line(args.github_output, f"publish_required={value}")
    if args.github_summary is not None:
      append_line(args.github_summary, f"- npm publication required: `{value}`")
    print(f"npm publication required: {value}")
    return

  if args.github_output is not None or args.github_summary is not None:
    raise RuntimeError("verify does not write GitHub step files")
  wait_for_candidate_registries(candidate, args.version, args.commit)
  print(f"Verified exact PyPI and npm registry bytes for {args.version}")


if __name__ == "__main__":
  main()
