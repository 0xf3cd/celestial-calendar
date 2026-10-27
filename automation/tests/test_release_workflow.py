# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import re
import json
import subprocess
import sys
import shutil

from pathlib import Path

import pytest
import yaml

RELEASE_WORKFLOW = Path(__file__).parents[2] / ".github" / "workflows" / "release.yml"


def test_release_workflow_is_deliberately_dispatched_with_exact_runs():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  trigger = workflow.get("on", workflow.get(True))

  assert set(trigger) == {"workflow_dispatch"}
  assert set(trigger["workflow_dispatch"]["inputs"]) == {"native_run_id", "wasm_run_id", "python_run_id"}
  assert all(value["required"] for value in trigger["workflow_dispatch"]["inputs"].values())
  assert workflow["concurrency"] == {
    "group": "release-${{ github.ref }}",
    "cancel-in-progress": False,
  }


def test_release_preparation_has_read_only_permissions_and_pinned_context():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  job = workflow["jobs"]["prepare_release"]
  steps = job["steps"]
  names = [step.get("name") for step in steps]

  assert job["permissions"] == {"actions": "read", "contents": "read"}
  checkout = next(step for step in steps if str(step.get("uses", "")).startswith("actions/checkout@"))
  assert checkout["with"] == {"fetch-depth": 0, "persist-credentials": False}
  setup = next(step for step in steps if step.get("name") == "Set up Python")
  assert re.fullmatch(r"\d+\.\d+(?:\.\d+)?", setup["with"]["python-version"])
  assert names.index("Install pinned Python dependencies") < names.index("Download exact producer runs")
  install = next(step for step in steps if step.get("name") == "Install pinned Python dependencies")
  assert install["run"] == "python3 -m pip install --require-hashes --only-binary :all: -r Requirements-producer.txt"


def test_release_preparation_validates_ref_and_stages_one_candidate():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  steps = workflow["jobs"]["prepare_release"]["steps"]
  context = next(step for step in steps if step.get("name") == "Validate protected release context")
  download = next(step for step in steps if step.get("name") == "Download exact producer runs")
  stage = next(step for step in steps if step.get("name") == "Stage immutable release candidate")
  classifiers = [step for step in steps if step.get("name", "").startswith("Classify exact npm-")]
  upload = next(step for step in steps if step.get("name") == "Upload immutable release candidate")

  assert context["env"] == {
    "REF_NAME": "${{ github.ref_name }}",
    "REF_PROTECTED": "${{ github.ref_protected }}",
    "REF_TYPE": "${{ github.ref_type }}",
  }
  assert 'os.environ["REF_TYPE"] != "tag"' in context["run"]
  assert 're.fullmatch(r"v\\d+\\.\\d+\\.\\d+", ref_name)' in context["run"]
  assert 'os.environ["REF_PROTECTED"] != "true"' in context["run"]
  assert '["./project.py", "--version"]' in context["run"]
  assert "git merge-base --is-ancestor HEAD origin/main" in context["run"]
  assert "validate_release_document_versions" in context["run"]
  assert download["env"] == {
    "GITHUB_TOKEN": "${{ secrets.GITHUB_TOKEN }}",
    "NATIVE_RUN_ID": "${{ inputs.native_run_id }}",
    "PYTHON_RUN_ID": "${{ inputs.python_run_id }}",
    "WASM_RUN_ID": "${{ inputs.wasm_run_id }}",
  }
  assert "--source-manifest release-sources.json" in download["run"]
  assert "toolbox/release_candidate.py" in stage["run"]
  assert "candidate/evidence/manifest.json" in stage["run"]
  assert "outputs" not in workflow["jobs"]["prepare_release"]
  assert len(classifiers) == 2
  assert "--package '@0xf3cd/celestial'" in classifiers[0]["run"]
  assert "--package celestial-calendar" in classifiers[1]["run"]
  for classify in classifiers:
    assert steps.index(stage) < steps.index(classify) < steps.index(upload)
    assert "toolbox/registry_verifier.py classify-npm" in classify["run"]
    assert '--github-output "$GITHUB_OUTPUT"' in classify["run"]
    assert '--github-summary "$GITHUB_STEP_SUMMARY"' in classify["run"]
  assert upload["with"] == {
    "name": "celestial-release-candidate",
    "path": "candidate/",
    "retention-days": 30,
    "if-no-files-found": "error",
  }


def assert_github_release_contract(workflow):
  job = workflow["jobs"]["create_release"]
  steps = job["steps"]

  assert job["needs"] == "prepare_release"
  assert job["permissions"] == {"contents": "write"}
  assert [step["name"] for step in steps] == [
    "Download immutable release candidate",
    "Create immutable GitHub Release",
  ]
  release = steps[1]
  assert release["uses"] == "ncipollo/release-action@339a81892b84b4eeb0f6e744e4574d79d0d9b8dd"
  assert release["with"] == {
    "artifacts": "candidate/github/*",
    "bodyFile": "candidate/evidence/RELEASE_NOTES.md",
    "immutableCreate": True,
    "artifactErrorsFailBuild": True,
  }


def assert_pypi_contract(workflow):
  job = workflow["jobs"]["publish_pypi"]
  steps = job["steps"]

  assert RELEASE_WORKFLOW.name == "release.yml"
  assert job["needs"] == "create_release"
  assert job["environment"] == "pypi"
  assert job["permissions"] == {"contents": "read", "id-token": "write"}
  assert [step["name"] for step in steps] == [
    "Download immutable release candidate",
    "Publish exact wheels to PyPI",
  ]
  assert "run" not in steps[0] and "run" not in steps[1]
  publish = steps[1]
  assert publish["uses"] == "pypa/gh-action-pypi-publish@dc37677b2e1c63e2034f94d8a5b11f265b73ba33"
  assert publish["with"] == {
    "packages-dir": "candidate/pypi",
    "verify-metadata": True,
    "attestations": True,
  }


@pytest.mark.parametrize("contract", [assert_github_release_contract, assert_pypi_contract])
def test_sole_credentialed_action_identity(contract):
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  contract(workflow)
  job_name = "create_release" if contract is assert_github_release_contract else "publish_pypi"
  workflow["jobs"][job_name]["steps"][1]["uses"] = (
    workflow["jobs"][job_name]["steps"][1]["uses"].partition("@")[0] + "@" + "a" * 40
  )
  with pytest.raises(AssertionError):
    contract(workflow)


def action_identities(workflow):
  identities = {}
  for job in workflow["jobs"].values():
    for step in job["steps"]:
      if "uses" not in step:
        continue
      target, separator, ref = step["uses"].partition("@")
      assert separator and re.fullmatch(r"[0-9a-f]{40}", ref)
      assert target not in identities or identities[target] == ref
      identities[target] = ref
  return identities


def assert_registry_job_contract(workflow, job_name, workflow_dir=RELEASE_WORKFLOW.parent):
  assert workflow.get("env", {}) == {}
  assert workflow.get("defaults", {}) == {}
  assert job_name in ("publish_npm", "verify_registries")
  identities = action_identities(workflow)
  wasm = yaml.safe_load((workflow_dir / "wasm.yml").read_text(encoding="utf-8"))
  wheels = yaml.safe_load((workflow_dir / "python-wheel.yml").read_text(encoding="utf-8"))
  python_version = next(
    step["with"]["python-version"]
    for step in workflow["jobs"]["prepare_release"]["steps"]
    if str(step.get("uses", "")).startswith("actions/setup-python@")
  )
  python_floor = next(
    step["with"]["python-version"]
    for step in wheels["jobs"]["macos-14-floor"]["steps"]
    if str(step.get("uses", "")).startswith("actions/setup-python@")
  )
  node_version, npm_version = wasm["env"]["NODE_CURRENT"], wasm["env"]["NPM_VERSION"]
  checkout = {
    "uses": f"actions/checkout@{identities['actions/checkout']}",
    "with": {"persist-credentials": False},
  }
  download = {
    "name": "Download immutable release candidate",
    "uses": f"actions/download-artifact@{identities['actions/download-artifact']}",
    "with": {"name": "celestial-release-candidate", "path": "candidate"},
  }
  python = {
    "name": "Set up Python",
    "uses": f"actions/setup-python@{identities['actions/setup-python']}",
    "with": {"python-version": python_version},
  }
  install = {
    "name": "Install pinned classifier dependencies",
    "run": "python3 -m pip install --require-hashes --only-binary=:all: -r Requirements-producer.txt",
  }
  node = {
    "name": "Set up the trusted npm runtime",
    "uses": f"actions/setup-node@{identities['actions/setup-node']}",
    "with": {"node-version": node_version},
  }
  identity = {"COMMIT_SHA": "${{ github.sha }}", "TAG_NAME": "${{ github.ref_name }}"}

  if job_name == "publish_npm":
    checkout["with"]["ref"] = "${{ github.sha }}"
    steps = [
      checkout,
      python,
      install,
      download,
      node,
      {
        "name": "Verify the trusted npm runtime",
        "run": f'if [ "$(node --version)" != "v{node_version}" ] || [ "$(npm --version)" != "{npm_version}" ]; then\n'
        '  echo "Unexpected Node/npm runtime: $(node --version) / $(npm --version)"\n'
        "  exit 1\n"
        "fi\n",
      },
    ]
    publications = (
      (
        "npm-primary",
        "'@0xf3cd/celestial'",
        'case "$STATE" in\n'
        '  absent) echo "npm-primary: publishing frozen tarball."; '
        'npm publish "$TARBALL" --access public --ignore-scripts ;;\n'
        '  exact) echo "npm-primary: exact bytes already published; verified no-op." ;;\n'
        '  bootstrap_required) echo "npm-primary: bootstrap required; no publication attempted."; exit 1 ;;\n'
        '  *) echo "npm-primary: invalid classification: $STATE"; exit 1 ;;\n'
        "esac\n",
      ),
      (
        "npm-alias",
        "celestial-calendar",
        'case "$STATE" in\n'
        '  absent) echo "npm-alias: publishing frozen tarball; npm-primary completed."; '
        'npm publish "$TARBALL" --access public --ignore-scripts ;;\n'
        '  exact) echo "npm-alias: exact bytes already published; verified no-op." ;;\n'
        '  bootstrap_required) echo "npm-alias: bootstrap required; npm-primary completed. '
        'Retain the frozen candidate and retry only this npm job after authorized bootstrap."; exit 1 ;;\n'
        '  *) echo "npm-alias: invalid classification: $STATE; npm-primary completed."; exit 1 ;;\n'
        "esac\n",
      ),
    )
    for label, package, publish in publications:
      steps.extend(
        [
          {
            "name": f"Freshly classify {label}",
            "id": label,
            "env": identity,
            "run": "python3 toolbox/registry_verifier.py classify-npm \\\n"
            f"  --package {package} \\\n"
            '  --candidate candidate --version "${TAG_NAME#v}" --commit "$COMMIT_SHA" \\\n'
            '  --github-output "$GITHUB_OUTPUT" --github-summary "$GITHUB_STEP_SUMMARY"\n',
          },
          {
            "name": f"Publish exact {label} tarball",
            "env": {
              "STATE": "${{ steps." + label + ".outputs.state }}",
              "TARBALL": "${{ steps." + label + ".outputs.tarball }}",
            },
            "run": publish,
          },
        ]
      )
    expected = {
      "needs": ["prepare_release", "create_release"],
      "runs-on": "ubuntu-latest",
      "environment": "npm",
      "permissions": {"contents": "read", "id-token": "write"},
      "steps": steps,
    }
  else:
    python["name"] = "Set up Python floor"
    python["with"]["python-version"] = python_floor
    install["name"] = "Install pinned registry verifier dependencies"
    node["name"] = "Set up Node registry consumer"
    expected = {
      "needs": ["publish_pypi", "publish_npm"],
      "runs-on": "ubuntu-latest",
      "permissions": {"contents": "read"},
      "steps": [
        checkout,
        download,
        python,
        install,
        {
          "name": "Verify exact registry metadata and bytes",
          "env": identity,
          "run": "python3 toolbox/registry_verifier.py verify \\\n"
          "  --candidate candidate \\\n"
          '  --version "${TAG_NAME#v}" \\\n'
          '  --commit "$COMMIT_SHA"\n',
        },
        {
          "name": "Clean-install the PyPI wheel and run acceptance",
          "env": {"TAG_NAME": "${{ github.ref_name }}"},
          "run": "python3 -m venv registry-venv\n"
          "registry-venv/bin/python -m pip --isolated install \\\n"
          "  --index-url https://pypi.org/simple \\\n"
          "  --only-binary=:all: \\\n"
          "  --no-cache-dir \\\n"
          "  --no-deps \\\n"
          '  "celestial-calendar==${TAG_NAME#v}"\n'
          "(\n"
          '  cd "$RUNNER_TEMP"\n'
          '  "$GITHUB_WORKSPACE/registry-venv/bin/python" \\\n'
          '    "$GITHUB_WORKSPACE/bindings/python/test/run_all.py"\n'
          ")\n",
        },
        node,
        {
          "name": "Clean-install npm-primary, npm-alias, and the pair and run acceptance",
          "env": {"TAG_NAME": "${{ github.ref_name }}"},
          "run": 'node bindings/javascript/test/registry/registry_consumer_test.mjs "${TAG_NAME#v}"',
        },
      ],
    }
  assert workflow["jobs"][job_name] == expected


def test_npm_job_uses_exact_candidate_with_no_token_or_mutable_install():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  assert_registry_job_contract(workflow, "publish_npm")


def test_registry_verifier_is_unprivileged_and_runs_both_clean_installs():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  assert_registry_job_contract(workflow, "verify_registries")


@pytest.mark.parametrize("job_name", ["publish_npm", "verify_registries"])
@pytest.mark.parametrize(
  "mutation",
  [
    "extra-run",
    "extra-action",
    "duplicate-step",
    "changed-run",
    "changed-action",
    "action-input",
    "workflow-env",
    "job-env",
    "step-env",
    "workflow-defaults",
    "job-defaults",
    "step-shell",
    "step-condition",
    "continue-on-error",
    "runner",
    "permissions",
    "missing-hashes",
  ],
)
def test_registry_job_rejects_execution_changes(job_name, mutation):
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  assert_registry_job_contract(workflow, job_name)
  job = workflow["jobs"][job_name]
  steps = job["steps"]
  run = next(step for step in steps if "run" in step)
  action = next(step for step in steps if "uses" in step)
  if mutation == "extra-run":
    steps.append({"name": "Unexpected step", "run": "py -m pip install unexpected==1"})
  elif mutation == "extra-action":
    steps.append({"uses": action["uses"]})
  elif mutation == "duplicate-step":
    steps.append(steps[-1])
  elif mutation == "changed-run":
    run = next(step for step in steps if "run" in step and "Requirements" not in step["run"])
    run["run"] += "\ntime pip install unexpected==1\n"
  elif mutation == "changed-action":
    action["uses"] = "./unexpected-action"
  elif mutation == "action-input":
    action["with"]["ref"] = "unreviewed-ref"
  elif mutation == "workflow-env":
    workflow["env"] = {"NEW_VALUE": "ordinary text"}
  elif mutation == "job-env":
    job["env"] = {"NEW_VALUE": "ordinary text"}
  elif mutation == "step-env":
    run.setdefault("env", {})["NEW_VALUE"] = "ordinary text"
  elif mutation == "workflow-defaults":
    workflow["defaults"] = {"run": {"shell": "sh"}}
  elif mutation == "job-defaults":
    job["defaults"] = {"run": {"shell": "sh"}}
  elif mutation == "step-shell":
    run["shell"] = "sh"
  elif mutation == "step-condition":
    run["if"] = False
  elif mutation == "continue-on-error":
    run["continue-on-error"] = True
  elif mutation == "runner":
    job["runs-on"] = "windows-latest"
  elif mutation == "permissions":
    job["permissions"]["contents"] = "write"
  elif mutation == "missing-hashes":
    run["run"] = run["run"].replace("--require-hashes", "")
  with pytest.raises(AssertionError):
    assert_registry_job_contract(workflow, job_name)


@pytest.mark.parametrize("mutation", ["target", "dependencies", "extra-command"])
def test_registry_consumer_is_an_exact_step_not_an_install_exception(mutation):
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  assert_registry_job_contract(workflow, "verify_registries")
  consumer = next(
    step
    for step in workflow["jobs"]["verify_registries"]["steps"]
    if step.get("name") == "Clean-install the PyPI wheel and run acceptance"
  )
  if mutation == "target":
    consumer["run"] = consumer["run"].replace("celestial-calendar==${TAG_NAME#v}", "unexpected==1")
  elif mutation == "dependencies":
    consumer["run"] = consumer["run"].replace("--no-deps", "")
  else:
    consumer["run"] += "\n{ pip install unexpected==1; }\n"
  with pytest.raises(AssertionError):
    assert_registry_job_contract(workflow, "verify_registries")


@pytest.mark.parametrize("job_name", ["publish_npm", "verify_registries"])
def test_registry_job_pins_every_script_body(job_name):
  source = RELEASE_WORKFLOW.read_text(encoding="utf-8")
  workflow = yaml.safe_load(source)
  assert_registry_job_contract(workflow, job_name)
  for index, step in enumerate(workflow["jobs"][job_name]["steps"]):
    if "run" in step:
      changed = yaml.safe_load(source)
      changed["jobs"][job_name]["steps"][index]["run"] += "\n: unexpected-command\n"
      with pytest.raises(AssertionError):
        assert_registry_job_contract(changed, job_name)


def test_registry_jobs_start_only_after_the_immutable_github_release():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  jobs = workflow["jobs"]

  assert jobs["create_release"]["needs"] == "prepare_release"
  assert "create_release" in [jobs["publish_pypi"]["needs"], *jobs["publish_npm"]["needs"]]
  assert jobs["verify_registries"]["needs"] == ["publish_pypi", "publish_npm"]


def test_release_recovery_preserves_pypi_policy_and_targets_npm_retry():
  guide = (RELEASE_WORKFLOW.parents[2] / "docs/RELEASING.md").read_text(encoding="utf-8")
  recovery = " ".join(guide.split("## Recovery", maxsplit=1)[1].split())

  assert (
    "An unambiguous PyPI failure before registry acceptance may use "
    "`gh run rerun RUN_ID --failed` after reviewing the evidence."
  ) in recovery
  assert (
    "If a PyPI publish command fails ambiguously but registry queries prove the exact candidate is present, "
    "leave that publication job red. Do not rerun it or use `skip-existing`; "
    "record the recovery and complete consumer validation manually."
  ) in recovery
  assert (
    "For npm publication failure, inspect the evidence and retry only the npm job using "
    "`gh run rerun --job NPM_JOB_ID`. Fresh classification retains successful exact package bytes "
    "and continues with the remaining package."
  ) in recovery
  assert 'After any irreversible job succeeds, never use "Re-run all jobs".' in recovery


def test_every_release_action_is_sha_pinned():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  assert action_identities(workflow)


@pytest.mark.parametrize(
  "target", ["actions/checkout", "actions/download-artifact", "actions/setup-python", "actions/setup-node"]
)
def test_registry_action_updates_require_same_target_identity_everywhere(target):
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  steps = [
    step for job in workflow["jobs"].values() for step in job["steps"] if step.get("uses", "").startswith(target + "@")
  ]
  assert len(steps) > 1
  steps[0]["uses"] = target + "@" + "a" * 40
  for job in ("publish_npm", "verify_registries"):
    with pytest.raises(AssertionError):
      assert_registry_job_contract(workflow, job)
  for step in steps:
    step["uses"] = target + "@" + "a" * 40
  for job in ("publish_npm", "verify_registries"):
    assert_registry_job_contract(workflow, job)


def test_registry_runtime_updates_follow_workflow_owners(tmp_path):
  source = RELEASE_WORKFLOW.read_text(encoding="utf-8")
  workflow = yaml.safe_load(source)
  for name in ("wasm.yml", "python-wheel.yml"):
    shutil.copyfile(RELEASE_WORKFLOW.parent / name, tmp_path / name)
  for old, new in (("24.19.0", "25.0.0"), ("11.17.0", "12.0.0"), ("3.11.9", "3.11.10"), ('"3.12"', '"3.13"')):
    changed = yaml.safe_load(source)
    job_name = "verify_registries" if old == "3.11.9" else "publish_npm"
    steps = changed["jobs"][job_name]["steps"]
    if old in ("24.19.0", "11.17.0"):
      step = next(step for step in steps if step.get("name") == "Verify the trusted npm runtime")
      step["run"] = step["run"].replace(old, new)
    else:
      step = next(step for step in steps if step.get("uses", "").startswith("actions/setup-python@"))
      step["with"]["python-version"] = new.strip('"')
    with pytest.raises(AssertionError):
      assert_registry_job_contract(changed, job_name, tmp_path)
    for name in ("wasm.yml", "python-wheel.yml"):
      path = tmp_path / name
      path.write_text(path.read_text().replace(old, new), encoding="utf-8")
    source = source.replace(old, new)
    workflow = yaml.safe_load(source)
    for job in ("publish_npm", "verify_registries"):
      assert_registry_job_contract(workflow, job, tmp_path)


@pytest.mark.parametrize(
  "states, exits, published, npm_exit",
  [
    (("absent", "absent"), (0, 0), ["primary", "alias"], 0),
    (("exact", "absent"), (0, 0), ["alias"], 0),
    (("exact", "exact"), (0, 0), [], 0),
    (("absent", "bootstrap_required"), (0, 1), ["primary"], 0),
    (("bootstrap_required", "absent"), (1,), [], 0),
    (("exact", "conflict"), (0, 1), [], 0),
    (("absent", "absent"), (9,), ["primary"], 9),
    (("exact", "absent"), (0, 9), ["alias"], 9),
  ],
)
def test_extracted_npm_publication_steps_only_publish_absent_exact_selected_bytes(
  tmp_path, states, exits, published, npm_exit
):
  if shutil.which("bash") is None:
    pytest.skip("publisher steps require bash")
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  steps = [
    step for step in workflow["jobs"]["publish_npm"]["steps"] if step.get("name", "").startswith("Publish exact npm-")
  ]
  assert len(steps) == 2
  fake_bin = tmp_path / "bin"
  fake_bin.mkdir()
  fake_npm = fake_bin / "npm"
  fake_npm.write_text(
    f"#!{sys.executable}\nimport json, os, sys\n"
    "from pathlib import Path\n"
    "assert sys.argv[1] == 'publish'\n"
    "assert sys.argv[3:] == ['--access', 'public', '--ignore-scripts']\n"
    "with Path(os.environ['CALLS']).open('a') as output:\n"
    "  output.write(json.dumps([sys.argv[2], Path(sys.argv[2]).read_text()]) + '\\n')\n"
    "raise SystemExit(int(os.environ['NPM_EXIT']))\n",
    encoding="utf-8",
  )
  fake_npm.chmod(0o755)
  calls = tmp_path / "calls.jsonl"
  actual_exits = []
  paths = {}
  for step, state, label in zip(steps, states, ("primary", "alias"), strict=True):
    tarball = tmp_path / f'{label} $(touch INJECTED); "quoted".tgz'
    tarball.write_text(label, encoding="utf-8")
    paths[label] = str(tarball)
    result = subprocess.run(
      [shutil.which("bash"), "--noprofile", "--norc", "-euo", "pipefail", "-c", step["run"]],
      cwd=tmp_path,
      env={
        "PATH": str(fake_bin),
        "CALLS": str(calls),
        "STATE": state,
        "TARBALL": str(tarball),
        "NPM_EXIT": str(npm_exit),
      },
      capture_output=True,
      text=True,
    )
    actual_exits.append(result.returncode)
    if result.returncode:
      assert "npm-" + label in result.stdout
      break
  assert tuple(actual_exits) == exits
  recorded = [json.loads(line) for line in calls.read_text().splitlines()] if calls.exists() else []
  assert recorded == [[paths[label], label] for label in published]
  assert not (tmp_path / "INJECTED").exists()
