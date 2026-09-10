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

from types import SimpleNamespace
from pathlib import Path

import pytest
import yaml

import toolbox.release_downloader as release_downloader_module

from automation.github import GitHub
from toolbox.release_downloader import archive_validation_version, release_license_validation
from toolbox.release_validation import LicenseValidation


RELEASE_WORKFLOW = Path(__file__).parents[2] / ".github" / "workflows" / "release.yml"


def release(tag_name):
  return GitHub.Release(7, tag_name, False, False, "", "", tag_name, "", "", "", "", "", [])


@pytest.mark.parametrize(
  ("tag_name", "expected"),
  [("v0.5.0", None), ("v0.6.0", "0.6.0"), ("v0.10.0", "0.10.0"), ("v1.2.3", "1.2.3")],
)
def test_archive_validation_starts_at_v060(tag_name, expected):
  assert archive_validation_version(tag_name) == expected


@pytest.mark.parametrize("tag_name", ["latest", "v0.6.0-rc1"])
def test_archive_validation_rejects_unknown_tag_shape(tag_name):
  with pytest.raises(RuntimeError, match="Cannot determine the archive contract"):
    archive_validation_version(tag_name)


@pytest.mark.parametrize(
  ("version", "expected"),
  [
    ("0.6.0", LicenseValidation.LEGACY),
    ("0.6.1", LicenseValidation.LEGACY),
    ("0.6.2", LicenseValidation.MEMBERS),
    ("0.7.0", LicenseValidation.MEMBERS),
    ("1.2.3", LicenseValidation.MEMBERS),
  ],
)
def test_published_legacy_license_contract_is_closed_at_v061(version, expected):
  assert release_license_validation(version) is expected


@pytest.mark.parametrize("version", ["0.7", "latest"])
def test_license_contract_rejects_unknown_version_shape(version):
  with pytest.raises(RuntimeError, match="Cannot determine the LICENSE contract"):
    release_license_validation(version)


def run_download(monkeypatch, tmp_path, tag_name, validator=None):
  selected = release(tag_name)
  downloaded = [tmp_path / "celestial-wasm.zip", tmp_path / "CHANGELOG.md", tmp_path / "src.zip"]
  calls = []
  for path in downloaded:
    path.write_bytes(b"downloaded")

  def record_validation(
    paths,
    version,
    check_documented_runtime,
    require_wheels,
    license_validation,
  ):
    calls.append(
      (
        paths,
        version,
        check_documented_runtime,
        require_wheels,
        license_validation,
      )
    )

  monkeypatch.setattr(
    release_downloader_module,
    "parse_args",
    lambda: SimpleNamespace(id=None, tag=tag_name, save_to=tmp_path, parallel=4),
  )
  monkeypatch.setattr(release_downloader_module, "validate_args", lambda _args: None)
  monkeypatch.setattr(release_downloader_module, "find_release", lambda _keyword: selected)
  monkeypatch.setattr(GitHub, "download_release", lambda _id, _save_to, _parallel: downloaded)
  monkeypatch.setattr(
    release_downloader_module,
    "validate_release_archives",
    validator or record_validation,
  )

  release_downloader_module.main()
  return downloaded, calls


def test_v060_release_download_reuses_archive_validation(monkeypatch, tmp_path):
  downloaded, calls = run_download(monkeypatch, tmp_path, "v0.6.0")

  assert calls == [(downloaded, "0.6.0", False, True, LicenseValidation.LEGACY)]


def test_release_download_validates_against_tag_version(monkeypatch, tmp_path):
  downloaded, calls = run_download(monkeypatch, tmp_path, "v1.2.3")

  assert calls == [(downloaded, "1.2.3", False, True, LicenseValidation.MEMBERS)]


def test_release_download_preserves_assets_when_validation_fails(monkeypatch, tmp_path):
  downloaded = [tmp_path / "celestial-wasm.zip", tmp_path / "CHANGELOG.md", tmp_path / "src.zip"]

  def reject_archives(
    paths,
    version,
    check_documented_runtime,
    require_wheels,
    license_validation,
  ):
    assert paths == downloaded
    assert version == "0.6.0"
    assert check_documented_runtime is False
    assert require_wheels is True
    assert license_validation is LicenseValidation.LEGACY
    raise RuntimeError("invalid archive")

  with pytest.raises(RuntimeError, match="invalid archive"):
    run_download(monkeypatch, tmp_path, "v0.6.0", validator=reject_archives)

  assert all(path.read_bytes() == b"downloaded" for path in downloaded)


def test_historical_release_download_keeps_legacy_behavior(monkeypatch, tmp_path):
  _downloaded, calls = run_download(monkeypatch, tmp_path, "v0.5.0")

  assert calls == []


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
  assert setup["with"]["python-version"] == "3.12"
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


def test_github_release_job_only_downloads_and_publishes_frozen_candidate():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
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


def test_pypi_job_has_only_candidate_download_and_oidc_publication():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
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


def test_npm_job_uses_exact_candidate_with_no_token_or_mutable_install():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  job = workflow["jobs"]["publish_npm"]
  text = yaml.safe_dump(job)
  setup = next(step for step in job["steps"] if step.get("name") == "Set up the trusted npm runtime")
  steps = job["steps"]

  assert job["needs"] == ["prepare_release", "create_release"]
  assert job["environment"] == "npm"
  assert job["permissions"] == {"contents": "read", "id-token": "write"}
  assert setup == {
    "name": "Set up the trusted npm runtime",
    "uses": "actions/setup-node@249970729cb0ef3589644e2896645e5dc5ba9c38",
    "with": {"node-version": "24.19.0"},
  }
  checkout = next(step for step in steps if str(step.get("uses", "")).startswith("actions/checkout@"))
  assert checkout["with"] == {"ref": "${{ github.sha }}", "persist-credentials": False}
  runtime = next(step for step in steps if step.get("name") == "Verify the trusted npm runtime")
  assert '"$(npm --version)" != "11.17.0"' in runtime["run"]
  last_publish = steps.index(runtime)
  for label, package in (("npm-primary", "'@0xf3cd/celestial'"), ("npm-alias", "celestial-calendar")):
    classify = next(step for step in steps if step.get("id") == label)
    publish = next(step for step in steps if step.get("name") == f"Publish exact {label} tarball")
    assert last_publish < steps.index(classify) == steps.index(publish) - 1
    last_publish = steps.index(publish)
    assert "toolbox/registry_verifier.py classify-npm" in classify["run"]
    assert f"--package {package}" in classify["run"]
    assert '--candidate candidate --version "${TAG_NAME#v}" --commit "$COMMIT_SHA"' in classify["run"]
    assert classify["env"] == {"COMMIT_SHA": "${{ github.sha }}", "TAG_NAME": "${{ github.ref_name }}"}
    assert publish["env"] == {
      "STATE": "${{ steps." + label + ".outputs.state }}",
      "TARBALL": "${{ steps." + label + ".outputs.tarball }}",
    }
    assert 'npm publish "$TARBALL" --access public --ignore-scripts' in publish["run"]
    assert "bootstrap_required)" in publish["run"] and "exit 1" in publish["run"]
    assert "${{" not in publish["run"]
  assert "NODE_AUTH_TOKEN" not in text
  assert "skip-existing" not in text
  assert "npm install" not in text
  assert "npm pack" not in text
  assert "build_npm" not in text and "build_wasm" not in text
  assert "tarballs[0]" not in text and "*.tgz" not in text


def test_registry_verifier_is_unprivileged_and_runs_both_clean_installs():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  job = workflow["jobs"]["verify_registries"]
  commands = "\n".join(str(step.get("run", "")) for step in job["steps"])

  assert job["needs"] == ["publish_pypi", "publish_npm"]
  assert job["permissions"] == {"contents": "read"}
  assert "environment" not in job
  assert "id-token" not in job["permissions"]
  assert "toolbox/registry_verifier.py verify" in commands
  assert "pip --isolated install" in commands
  assert "--only-binary=:all:" in commands
  assert "--no-deps" in commands
  assert 'cd "$RUNNER_TEMP"' in commands
  assert 'bindings/javascript/test/registry/registry_consumer_test.mjs "${TAG_NAME#v}"' in commands


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
    "`gh run rerun RUN_ID --job NPM_JOB_ID`. Fresh classification retains successful exact package bytes "
    "and continues with the remaining package."
  ) in recovery
  assert 'After any irreversible job succeeds, never use "Re-run all jobs".' in recovery


def test_every_release_action_is_sha_pinned():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  actions = [step["uses"] for job in workflow["jobs"].values() for step in job["steps"] if "uses" in step]

  assert actions
  assert all(re.fullmatch(r"[^@]+@[0-9a-f]{40}", action) for action in actions)


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
