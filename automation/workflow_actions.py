# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

from pathlib import Path


def load_workflow(path: Path) -> dict:
  """Keep YAML scalars as text, including `on`, booleans and version numbers."""
  import yaml

  workflow = yaml.load(path.read_text(encoding="utf-8"), Loader=yaml.BaseLoader)
  if not isinstance(workflow, dict):
    raise ValueError(f"{path.name}: expected a YAML mapping")
  return workflow


def workflow_root(path: Path) -> Path:
  return path.parent.parent.parent if path.parent.name == "workflows" else path.parent


def local_action(repo_root: Path, reference: str) -> Path:
  """Resolve a repository-local composite without traversal or symlink escape."""
  relative = reference.removeprefix("./")
  if not reference.startswith("./") or any(part in relative for part in ("..", "@", "${{", "\\")):
    raise ValueError(f"unsupported local action: {reference}")
  directory = (repo_root / relative).resolve()
  if not directory.is_relative_to(repo_root.resolve()):
    raise ValueError(f"local action escapes repository: {reference}")
  definitions = [directory / name for name in ("action.yml", "action.yaml") if (directory / name).is_file()]
  if len(definitions) != 1:
    raise ValueError(f"{reference}: expected exactly one action.yml or action.yaml")
  path = definitions[0]
  if not path.resolve().is_relative_to(repo_root.resolve()):
    raise ValueError(f"local action definition escapes repository: {reference}")
  return path


def resolved_steps(steps: list, repo_root: Path, ancestors: tuple[Path, ...] = ()) -> list[dict]:
  """Include local call sites and all reached composite steps; reject cycles."""
  result = []
  for step in steps:
    if isinstance(step, list):
      result.extend(resolved_steps(step, repo_root, ancestors))
      continue
    if not isinstance(step, dict):
      raise ValueError("expected a step mapping")
    result.append(step)
    reference = step.get("uses", "")
    if not isinstance(reference, str):
      raise ValueError("expected a scalar action reference")
    if not reference.startswith("./"):
      continue
    path = local_action(repo_root, reference)
    if path in ancestors:
      raise ValueError(f"cyclic local action: {reference}")
    action = load_workflow(path)
    runs = action.get("runs", {})
    if runs.get("using") != "composite" or not isinstance(runs.get("steps"), list):
      raise ValueError(f"{reference}: only composite actions with steps are supported")
    result.extend(resolved_steps(runs["steps"], repo_root, (*ancestors, path)))
  return result
