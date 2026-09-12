# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import json
import re
import shlex
import subprocess
import sys
import shutil
from collections import Counter
from pathlib import Path

import yaml
import pytest

from toolbox import build_npm
from toolbox.release_validation import SOURCE_SPECS


ROOT = Path(__file__).parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "wasm.yml"
RELEASE_WORKFLOW = ROOT / ".github" / "workflows" / "release.yml"
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
  expected = next(names for _field, name, names in SOURCE_SPECS if name == workflow["name"])

  assert Counter(uploads) == Counter(expected)


def test_wasm_workflow_never_publishes_to_npm():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  job = workflow["jobs"]["wasm"]
  steps = job["steps"]
  commands = "\n".join(str(step.get("run", "")) for step in steps)

  assert "npm publish" not in commands
  assert "NPM_CONFIG_DRY_RUN" not in workflow.get("env", {})
  assert "NPM_CONFIG_DRY_RUN" not in job.get("env", {})
  assert all("NPM_CONFIG_DRY_RUN" not in step.get("env", {}) for step in steps)


def test_javascript_test_entries_match_their_execution_owners():
  test_root = JAVASCRIPT / "test"
  assert {path.name for path in test_root.iterdir() if path.is_dir()} == {
    "abi",
    "browser",
    "node",
    "registry",
    "support",
    "types",
  }
  assert {path.relative_to(test_root).as_posix() for path in test_root.glob("support/*.mjs")} == {
    "support/package_consumer.mjs"
  }
  entries = {
    path.relative_to(test_root).as_posix()
    for pattern in ("abi/*.mjs", "node/*.mjs", "browser/*.mjs", "registry/*.mjs", "types/*.ts")
    for path in test_root.glob(pattern)
  }

  wasm_check = WASM_CHECK.read_text(encoding="utf-8")
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  release_workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  package = json.loads((JAVASCRIPT / "package.json").read_text(encoding="utf-8"))
  tsconfig = json.loads((test_root / "types" / "tsconfig.json").read_text(encoding="utf-8"))
  workflow_commands = "\n".join(str(step.get("run", "")) for step in workflow["jobs"]["wasm"]["steps"])
  release_commands = "\n".join(
    str(step.get("run", "")) for job in release_workflow["jobs"].values() for step in job["steps"]
  )

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
  registry_entries = set(
    re.findall(
      r"^\s*node\s+bindings/javascript/test/(registry/[a-z0-9_.-]+\.mjs)(?:\s|$)",
      release_commands,
      re.MULTILINE,
    )
  )
  type_entries = {f"types/{path}" for path in tsconfig["include"]}

  assert entries == abi_entries | workflow_entries | registry_entries | type_entries
  assert abi_entries == {path for path in entries if path.startswith("abi/")}
  assert workflow_entries == {path for path in entries if path.startswith(("node/", "browser/"))}
  assert registry_entries == {path for path in entries if path.startswith("registry/")}
  assert type_entries == {path for path in entries if path.startswith("types/")}
  assert package["scripts"]["test:types"] == "tsc --noEmit -p test/types/tsconfig.json"
  assert "node toolbox/wasm_check.mjs" in workflow_commands
  assert "npm run test:types --prefix bindings/javascript" in workflow_commands


def test_date_bridge_runs_on_current_and_floor_node():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  entry = "bindings/javascript/test/node/date_test.mjs"
  command = ["node", entry, "build/npm/package/date.mjs"]
  node_version = None
  invocations = []
  for step in workflow["jobs"]["wasm"]["steps"]:
    if str(step.get("uses", "")).startswith("actions/setup-node@"):
      node_version = step["with"]["node-version"]
    for line in step.get("run", "").splitlines():
      if line.strip().startswith(f"node {entry} "):
        invocations.append((node_version, shlex.split(line)))

  assert invocations == [
    ("${{ env.NODE_CURRENT }}", [*command, "--exhaustive"]),
    ("${{ env.NODE_FLOOR }}", command),
  ]


def test_wasm_pack_outputs_select_both_tarballs_from_metadata(tmp_path, monkeypatch):
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  step = next(step for step in workflow["jobs"]["wasm"]["steps"] if step.get("id") == "npm-package")
  build, separator, script = step["run"].partition("python3 - <<'PY'\n")
  assert build.strip() == "python3 toolbox/build_npm.py"
  assert separator and script.endswith("PY\n")

  out = tmp_path / "build" / "npm"
  out.mkdir(parents=True)
  for name, stem, filename in (
    ("@0xf3cd/celestial", "npm-pack", "primary-selected.tgz"),
    ("celestial-calendar", "npm-alias-pack", "alias-selected.tgz"),
  ):
    (out / filename).write_bytes(b"tarball fixture")
    (out / f"{stem}.json").write_text(
      json.dumps([{"name": name, "version": "0.7.0", "filename": filename}]), encoding="utf-8"
    )
  (out / "000-decoy.tgz").write_bytes(b"not selected by metadata")
  output = tmp_path / "github-output"
  monkeypatch.setenv("GITHUB_OUTPUT", str(output))
  monkeypatch.setattr(build_npm, "project_version", lambda: "0.7.0")
  monkeypatch.chdir(tmp_path)

  exec(compile(script.removesuffix("PY\n"), str(WORKFLOW), "exec"), {})

  assert output.read_text(encoding="utf-8").splitlines() == [
    f"tarball={Path('build/npm/primary-selected.tgz')}",
    f"alias_tarball={Path('build/npm/alias-selected.tgz')}",
  ]


def test_wasm_consumers_receive_the_metadata_pair_through_quoted_environment(tmp_path):
  if shutil.which("bash") is None:
    pytest.skip("WASM consumer steps require bash")
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  steps = workflow["jobs"]["wasm"]["steps"]
  consumers = [step for step in steps if "TARBALL" in step.get("env", {})]
  assert len(consumers) == 3
  assert sum("toolbox/build_npm.py" in step.get("run", "") for step in steps) == 1
  fake_bin = tmp_path / "bin"
  fake_bin.mkdir()
  calls = tmp_path / "calls.jsonl"
  for name in ("node", "npm"):
    executable = fake_bin / name
    executable.write_text(
      f"#!{sys.executable}\nimport json, os, sys\n"
      "with open(os.environ['CALLS'], 'a') as output:\n"
      "  output.write(json.dumps(sys.argv[1:]) + '\\n')\n",
      encoding="utf-8",
    )
    executable.chmod(0o755)
  primary = 'primary $(touch INJECTED) " ;.tgz'
  alias = 'alias `touch INJECTED` " ;.tgz'
  for step in consumers:
    assert step["env"] == {
      "TARBALL": "${{ steps.npm-package.outputs.tarball }}",
      "ALIAS_TARBALL": "${{ steps.npm-package.outputs.alias_tarball }}",
    }
    assert "${{" not in step["run"]
    result = subprocess.run(
      [shutil.which("bash"), "--noprofile", "--norc", "-euo", "pipefail", "-c", step["run"]],
      cwd=tmp_path,
      env={"PATH": str(fake_bin), "CALLS": str(calls), "TARBALL": primary, "ALIAS_TARBALL": alias},
      capture_output=True,
      text=True,
    )
    assert result.returncode == 0, result.stderr
  records = [json.loads(line) for line in calls.read_text().splitlines()]
  pair_calls = [
    args for args in records if args and args[0].endswith(("tarball_consumer_test.mjs", "browser_test.mjs"))
  ]
  assert len(pair_calls) == 3
  assert all(args[1:] == [primary, alias] for args in pair_calls)
  assert not (tmp_path / "INJECTED").exists()
