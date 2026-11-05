# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import os
import re
import subprocess
from pathlib import Path

from toolbox.docs_links import REPOSITORY, pin_package_links, pinned_source


ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA_ENV = "CELESTIAL_DOCS_SOURCE_SHA"
CONTEXT_START = "<!-- package-context -->"
CONTEXT_END = "<!-- /package-context -->"


def source_sha(root: Path = ROOT) -> str | None:
  """Return the producer or clean-checkout SHA, or None for a developmental local build."""
  supplied = os.environ.get(SOURCE_SHA_ENV)
  if supplied is not None and re.fullmatch(r"[0-9a-f]{40}", supplied) is None:
    raise ValueError(f"{SOURCE_SHA_ENV} must be a 40-character lowercase hexadecimal SHA")
  if not os.path.lexists(root / ".git"):
    return supplied
  try:
    actual_root = subprocess.check_output(
      ["git", "rev-parse", "--show-toplevel"], cwd=root, text=True, stderr=subprocess.PIPE
    ).strip()
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True, stderr=subprocess.PIPE).strip()
    dirty = subprocess.check_output(
      ["git", "status", "--porcelain", "--untracked-files=no"], cwd=root, text=True, stderr=subprocess.PIPE
    )
    untracked = subprocess.check_output(
      ["git", "ls-files", "--others", "--exclude-standard", "--", "src", "bindings", "toolbox", "docs", "project.py"],
      cwd=root,
      text=True,
      stderr=subprocess.PIPE,
    )
  except (OSError, subprocess.CalledProcessError):
    return supplied
  if Path(actual_root).resolve() != root.resolve():
    raise ValueError("package documentation source must be the exact repository root")
  if supplied is not None and (head != supplied or dirty or untracked):
    raise ValueError("package documentation requires a clean checkout matching the supplied SHA")
  return supplied or (head if not dirty and not untracked else None)


def documentation_url(language: str, sha: str | None) -> str:
  if language not in ("python", "javascript"):
    raise ValueError(f"unknown package documentation language: {language}")
  guide = f"bindings/{language}/README.md"
  return pinned_source(guide, sha) if sha is not None else f"{REPOSITORY}/blob/main/{guide}"


def python_urls(sha: str | None) -> dict[str, str]:
  return {
    "Homepage": REPOSITORY,
    "Documentation": documentation_url("python", sha),
    "Repository": REPOSITORY,
    "Issues": f"{REPOSITORY}/issues",
  }


def render_readme(text: str, version: str, sha: str | None) -> str:
  """Generate only package context and recognized documentation URLs from the source guide."""
  if re.fullmatch(r"\d+\.\d+\.\d+", version) is None:
    raise ValueError(f"invalid package documentation version: {version}")
  if text.count(CONTEXT_START) != 1 or text.count(CONTEXT_END) != 1:
    raise ValueError("package README must contain exactly one package-context block")
  before, remainder = text.split(CONTEXT_START)
  _, after = remainder.split(CONTEXT_END)
  context = (
    f"Documentation for package version {version} at [source {sha}]({REPOSITORY}/tree/{sha})."
    if sha is not None
    else f"Unreleased local build (package version {version}); documentation follows the current source."
  )
  result = before + context + after
  return pin_package_links(result, sha) if sha is not None else result


def dynamic_metadata(settings: dict, project: dict) -> dict:
  """Supply the primary wheel's README and URLs after its version provider has run."""
  if settings:
    raise ValueError("package documentation provider accepts no fixed source settings")
  sha = source_sha()
  return {
    "readme": {
      "text": render_readme((ROOT / "bindings/python/README.md").read_text(encoding="utf-8"), project["version"], sha),
      "content-type": "text/markdown",
    },
    "urls": python_urls(sha),
  }
