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

import re
from pathlib import Path

import yaml

from toolbox.artifact_downloader import PYTHON_ARTIFACTS
from toolbox.release_validation import SOURCE_SPECS


WORKFLOW = Path(__file__).parents[2] / ".github" / "workflows" / "python-wheel.yml"
PYTHON_TEST_ROOT = Path(__file__).parents[2] / "bindings" / "python" / "test"
WHEEL_TEST_ROOT = PYTHON_TEST_ROOT / "wheel"
TYPE_TEST_ROOT = PYTHON_TEST_ROOT / "types"


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

  expected = next(names for _field, name, names in SOURCE_SPECS if name == "Python Wheels")
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
  assert "CIBW_TEST_COMMAND" not in workflow["env"]
  text = WORKFLOW.read_text(encoding="utf-8").replace("\\", "/")
  acceptance_scripts = {
    path.relative_to(PYTHON_TEST_ROOT).as_posix()
    for directory in ("abi", "consumer")
    for path in (PYTHON_TEST_ROOT / directory).glob("*.py")
  }
  assert all(f"test/{script}" not in text for script in acceptance_scripts)
  assert text.count("test/run_all.py") == 6


def test_python_wheel_floor_consumers_are_offline_or_artifact_only():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  jobs = workflow["jobs"]

  linux = next(
    step for step in jobs["manylinux"]["steps"] if step.get("name") == "Test exact wheel on Python 3.11 floor"
  )
  macos = next(
    step for step in jobs["macos-14-floor"]["steps"] if step.get("name") == "Clean-install and test on macOS 14"
  )
  windows = next(
    step for step in jobs["windows-amd64"]["steps"] if step.get("name") == "Test exact wheel on Python 3.11.9"
  )

  assert "--network none" in linux["run"]
  assert "/opt/python/cp311-cp311/bin/python" in linux["run"]
  for step in (linux, macos, windows):
    command = step["run"].replace("\\", "/")
    assert "pip install --no-deps" in command
    assert "test/run_all.py" in command


def test_python_wheel_scripts_and_references_match():
  text = WORKFLOW.read_text(encoding="utf-8").replace("\\", "/")
  referenced = set(re.findall(r"bindings/python/test/wheel/([A-Za-z0-9_-]+\.py)", text))
  discovered = {path.name for path in WHEEL_TEST_ROOT.glob("*.py")}
  assert referenced == discovered


def test_python_wheel_producers_verify_the_bootstrap_pip_is_embedded():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  commands = [
    step["run"]
    for job_name in ("manylinux", "macos-arm64", "windows-amd64")
    for step in workflow["jobs"][job_name]["steps"]
    if step.get("name") == "Verify cibuildwheel bootstrap"
  ]
  assert (
    commands
    == ["python bindings/python/test/wheel/verify_bootstrap.py bindings/python/constraints-cibuildwheel.txt"] * 3
  )


def test_python_wheel_typing_contract_is_pinned_and_wired():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  step = next(
    step for step in workflow["jobs"]["manylinux"]["steps"] if step.get("name") == "Verify installed typing contract"
  )
  command = step["run"].replace("\\", "/")
  referenced = set(re.findall(r"bindings/python/test/types/([A-Za-z0-9_-]+\.py)", command))
  discovered = {path.name for path in TYPE_TEST_ROOT.glob("*.py")}

  assert step["if"] == "matrix.identifier == 'cp311-manylinux_x86_64'"
  assert (
    "current-venv/bin/python -m pip install --require-hashes --only-binary :all: "
    "-r bindings/python/requirements-mypy.txt"
  ) in command
  assert referenced == discovered == {"consumer.py"}


def test_python_wheel_platform_toolchains_are_explicit():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  jobs = workflow["jobs"]
  floor_python = next(
    step["with"]["python-version"]
    for step in jobs["macos-14-floor"]["steps"]
    if str(step.get("uses", "")).startswith("actions/setup-python@")
  )

  assert floor_python == "3.11.9"
  assert workflow["env"]["CIBW_ENVIRONMENT_WINDOWS"] == (
    "CC=clang CXX=clang++ CMAKE_GENERATOR=Ninja PIP_REQUIRE_HASHES=1 PIP_ONLY_BINARY=:all:"
  )
