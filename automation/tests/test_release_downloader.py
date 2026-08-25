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
    ("0.6.1", LicenseValidation.LEGACY),
    ("0.7.0", LicenseValidation.MEMBERS),
    ("1.2.3", LicenseValidation.MEMBERS),
  ],
)
def test_canonical_license_contract_starts_at_v070(version, expected):
  assert release_license_validation(version) is expected


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
  assert install["run"] == "python3 -m pip install -r Requirements.txt"


def test_release_preparation_validates_ref_and_stages_one_candidate():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  steps = workflow["jobs"]["prepare_release"]["steps"]
  context = next(step for step in steps if step.get("name") == "Validate protected release context")
  download = next(step for step in steps if step.get("name") == "Download exact producer runs")
  stage = next(step for step in steps if step.get("name") == "Stage immutable release candidate")
  classify = next(step for step in steps if step.get("name") == "Classify exact npm version")
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
  assert workflow["jobs"]["prepare_release"]["outputs"] == {
    "npm_publish_required": "${{ steps.npm-version.outputs.publish_required }}"
  }
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
  publish = next(
    step for step in job["steps"] if step.get("name") == "Publish exact tarball or report verified bootstrap"
  )

  assert job["needs"] == ["prepare_release", "create_release"]
  assert job["environment"] == "npm"
  assert job["permissions"] == {"contents": "read", "id-token": "write"}
  assert setup == {
    "name": "Set up the trusted npm runtime",
    "uses": "actions/setup-node@249970729cb0ef3589644e2896645e5dc5ba9c38",
    "with": {"node-version": "24.19.0"},
  }
  assert publish["env"] == {
    "PUBLISH_REQUIRED": "${{ needs.prepare_release.outputs.npm_publish_required }}"
  }
  assert '"$(npm --version)" != "11.17.0"' in publish["run"]
  assert 'npm publish "${tarballs[0]}" --access public --ignore-scripts' in publish["run"]
  assert "NODE_AUTH_TOKEN" not in text
  assert "skip-existing" not in text
  assert "npm install" not in text
  assert "actions/checkout" not in text
  assert "toolbox/" not in text


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


def test_every_release_action_is_sha_pinned():
  workflow = yaml.safe_load(RELEASE_WORKFLOW.read_text(encoding="utf-8"))
  actions = [
    step["uses"]
    for job in workflow["jobs"].values()
    for step in job["steps"]
    if "uses" in step
  ]

  assert actions
  assert all(re.fullmatch(r"[^@]+@[0-9a-f]{40}", action) for action in actions)
