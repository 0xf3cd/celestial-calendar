# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import fnmatch
import json
import os
import subprocess
from pathlib import Path

import pytest

from automation.workflow_actions import load_workflow, resolved_steps
from toolbox.runtime_floor import SUPPORTED_RUNTIME


REPO = Path(__file__).resolve().parents[2]
WORKFLOWS = REPO / ".github/workflows"
ACTIONS = REPO / ".github/actions"


def assert_caller_environment(action):
  assert "inputs" not in action or set(action["inputs"]) == {"python-version"}
  assert "env" not in action
  for step in action["runs"]["steps"]:
    assert "env" not in step
    command = step.get("run", "")
    assert "CELESTIAL_" not in command and "WINDOWS_EVIDENCE_MODE" not in command
    assert "GITHUB_ENV" not in command


def assert_windows_encoding(command):
  lines = [line.strip() for line in command.splitlines()]
  assert "chcp.com 65001" in lines
  assert '$env:PYTHONIOENCODING = "utf-8"' in lines


def assert_soak_wiring(workflow):
  assert workflow["env"] == {"CELESTIAL_TEST_SEED": "${{ github.run_id }}"}
  for name, job in workflow["jobs"].items():
    if not name.startswith("soak-"):
      continue
    assert "strategy" not in job
    assert "env" not in job
    builds = [step for step in job["steps"] if step.get("uses") == "./.github/actions/native-build"]
    assert len(builds) == 1
    assert "CELESTIAL_TEST_SEED" not in builds[0].get("env", {})
    verification = next(step for step in job["steps"] if step.get("name") == "Verify received soak seed")
    assert "2>&1" in verification["run"]
    assert "GITHUB_RUN_ID" in verification["run"]
    assert "Earth.ObliquityJ2000" in verification["run"]
    assert "earth_test" in verification["run"]
    assert job["steps"].index(builds[0]) < job["steps"].index(verification)
    assert verification["shell"] == ("pwsh" if "windows" in name else "bash")
    if verification["shell"] == "pwsh":
      lines = [line.strip() for line in verification["run"].splitlines() if line.strip()]
      invoked = lines.index("$output = & ./build/test/earth_test.exe --gtest_filter=Earth.ObliquityJ2000 2>&1")
      captured = lines.index("$code = $LASTEXITCODE")
      propagated = lines.index("if ($code -ne 0) { exit $code }")
      checked = lines.index(
        'if ($lines -notcontains "[ util::random ] seed = $env:GITHUB_RUN_ID (override: CELESTIAL_TEST_SEED)") {'
      )
      assert invoked < captured < propagated < checked
      assert lines[checked + 1].startswith("throw ")


def test_native_composites_inherit_seed_and_evidence_without_command_inputs():
  for path in ACTIONS.glob("*/action.yml"):
    assert_caller_environment(load_workflow(path))
  build = load_workflow(ACTIONS / "native-build/action.yml")
  windows = next(step for step in build["runs"]["steps"] if step["shell"] == "pwsh")
  assert_windows_encoding(windows["run"])


@pytest.mark.parametrize("guard", ["chcp.com 65001", '$env:PYTHONIOENCODING = "utf-8"'])
def test_windows_native_build_rejects_missing_utf8_guards(guard):
  build = load_workflow(ACTIONS / "native-build/action.yml")
  windows = next(step for step in build["runs"]["steps"] if step["shell"] == "pwsh")
  assert windows["run"].count(guard) == 1
  command = windows["run"].replace(guard, "")
  with pytest.raises(AssertionError):
    assert_windows_encoding(command)


@pytest.mark.parametrize("fault", ["seed-env", "evidence-env", "seed-run", "evidence-run", "export", "command-input"])
def test_shared_action_contract_rejects_seed_or_evidence_overrides(fault):
  action = load_workflow(ACTIONS / "native-build/action.yml")
  step = action["runs"]["steps"][0]
  if fault.endswith("env"):
    key = "CELESTIAL_TEST_SEED" if fault == "seed-env" else "CELESTIAL_WINDOWS_EVIDENCE_DIR"
    step["env"] = {key: ""}
  elif fault == "command-input":
    action["inputs"] = {"command": {"required": "true"}}
  else:
    step["run"] += {
      "seed-run": "\nunset CELESTIAL_TEST_SEED",
      "evidence-run": "\n$env:WINDOWS_EVIDENCE_MODE = ''",
      "export": '\necho "VALUE=" >> "$GITHUB_ENV"',
    }[fault]
  with pytest.raises(AssertionError):
    assert_caller_environment(action)


def test_native_build_receives_caller_environment(tmp_path):
  action = load_workflow(ACTIONS / "native-build/action.yml")
  command = action["runs"]["steps"][0]["run"]
  workflow = load_workflow(WORKFLOWS / "build_and_test.yml")
  evidence = workflow["jobs"]["windows"]["env"]
  expected = {**evidence, "CELESTIAL_TEST_SEED": "1234567", "CXX": "caller-compiler"}
  (tmp_path / "project.py").write_text(
    "import json, os\nprint(json.dumps({key: os.environ.get(key) for key in " + repr(list(expected)) + "}))\n",
    encoding="utf-8",
  )
  result = subprocess.run(
    ["bash", "--noprofile", "--norc", "-euo", "pipefail", "-c", command],
    cwd=tmp_path,
    env={**os.environ, **expected},
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  assert json.loads(result.stdout) == expected


def test_soak_seed_and_platform_wiring():
  workflow = load_workflow(WORKFLOWS / "random_soak.yml")
  assert_soak_wiring(workflow)
  assert set(workflow["on"]) == {"schedule", "workflow_dispatch"}
  macos = workflow["jobs"]["soak-macos-apple-clang"]
  build = next(step for step in macos["steps"] if step.get("uses") == "./.github/actions/native-build")
  assert build["env"]["MACOSX_DEPLOYMENT_TARGET"] == SUPPORTED_RUNTIME["macos_arm64"]["macos"]


@pytest.mark.parametrize("fault", ["lost", "misspelled", "empty", "shadowed"])
def test_soak_contract_rejects_override_wiring_faults(fault):
  workflow = load_workflow(WORKFLOWS / "random_soak.yml")
  if fault == "lost":
    del workflow["env"]["CELESTIAL_TEST_SEED"]
  elif fault == "misspelled":
    workflow["env"]["CELESTIAL_TEST_SED"] = workflow["env"].pop("CELESTIAL_TEST_SEED")
  elif fault == "empty":
    workflow["env"]["CELESTIAL_TEST_SEED"] = ""
  else:
    workflow["jobs"]["soak-ubuntu-clang"]["env"] = {"CELESTIAL_TEST_SEED": ""}
  with pytest.raises(AssertionError):
    assert_soak_wiring(workflow)


@pytest.mark.parametrize("fault", ["capture-before-run", "exit", "prefix", "variable-name", "throw"])
def test_windows_soak_contract_rejects_weakened_failure_or_seed_checks(fault):
  workflow = load_workflow(WORKFLOWS / "random_soak.yml")
  verification = next(
    step for step in workflow["jobs"]["soak-windows-clang"]["steps"] if step.get("name") == "Verify received soak seed"
  )
  old, new = {
    "capture-before-run": (
      "$output = & ./build/test/earth_test.exe --gtest_filter=Earth.ObliquityJ2000 2>&1\n$code = $LASTEXITCODE",
      "$code = $LASTEXITCODE\n$output = & ./build/test/earth_test.exe --gtest_filter=Earth.ObliquityJ2000 2>&1",
    ),
    "exit": ("if ($code -ne 0) { exit $code }", ""),
    "prefix": (
      '"[ util::random ] seed = $env:GITHUB_RUN_ID (override: CELESTIAL_TEST_SEED)"',
      '"seed = $env:GITHUB_RUN_ID"',
    ),
    "variable-name": (
      '"[ util::random ] seed = $env:GITHUB_RUN_ID (override: CELESTIAL_TEST_SEED)"',
      '"GITHUB_RUN_ID"',
    ),
    "throw": ('throw "The RNG binary did not receive the soak run-ID seed."', ""),
  }[fault]
  assert verification["run"].count(old) == 1
  verification["run"] = verification["run"].replace(old, new)
  with pytest.raises((AssertionError, ValueError)):
    assert_soak_wiring(workflow)


@pytest.mark.parametrize("job_name", ["soak-ubuntu-clang", "soak-macos-apple-clang"])
@pytest.mark.parametrize("fault", ["none", "lost", "misspelled", "empty", "binary-failure"])
def test_bash_soak_assertion_checks_received_stderr_not_just_the_environment(tmp_path, job_name, fault):
  workflow = load_workflow(WORKFLOWS / "random_soak.yml")
  verification = next(
    step for step in workflow["jobs"][job_name]["steps"] if step.get("name") == "Verify received soak seed"
  )
  binary = tmp_path / "build/test/earth_test"
  binary.parent.mkdir(parents=True)
  binary.write_text(
    '#!/bin/bash\necho "[ util::random ] seed = ${CELESTIAL_TEST_SEED:-fallback} (override: CELESTIAL_TEST_SEED)" >&2\n'
    + ("exit 7\n" if fault == "binary-failure" else ""),
    encoding="utf-8",
  )
  binary.chmod(0o755)
  env = {key: value for key, value in os.environ.items() if not key.startswith("CELESTIAL_TEST_SE")}
  env["GITHUB_RUN_ID"] = "1234567"
  if fault != "lost":
    key = "CELESTIAL_TEST_SED" if fault == "misspelled" else "CELESTIAL_TEST_SEED"
    env[key] = "" if fault == "empty" else "1234567"
  result = subprocess.run(
    ["bash", "--noprofile", "--norc", "-euo", "pipefail", "-c", verification["run"]],
    cwd=tmp_path,
    env=env,
    capture_output=True,
    text=True,
  )
  assert (result.returncode == 0) is (fault == "none")
  if fault == "binary-failure":
    assert result.returncode == 7


def assert_required_job_shapes(core, native):
  singleton_jobs = [
    job
    for workflow in (core, native)
    for job in workflow["jobs"].values()
    if set(job.get("strategy", {}).get("matrix", {})) == {"python-version"}
  ]
  assert len(singleton_jobs) == 6
  assert all(job["strategy"]["matrix"]["python-version"] == ["3.12"] for job in singleton_jobs)
  for job in core["jobs"].values():
    assert job["if"] == "github.event_name == 'push' || github.event.pull_request.head.repo.fork"
    if "strategy" in job:
      assert "name" not in job
      assert job["strategy"]["matrix"]


def test_required_runtime_matrices_and_conditioned_skip_separation_remain():
  core = load_workflow(WORKFLOWS / "core_tests.yml")
  native = load_workflow(WORKFLOWS / "build_and_test.yml")
  assert_required_job_shapes(core, native)
  assert "CELESTIAL_TEST_SEED" not in json.dumps(core)
  assert "CELESTIAL_TEST_SEED" not in json.dumps(native)
  assert "CELESTIAL_TEST_SEED" not in (REPO / "Dockerfile").read_text(encoding="utf-8")


@pytest.mark.parametrize("fault", ["drop-matrix", "explicit-name", "condition"])
def test_required_job_shape_controls_reject_matrix_or_skip_name_changes(fault):
  for filename in ("core_tests.yml", "build_and_test.yml"):
    original = load_workflow(WORKFLOWS / filename)
    names = [
      name
      for name, job in original["jobs"].items()
      if set(job.get("strategy", {}).get("matrix", {})) == {"python-version"}
    ]
    for name in names:
      core = load_workflow(WORKFLOWS / "core_tests.yml")
      native = load_workflow(WORKFLOWS / "build_and_test.yml")
      job = (core if filename == "core_tests.yml" else native)["jobs"][name]
      if fault == "drop-matrix":
        del job["strategy"]
      elif filename != "core_tests.yml":
        continue
      elif fault == "explicit-name":
        job["name"] = name + " (3.12)"
      else:
        job["if"] = "false"
      with pytest.raises(AssertionError):
        assert_required_job_shapes(core, native)


def test_per_pr_and_weekly_feature_jobs_have_distinct_names_and_the_same_legs():
  core = load_workflow(WORKFLOWS / "core_tests.yml")
  soak = load_workflow(WORKFLOWS / "random_soak.yml")
  per_pr = core["jobs"]["cxx-feature-probe"]
  weekly = soak["jobs"]["weekly-cxx-feature-probe"]
  assert len(per_pr["strategy"]["matrix"]["include"]) == 3
  assert weekly["strategy"]["matrix"] == per_pr["strategy"]["matrix"]
  assert "if" not in weekly and "name" not in weekly
  assert "weekly-cxx-feature-probe" not in core["jobs"]
  assert "cxx-feature-probe" not in soak["jobs"]
  for job in (per_pr, weekly):
    steps = resolved_steps(job["steps"], REPO)
    probes = [step for step in steps if "--features" in step.get("run", "")]
    assert len(probes) == 1 and "if" not in probes[0]
    assert probes[0]["env"] == {"CXX": "${{ matrix.cxx }}"}
    assert probes[0]["run"] == "python ./checks.py --features ${{ matrix.leg }}"


def test_macos_header_and_feature_jobs_select_the_native_xcode_major():
  core = load_workflow(WORKFLOWS / "core_tests.yml")
  toolchain = load_workflow(ACTIONS / "native-toolchain/action.yml")
  selected = next(
    step["with"] for step in toolchain["runs"]["steps"] if step.get("uses", "").startswith("maxim-lobanov/setup-xcode@")
  )
  for name in ("self-contained-headers", "cxx-feature-probe"):
    selection = next(
      step for step in core["jobs"][name]["steps"] if step.get("uses", "").startswith("maxim-lobanov/setup-xcode@")
    )
    assert selection["with"] == selected
    assert selection["if"] == "runner.os == 'macOS'"


def test_dependabot_covers_every_actual_composite_directory():
  config = load_workflow(REPO / ".github/dependabot.yml")
  update = next(entry for entry in config["updates"] if entry["package-ecosystem"] == "github-actions")
  directories = update.get("directories", [update.get("directory")])
  for action in ACTIONS.glob("*/action.yml"):
    directory = "/" + action.parent.relative_to(REPO).as_posix()
    assert any(fnmatch.fnmatchcase(directory, pattern) for pattern in directories)
