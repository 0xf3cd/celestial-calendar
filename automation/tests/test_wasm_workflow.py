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

import json
import re
from collections import Counter
from pathlib import Path

import yaml

from toolbox.artifact_downloader import ARTIFACT_SOURCES


ROOT = Path(__file__).parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "wasm.yml"
JAVASCRIPT = ROOT / "bindings" / "javascript"
WASM_CHECK = ROOT / "toolbox" / "wasm_check.mjs"


def test_emsdk_source_is_pinned_even_on_cache_hits():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  env = workflow["env"]
  steps = workflow["jobs"]["wasm"]["steps"]
  cache_step = next(step for step in steps if step.get("name") == "Cache emsdk")
  install_step = next(step for step in steps if step.get("name") == "Install emsdk (cache miss only)")
  identity_step = next(step for step in steps if step.get("name") == "Verify emsdk source identity")

  assert env["EMSDK_VERSION"] == "6.0.6"
  assert env["EMSDK_COMMIT"] == "9981799f744be74ac67b1c1813ff172f63be0630"
  assert cache_step["with"]["key"] == "emsdk-${{ env.EMSDK_VERSION }}-${{ env.EMSDK_COMMIT }}-${{ runner.os }}"
  assert install_step["if"] == "steps.emsdk-cache.outputs.cache-hit != 'true'"
  assert 'git -C emsdk fetch --depth 1 origin "$EMSDK_COMMIT"' in install_step["run"]
  assert "git -C emsdk checkout --detach FETCH_HEAD" in install_step["run"]
  assert "if" not in identity_step
  commands = [line.strip() for line in identity_step["run"].splitlines() if line.strip()]
  assert commands == [
    "actual_commit=$(git -C emsdk rev-parse HEAD)",
    'if [ "$actual_commit" != "$EMSDK_COMMIT" ]; then',
    'echo "Unexpected emsdk commit: $actual_commit"',
    "exit 1",
    "fi",
    "if git -C emsdk symbolic-ref --quiet HEAD >/dev/null; then",
    'echo "emsdk checkout is not detached"',
    "exit 1",
    "fi",
  ]


def test_wasm_artifact_inventory_matches_collector():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  upload_jobs = [
    job
    for job in workflow["jobs"].values()
    if any(str(step.get("uses", "")).startswith("actions/upload-artifact@") for step in job["steps"])
  ]
  for job in upload_jobs:
    matrix = job.get("strategy", {}).get("matrix", {})
    assert set(matrix).isdisjoint({"include", "exclude"})
    assert all(len(values) == 1 for values in matrix.values())

  uploads = [
    step["with"]["name"]
    for job in upload_jobs
    for step in job["steps"]
    if str(step.get("uses", "")).startswith("actions/upload-artifact@")
  ]
  expected = next(names for name, names in ARTIFACT_SOURCES if name == workflow["name"])

  assert Counter(uploads) == Counter(expected)


def test_npm_publish_stays_a_dry_run():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  job = workflow["jobs"]["wasm"]
  steps = job["steps"]
  publish_steps = [step for step in steps if "npm publish" in step.get("run", "")]

  assert len(publish_steps) == 1
  publish_step = publish_steps[0]
  assert "NPM_CONFIG_DRY_RUN" not in workflow.get("env", {})
  assert "NPM_CONFIG_DRY_RUN" not in job.get("env", {})
  dry_run_steps = [step for step in steps if "NPM_CONFIG_DRY_RUN" in step.get("env", {})]
  assert dry_run_steps == [publish_step]
  assert publish_step.get("env", {}).get("NPM_CONFIG_DRY_RUN") == "true"
  commands = [line.strip() for line in publish_step["run"].splitlines() if line.strip()]
  assert commands == ['npm publish --dry-run "${{ steps.npm-package.outputs.tarball }}"']


def test_javascript_test_entries_match_their_execution_owners():
  test_root = JAVASCRIPT / "test"
  entries = {
    path.relative_to(test_root).as_posix()
    for pattern in ("abi/*.mjs", "node/*.mjs", "browser/*.mjs", "types/*.ts")
    for path in test_root.glob(pattern)
  }

  wasm_check = WASM_CHECK.read_text(encoding="utf-8")
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  package = json.loads((JAVASCRIPT / "package.json").read_text(encoding="utf-8"))
  tsconfig = json.loads((test_root / "types" / "tsconfig.json").read_text(encoding="utf-8"))
  workflow_commands = "\n".join(str(step.get("run", "")) for step in workflow["jobs"]["wasm"]["steps"])

  abi_entries = set(
    re.findall(
      r'^\s*await import\("\.\./bindings/javascript/test/(abi/[a-z0-9_.-]+\.mjs)"\);\s*$',
      wasm_check,
      re.MULTILINE,
    )
  )
  workflow_entries = set(
    re.findall(
      r"^\s*node\s+bindings/javascript/test/((?:node|browser)/[a-z0-9_.-]+\.mjs)(?:\s|$)",
      workflow_commands,
      re.MULTILINE,
    )
  )
  type_entries = {f"types/{path}" for path in tsconfig["include"]}

  assert entries == abi_entries | workflow_entries | type_entries
  assert abi_entries == {path for path in entries if path.startswith("abi/")}
  assert workflow_entries == {path for path in entries if path.startswith(("node/", "browser/"))}
  assert type_entries == {path for path in entries if path.startswith("types/")}
  assert package["scripts"]["test:types"] == "tsc --noEmit -p test/types/tsconfig.json"
  assert "node toolbox/wasm_check.mjs" in workflow_commands
  assert "npm run test:types --prefix bindings/javascript" in workflow_commands
