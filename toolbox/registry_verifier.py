#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
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

from toolbox.registry_validation import (
  NPM_LABELS,
  classify_npm_candidate,
  prepare_pypi_candidate,
  wait_for_candidate_registries,
)
from toolbox.build_python_aliases import PYPI_PACKAGES
from toolbox.release_validation import npm_candidate_tarballs, npm_package_metadata


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description="Validate exact npm and PyPI release artifacts.")
  parser.add_argument("command", choices=("classify-npm", "prepare-pypi", "verify"))
  parser.add_argument("--candidate", type=Path, required=True)
  parser.add_argument("--version", required=True)
  parser.add_argument("--commit", required=True)
  parser.add_argument("--package", choices=tuple(dict.fromkeys([*NPM_LABELS, *PYPI_PACKAGES])))
  parser.add_argument("--upload-to", type=Path)
  parser.add_argument("--github-output", type=Path)
  parser.add_argument("--github-summary", type=Path)
  return parser.parse_args()


def append_line(path: Path, line: str) -> None:
  with path.open("a", encoding="utf-8", newline="\n") as output:
    output.write(f"{line}\n")


def main() -> None:
  args = parse_args()
  candidate = args.candidate.resolve()
  if args.command == "prepare-pypi":
    if args.github_output is None or args.package not in PYPI_PACKAGES or args.upload_to is None:
      raise RuntimeError("prepare-pypi requires --package, --upload-to and --github-output")
    state = prepare_pypi_candidate(candidate, args.version, args.commit, args.package, args.upload_to)
    if state not in {"absent", "partial", "exact"}:
      raise RuntimeError(f"Invalid PyPI classification: {state}")
    append_line(args.github_output, f"state={state}")
    append_line(args.github_output, f"packages-dir={args.upload_to.resolve()}")
    if args.github_summary is not None:
      append_line(args.github_summary, f"- PyPI {args.package}: `{state}`")
    print(f"PyPI {args.package}: {state}")
    return
  if args.command == "classify-npm":
    if args.github_output is None or args.package not in NPM_LABELS or getattr(args, "upload_to", None) is not None:
      raise RuntimeError("classify-npm requires --package and --github-output")
    label = NPM_LABELS[args.package]
    try:
      state = classify_npm_candidate(candidate, args.version, args.commit, args.package)
    except Exception as error:
      raise RuntimeError(f"{label} classification failed: {error}") from error
    tarball = npm_candidate_tarballs(candidate, args.version)[args.package]
    append_line(args.github_output, f"state={state}")
    append_line(args.github_output, f"tarball={tarball}")
    if args.github_summary is not None:
      append_line(args.github_summary, f"- {label} ({args.package}): `{state}`")
    print(f"{label} ({args.package}): {state}")
    return

  if (
    args.github_output is not None
    or args.github_summary is not None
    or args.package is not None
    or getattr(args, "upload_to", None) is not None
  ):
    raise RuntimeError("verify takes no package selector or GitHub step files")
  wait_for_candidate_registries(candidate, args.version, args.commit)
  labels = ", ".join(NPM_LABELS[name] for name in npm_package_metadata(args.version))
  print(f"Verified exact PyPI, {labels} registry bytes for {args.version}")


if __name__ == "__main__":
  main()
