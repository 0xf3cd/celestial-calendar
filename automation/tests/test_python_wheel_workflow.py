# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

from pathlib import Path

import yaml

from toolbox.artifact_downloader import ARTIFACT_SOURCES, PYTHON_ARTIFACTS


WORKFLOW = Path(__file__).parents[2] / ".github" / "workflows" / "python-wheel.yml"


def test_python_wheel_artifact_inventory_is_exact():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  uploaded = set()
  for job in workflow["jobs"].values():
    matrix_artifacts = {
      entry["artifact"] for entry in job.get("strategy", {}).get("matrix", {}).get("include", []) if "artifact" in entry
    }
    for step in job["steps"]:
      if str(step.get("uses", "")).startswith("actions/upload-artifact@"):
        name = step["with"]["name"]
        uploaded.update(matrix_artifacts if name == "${{ matrix.artifact }}" else {name})

  expected = next(names for name, names in ARTIFACT_SOURCES if name == "Python Wheels")
  assert workflow["name"] == "Python Wheels"
  assert uploaded == set(PYTHON_ARTIFACTS) == expected


def test_python_wheel_workflow_never_publishes():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  commands = "\n".join(str(step.get("run", "")) for job in workflow["jobs"].values() for step in job["steps"]).lower()
  actions = {str(step.get("uses", "")).lower() for job in workflow["jobs"].values() for step in job["steps"]}

  assert workflow["permissions"] == {"contents": "read"}
  assert "id-token" not in workflow["permissions"]
  assert "twine" not in commands
  assert "--sdist" not in commands
  assert all("pypa/gh-action-pypi-publish" not in action for action in actions)


def test_python_wheel_acceptance_has_one_entry_point():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  assert workflow["env"]["CIBW_TEST_COMMAND"] == "python {package}/test/run_all.py"
  text = WORKFLOW.read_text(encoding="utf-8")
  assert "test/abi/verify.py" not in text
  assert "test/abi/raw_protocol.py" not in text
  assert "test/consumer/smoke.py" not in text
  assert text.replace("\\", "/").count("test/run_all.py") == 5


def test_python_wheel_platform_toolchains_are_explicit():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  jobs = workflow["jobs"]
  floor_python = next(
    step["with"]["python-version"]
    for step in jobs["macos-14-floor"]["steps"]
    if str(step.get("uses", "")).startswith("actions/setup-python@")
  )

  assert floor_python == "3.11.9"
  assert jobs["windows-amd64"]["env"]["CIBW_ENVIRONMENT_WINDOWS"] == ("CC=clang CXX=clang++ CMAKE_GENERATOR=Ninja")
