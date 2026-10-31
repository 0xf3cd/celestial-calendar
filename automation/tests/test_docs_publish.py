# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import copy
import hashlib
import os
import subprocess
from pathlib import Path

import pytest
import yaml


WORKFLOW = Path(__file__).parents[2] / ".github/workflows/docs-publish.yml"
CHECKOUT = "actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1"
PYTHON = "actions/setup-python@5fda3b95a4ea91299a34e894583c3862153e4b97"
UPLOAD = "actions/upload-pages-artifact@7b1f4a764d45c48632c6b24a0339c27f5614fb0b"
DEPLOY = "actions/deploy-pages@d6db90164ac5ed86f2b6aed7e0febac5b3c0c03e"
CONDITION = (
  "github.event_name == 'workflow_dispatch' && github.ref_type == 'branch' && "
  "github.ref_name == github.event.repository.default_branch && github.ref_protected"
)
SCRIPT_HASHES = {
  ("context", 0): "32dc4649cd959bc765c15f40011a69361552041235cd8eac27a0f9f23ea589df",
  ("build", 2): "67d43eafe48f975a8ec1ccc9d79c9d235e0396774dea9fe40df4fe5e4a450575",
  ("build", 3): "478005747fa3da1fa39db13b5cc99aad72392b1b9b0a8a36ef01c8fe74f17106",
}


def workflow():
  return yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))


def assert_publish_contract(document):
  # Pin complete job shapes, including every script, so a new writer cannot hide behind an allowed action.
  actual = copy.deepcopy(document)
  for name, job in actual["jobs"].items():
    for index, step in enumerate(job["steps"]):
      if "run" in step:
        assert hashlib.sha256(step["run"].encode()).hexdigest() == SCRIPT_HASHES[(name, index)]
        step["run"] = "verified script"
  assert actual == {
    "name": "Publish Development Documentation",
    True: {"workflow_dispatch": None},
    "permissions": {},
    "concurrency": {"group": "docs-publish", "cancel-in-progress": False},
    "jobs": {
      "context": {
        "runs-on": "ubuntu-26.04",
        "permissions": {},
        "steps": [
          {
            "name": "Validate protected default-branch dispatch",
            "run": "verified script",
            "env": {
              "EVENT_NAME": "${{ github.event_name }}",
              "REF_TYPE": "${{ github.ref_type }}",
              "REF_NAME": "${{ github.ref_name }}",
              "DEFAULT_BRANCH": "${{ github.event.repository.default_branch }}",
              "REF_PROTECTED": "${{ github.ref_protected }}",
              "SOURCE_SHA": "${{ github.sha }}",
            },
          }
        ],
      },
      "build": {
        "needs": "context",
        "runs-on": "ubuntu-26.04",
        "permissions": {"contents": "read"},
        "env": {"DOXYGEN_PACKAGE_VERSION": "1.15.0+ds1-1ubuntu3"},
        "outputs": {"artifact_id": "${{ steps.pages.outputs.artifact_id }}"},
        "steps": [
          {"uses": CHECKOUT, "with": {"ref": "${{ github.sha }}", "persist-credentials": False}},
          {"uses": PYTHON, "with": {"python-version": "3.12"}},
          {"name": "Install pinned documentation tools", "run": "verified script"},
          {
            "name": "Build and verify exact development source",
            "env": {"SOURCE_SHA": "${{ github.sha }}"},
            "run": "verified script",
          },
          {
            "name": "Upload sole immutable Pages artifact",
            "id": "pages",
            "uses": UPLOAD,
            "with": {"name": "github-pages", "path": "build/docs-site/", "retention-days": 30},
          },
        ],
      },
      "deploy": {
        "needs": "build",
        "if": CONDITION,
        "runs-on": "ubuntu-26.04",
        "permissions": {"pages": "write", "id-token": "write"},
        "environment": {"name": "github-pages", "url": "${{ steps.deployment.outputs.page_url }}"},
        "steps": [
          {
            "name": "Deploy the verified artifact from this run",
            "id": "deployment",
            "uses": DEPLOY,
            "with": {"artifact_name": "github-pages"},
          }
        ],
      },
    },
  }


def test_manual_publish_has_one_immutable_writer_and_no_privileged_source_execution():
  assert_publish_contract(workflow())


@pytest.mark.parametrize(
  "fault",
  [
    "event",
    "input",
    "default",
    "protected",
    "needs-context",
    "needs-build",
    "ref",
    "credentials",
    "build-write",
    "deploy-read",
    "deploy-checkout",
    "deploy-run",
    "upload-v3",
    "another-writer",
    "extra-job",
    "token",
    "env",
    "defaults",
    "source-run",
    "action-identity",
    "artifact-selector",
    "skip-validation",
    "continue-on-error",
  ],
)
def test_contract_rejects_privilege_guard_writer_and_execution_drift(fault):
  document = workflow()
  assert_publish_contract(document)
  jobs = document["jobs"]
  build, deploy = jobs["build"], jobs["deploy"]
  if fault == "event":
    document[True]["push"] = {}
  elif fault == "input":
    document[True]["workflow_dispatch"] = {"inputs": {"ref": {"type": "string"}}}
  elif fault in ("default", "protected"):
    deploy["if"] = deploy["if"].replace(
      "github.ref_protected" if fault == "protected" else "github.ref_name == github.event.repository.default_branch",
      "true",
    )
  elif fault == "needs-context":
    del build["needs"]
  elif fault == "needs-build":
    deploy["needs"] = "context"
  elif fault == "ref":
    build["steps"][0]["with"]["ref"] = "main"
  elif fault == "credentials":
    build["steps"][0]["with"]["persist-credentials"] = True
  elif fault == "build-write":
    build["permissions"]["pages"] = "write"
  elif fault == "deploy-read":
    deploy["permissions"]["contents"] = "read"
  elif fault == "deploy-checkout":
    deploy["steps"].insert(0, {"uses": CHECKOUT})
  elif fault == "deploy-run":
    deploy["steps"].append({"run": "python project.py --site"})
  elif fault == "upload-v3":
    build["steps"][-1]["uses"] = "actions/upload-artifact@v3"
  elif fault == "another-writer":
    build["steps"].append(copy.deepcopy(build["steps"][-1]))
  elif fault == "extra-job":
    jobs["writer"] = copy.deepcopy(build)
  elif fault == "token":
    deploy["steps"][0]["env"] = {"TOKEN": "${{ secrets.GITHUB_TOKEN }}"}
  elif fault == "env":
    document["env"] = {"BASH_ENV": "startup.sh"}
  elif fault == "defaults":
    document["defaults"] = {"run": {"shell": "bash"}}
  elif fault == "source-run":
    build["steps"][3]["run"] += "\npython unverified.py\n"
  elif fault == "action-identity":
    deploy["steps"][0]["uses"] = "actions/deploy-pages@" + "a" * 40
  elif fault == "artifact-selector":
    deploy["steps"][0]["with"]["run_id"] = "123"
  elif fault == "skip-validation":
    jobs["context"]["steps"][0]["if"] = False
  else:
    jobs["context"]["continue-on-error"] = True
  with pytest.raises((AssertionError, KeyError)):
    assert_publish_contract(document)


@pytest.mark.parametrize(
  "field,value",
  [
    (None, None),
    ("EVENT_NAME", "push"),
    ("REF_TYPE", "tag"),
    ("REF_NAME", "feature"),
    ("DEFAULT_BRANCH", ""),
    ("REF_PROTECTED", "false"),
    ("SOURCE_SHA", "main"),
    ("REF_NAME", 'main"; touch INJECTED; #'),
  ],
)
def test_actual_metadata_script_fails_before_checkout(tmp_path, field, value):
  document = workflow()
  assert_publish_contract(document)
  environment = {
    **os.environ,
    "EVENT_NAME": "workflow_dispatch",
    "REF_TYPE": "branch",
    "REF_NAME": "main",
    "DEFAULT_BRANCH": "main",
    "REF_PROTECTED": "true",
    "SOURCE_SHA": "1" * 40,
  }
  if field:
    environment[field] = value
  result = subprocess.run(
    ["bash", "-euo", "pipefail", "-c", document["jobs"]["context"]["steps"][0]["run"]],
    cwd=tmp_path,
    env=environment,
    capture_output=True,
    text=True,
  )
  assert result.returncode == (0 if field is None else 1)
  assert not (tmp_path / "INJECTED").exists()
