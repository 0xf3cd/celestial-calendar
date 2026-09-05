# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import pytest
import yaml

from automation import linter
from automation.utils import ProcReturn


@pytest.mark.parametrize(
  ("retcodes", "expected_commands", "expected_result"),
  [
    ([0, 0], 2, 0),
    ([1], 1, 1),
    ([2], 1, 2),
    ([0, 1], 2, 1),
    ([0, 2], 2, 2),
  ],
)
def test_run_ruff_checks_lint_and_format(monkeypatch, tmp_path, retcodes, expected_commands, expected_result):
  results = iter(retcodes)
  commands = []

  monkeypatch.setattr(linter, "check_tool", lambda _tool: True)
  monkeypatch.setattr(linter.paths, "proj_root", lambda: tmp_path)
  monkeypatch.setattr(
    linter,
    "run_cmd",
    lambda command: commands.append(command) or ProcReturn(next(results), "", ""),
  )

  assert linter.run_ruff() == expected_result
  assert (
    commands
    == [
      ["ruff", "check", str(tmp_path)],
      ["ruff", "format", "--check", str(tmp_path)],
    ][:expected_commands]
  )


def test_core_tests_pin_ruff_and_run_repo_entrypoint():
  workflow_path = linter.paths.proj_root() / ".github" / "workflows" / "core_tests.yml"
  workflow = yaml.safe_load(workflow_path.read_text(encoding="utf-8"))
  steps = workflow["jobs"]["linters-and-static-analysis"]["steps"]
  install = next(step for step in steps if step.get("name") == "Install Python Dependencies")
  style = next(step for step in steps if step.get("name") == "Style Check")

  assert "python3 -m pip install ruff==0.16.1" in install["run"].splitlines()
  assert "./checks.py --ruff --clang-tidy --abi-layout" in [line.strip() for line in style["run"].splitlines()]
