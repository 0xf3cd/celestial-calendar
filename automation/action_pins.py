# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import re

from pathlib import Path
from typing import Final, Iterable

from . import paths
from .utils import green_print, red_print, yellow_print


SHA: Final[re.Pattern] = re.compile(r"^[0-9a-f]{40}$")
MAJOR_TAG: Final[re.Pattern] = re.compile(r"^v[1-9][0-9]*$")


def _uses_nodes(node, yaml, flow_end=None) -> Iterable:
  cutoff = flow_end or (node.end_mark if getattr(node, "flow_style", False) else None)
  if isinstance(node, yaml.MappingNode):
    for key, value in node.value:
      if isinstance(key, yaml.ScalarNode) and key.value == "uses":
        yield value, cutoff or value.end_mark
      yield from _uses_nodes(value, yaml, cutoff)
  elif isinstance(node, yaml.SequenceNode):
    for value in node.value:
      yield from _uses_nodes(value, yaml, cutoff)


def check_action_pins(workflow_dir: Path | None = None) -> int:
  """Require major tags or SHAs for actions/*, SHAs plus release/tag provenance for third-party actions."""
  try:
    import yaml
  except ModuleNotFoundError:
    red_print("This check needs PyYAML: run `pip install -r Requirements.txt` first")
    return 1

  print("#" * 60)
  yellow_print("Checking GitHub Action pins across all workflows...")
  workflow_dir = workflow_dir or paths.proj_root() / ".github" / "workflows"
  workflow_paths = sorted((*workflow_dir.rglob("*.yml"), *workflow_dir.rglob("*.yaml")))
  failures = []
  if not workflow_paths:
    failures.append(f"{workflow_dir}: no workflow files found")

  for path in workflow_paths:
    display_path = path.relative_to(workflow_dir)
    text = path.read_text(encoding="utf-8")
    try:
      workflow = yaml.compose(text)
    except yaml.YAMLError as error:
      failures.append(f"{display_path}: invalid YAML ({error.__class__.__name__})")
      continue
    if workflow is None:
      failures.append(f"{display_path}: empty workflow")
      continue

    lines = text.splitlines()
    for node, cutoff in _uses_nodes(workflow, yaml):
      line_number = node.start_mark.line + 1
      reference = node.value if isinstance(node, yaml.ScalarNode) else ""
      if reference.startswith("./"):
        continue
      target, separator, ref = reference.rpartition("@")
      label = f"{display_path}:{line_number}: `{reference}`"
      if not separator or not target or not ref:
        failures.append(f"{label} is not an action pinned to a ref")
        continue
      if SHA.fullmatch(ref):
        if target.startswith("actions/"):
          continue
        source_line = lines[node.start_mark.line]
        has_provenance = cutoff.line == node.start_mark.line and (
          re.match(r"^\s+#\s*\S", source_line[cutoff.column :]) is not None
        )
        if not has_provenance:
          if cutoff.line != node.start_mark.line:
            failures.append(f"{label} uses a multi-line YAML node; rewrite it as plain `uses: <target>@<SHA> # v2`")
          else:
            failures.append(f"{label} needs a trailing release/tag provenance comment (e.g. `# v2`)")
        continue
      if target.startswith("actions/") and MAJOR_TAG.fullmatch(ref):
        continue
      policy = "a major tag or commit SHA" if target.startswith("actions/") else "a 40-character commit SHA"
      failures.append(f"{label} must use {policy}")

  print("#" * 60)
  if failures:
    red_print(f"Action-pin gate failed ({len(failures)} finding(s)):")
    for failure in failures:
      red_print(f"  - {failure}")
    return 1
  green_print(f"GitHub Action pins satisfy policy ({len(workflow_paths)} files)")
  return 0
