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
import io
import json
import os
import subprocess
import sys
from urllib import error, request
from pathlib import Path

import pytest
import yaml

from automation.tests.test_producer_dependencies import assert_hash_locked_install, pip_install_lines


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
  ("context", 0): "04707ca9f4658067764273e8b79c650590430e8d16987c6bcabb586a48004fc2",
  ("context", 2): "776490863bf1a87108045999e62d380f269ed4ea41756b637a132842b9982a54",
  ("build", 2): "c3d278e2c3aa5241d21b7da42ca14eb6d5d6164b95935733907a995c77b5dd8d",
  ("build", 3): "755bfe725e5cc201d0bdc4a2adeac9bd0b9ad8e96805e4dd06e59d034ce17c4a",
  ("build", 4): "9259c34a31076300a54f331f65c1db6e93ce1e6ab3d2b13bd0170d8d933b54e9",
}
SOURCE_CONTEXT = {
  "SOURCE_SHA": "${{ needs.context.outputs.source_sha }}",
  "SOURCE_VERSION": "${{ needs.context.outputs.version }}",
  "RELEASE_TAG": "${{ needs.context.outputs.release_tag }}",
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
    "name": "Publish Documentation",
    True: {
      "workflow_dispatch": {
        "inputs": {
          "release_tag": {
            "description": "Published immutable release tag; leave empty for development",
            "type": "string",
            "required": False,
            "default": "",
          }
        }
      }
    },
    "permissions": {},
    "concurrency": {"group": "docs-publish", "cancel-in-progress": False},
    "jobs": {
      "context": {
        "runs-on": "ubuntu-26.04",
        "permissions": {"contents": "read"},
        "outputs": {
          key: "${{ steps.resolve.outputs." + key + " }}" for key in ("source_sha", "version", "release_tag", "state")
        },
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
              "RELEASE_TAG": "${{ inputs.release_tag }}",
            },
          },
          {"uses": CHECKOUT, "with": {"ref": "${{ github.sha }}", "fetch-depth": 0, "persist-credentials": False}},
          {
            "name": "Resolve exact documentation source",
            "id": "resolve",
            "run": "verified script",
            "env": {
              "GITHUB_TOKEN": "${{ secrets.GITHUB_TOKEN }}",
              "REPOSITORY": "${{ github.repository }}",
              "DISPATCH_SHA": "${{ github.sha }}",
              "RELEASE_TAG": "${{ inputs.release_tag }}",
            },
          },
        ],
      },
      "build": {
        "needs": "context",
        "runs-on": "ubuntu-26.04",
        "permissions": {"contents": "read"},
        "env": {"DOXYGEN_PACKAGE_VERSION": "1.15.0+ds1-1ubuntu3"},
        "outputs": {"artifact_id": "${{ steps.pages.outputs.artifact_id }}"},
        "steps": [
          {
            "uses": CHECKOUT,
            "with": {
              "ref": "${{ needs.context.outputs.source_sha }}",
              "fetch-depth": 0,
              "persist-credentials": False,
            },
          },
          {"uses": PYTHON, "with": {"python-version": "3.12"}},
          {"name": "Install pinned documentation tools", "run": "verified script"},
          {
            "name": "Build and verify exact documentation source",
            "env": SOURCE_CONTEXT,
            "run": "verified script",
          },
          {
            "name": "Verify publication source and labels independently",
            "env": {**SOURCE_CONTEXT, "SOURCE_STATE": "${{ needs.context.outputs.state }}"},
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
    "tag-ref",
    "publisher-ref",
    "publisher-write",
    "skip-independent",
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
  elif fault == "tag-ref":
    build["steps"][0]["with"]["ref"] = "${{ inputs.release_tag }}"
  elif fault == "publisher-ref":
    jobs["context"]["steps"][1]["with"]["ref"] = "${{ inputs.release_tag }}"
  elif fault == "publisher-write":
    jobs["context"]["permissions"]["contents"] = "write"
  elif fault == "skip-independent":
    del build["steps"][4]
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
    ("RELEASE_TAG", "main"),
    ("RELEASE_TAG", "v1.2.0; touch INJECTED"),
    ("RELEASE_TAG", "v1.2.0\n"),
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
    "RELEASE_TAG": "",
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


def assert_documentation_install_policy(document):
  lines = [
    line
    for job in document["jobs"].values()
    for step in job["steps"]
    for line in pip_install_lines(step.get("run", ""))
  ]
  assert len(lines) == 1
  assert_hash_locked_install(lines[0])
  assert lines[0].endswith("-r Requirements-docs.txt")
  assert "--no-deps" not in lines[0]


@pytest.mark.parametrize("fault", ["hashes", "binary", "general", "extra"])
def test_publication_dependency_policy_rejects_unhashed_or_general_fallback(fault):
  document = workflow()
  assert_documentation_install_policy(document)
  step = document["jobs"]["build"]["steps"][2]
  if fault == "hashes":
    step["run"] = step["run"].replace("--require-hashes ", "")
  elif fault == "binary":
    step["run"] = step["run"].replace("--only-binary=:all: ", "")
  elif fault == "general":
    step["run"] = step["run"].replace("Requirements-docs.txt", "Requirements.txt")
  else:
    step["run"] += "\npython3 -m pip install markdown-it-py\n"
  with pytest.raises(AssertionError):
    assert_documentation_install_policy(document)


def inline_python(job, index):
  script = workflow()["jobs"][job]["steps"][index]["run"]
  return script.split("python3 - <<'PY'\n", 1)[1].rsplit("\nPY", 1)[0]


@pytest.fixture
def publisher_environment(tmp_path, monkeypatch):
  monkeypatch.chdir(tmp_path)
  executable = tmp_path / "bin/git"
  executable.parent.mkdir()
  executable.write_text(
    f"#!{sys.executable}\n"
    "import json,os,sys\n"
    "args=sys.argv[1:]\n"
    "with open(os.environ['GIT_CALL_LOG'],'a') as log: log.write(json.dumps(args)+'\\n')\n"
    "if args==['rev-parse','HEAD']: print(os.environ['CHECKOUT_SHA'])\n"
    "elif args==['rev-parse','FETCH_HEAD^{commit}']: print(os.environ['RESOLVED_SHA'])\n"
    "elif args[0]=='fetch': pass\n"
    "elif args[0]=='show':\n"
    " sha,path=args[1].split(':',1)\n"
    " version=os.environ['RELEASE_SOURCE_VERSION'] if sha==os.environ['RESOLVED_SHA'] else '9.9.9'\n"
    " if path=='project.py': print('BUILD_VERSION: str = '+json.dumps(version))\n"
    " else: print('## [v'+os.environ.get('DOCUMENT_VERSION',version)+']')\n"
    "elif args[:2]==['merge-base','--is-ancestor']: sys.exit(0 if os.environ['ANCESTOR']=='true' else 1)\n"
    "else: raise AssertionError(args)\n"
  )
  executable.chmod(0o755)
  temporary = tmp_path / "runner-temp"
  temporary.mkdir()
  values = {
    "PATH": str(executable.parent) + os.pathsep + os.environ["PATH"],
    "GIT_CALL_LOG": str(tmp_path / "git-calls.jsonl"),
    "CHECKOUT_SHA": "b" * 40,
    "DISPATCH_SHA": "b" * 40,
    "RESOLVED_SHA": "a" * 40,
    "SOURCE_SHA": "a" * 40,
    "SOURCE_VERSION": "1.2.0",
    "SOURCE_STATE": "release",
    "RELEASE_TAG": "v1.2.0",
    "RELEASE_SOURCE_VERSION": "1.2.0",
    "ANCESTOR": "true",
    "REPOSITORY": "example/project",
    "GITHUB_TOKEN": "synthetic-test-value",
    "GITHUB_OUTPUT": str(tmp_path / "github-output"),
    "RUNNER_TEMP": str(temporary),
  }
  for name, value in values.items():
    monkeypatch.setenv(name, value)
  state = {
    "release": {"tag_name": "v1.2.0", "immutable": True, "draft": False, "published_at": "2026-01-01T00:00:00Z"},
    "requests": [],
    "api_error": None,
    "root": tmp_path,
  }

  def urlopen(message, timeout):
    assert timeout == 30
    assert message.full_url == "https://api.github.com/repos/example/project/releases/tags/v1.2.0"
    assert message.get_header("Authorization") == "Bearer synthetic-test-value"
    state["requests"].append(message.full_url)
    if state["api_error"]:
      raise error.HTTPError(message.full_url, 404, "missing release", {}, None)
    return io.BytesIO(json.dumps(state["release"]).encode())

  monkeypatch.setattr(request, "urlopen", urlopen)
  return state


def run_resolution():
  exec(compile(inline_python("context", 2), "<workflow-source-resolution>", "exec"), {})


def test_newer_main_dispatch_resolves_and_checks_the_older_published_tag(publisher_environment):
  run_resolution()
  root = publisher_environment["root"]
  output = dict(line.split("=", 1) for line in (root / "github-output").read_text().splitlines())
  assert output == {"source_sha": "a" * 40, "version": "1.2.0", "release_tag": "v1.2.0", "state": "release"}
  calls = [json.loads(line) for line in (root / "git-calls.jsonl").read_text().splitlines()]
  assert ["fetch", "--no-tags", "origin", "refs/tags/v1.2.0"] in calls
  assert ["merge-base", "--is-ancestor", "a" * 40, "origin/main"] in calls
  assert len(publisher_environment["requests"]) == 1


def test_development_resolution_uses_dispatch_sha_without_a_release_lookup(publisher_environment, monkeypatch):
  monkeypatch.setenv("RELEASE_TAG", "")
  run_resolution()
  output = (publisher_environment["root"] / "github-output").read_text()
  assert "source_sha=" + "b" * 40 in output
  assert "version=9.9.9" in output and "state=unreleased development" in output
  assert not publisher_environment["requests"]


@pytest.mark.parametrize("annotated", [False, True])
def test_resolver_peels_real_tags_in_complete_git_history(tmp_path, monkeypatch, annotated):
  remote = tmp_path / "origin.git"
  subprocess.run(["git", "init", "--bare", str(remote)], check=True, capture_output=True)
  environment = {
    **os.environ,
    "GIT_AUTHOR_NAME": "Fixture",
    "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
    "GIT_COMMITTER_NAME": "Fixture",
    "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
  }

  def git(*args, data=None):
    return subprocess.run(
      ["git", "-c", "commit.gpgsign=false", "--git-dir", str(remote), *args],
      input=data,
      text=True,
      capture_output=True,
      check=True,
      env=environment,
    ).stdout.strip()

  def blob(text):
    return git("hash-object", "-w", "--stdin", data=text)

  document = blob("## [v1.2.0]\n")
  docs = git("mktree", data=f"100644 blob {document}\tCHANGELOG.md\n100644 blob {document}\tRELEASE_NOTES.md\n")

  def tree(version):
    project = blob(f'BUILD_VERSION: str = "{version}"\n')
    return git("mktree", data=f"040000 tree {docs}\tdocs\n100644 blob {project}\tproject.py\n")

  release = git("commit-tree", tree("1.2.0"), "-m", "Release fixture")
  dispatch = git("commit-tree", tree("9.9.9"), "-p", release, "-m", "Newer dispatch fixture")
  target = (
    git(
      "mktag",
      data=(
        f"object {release}\ntype commit\ntag v1.2.0\n"
        "tagger Fixture <fixture@example.invalid> 1700000000 +0000\n\nRelease fixture\n"
      ),
    )
    if annotated
    else release
  )
  git("update-ref", "refs/tags/v1.2.0", target)
  git("update-ref", "refs/heads/main", dispatch)
  checkout = tmp_path / "publisher"
  subprocess.run(["git", "clone", "--branch", "main", str(remote), str(checkout)], check=True, capture_output=True)
  monkeypatch.chdir(checkout)
  for key, value in {
    "RELEASE_TAG": "v1.2.0",
    "DISPATCH_SHA": dispatch,
    "REPOSITORY": "example/project",
    "GITHUB_TOKEN": "synthetic-test-value",
    "RUNNER_TEMP": str(tmp_path),
    "GITHUB_OUTPUT": str(tmp_path / "outputs"),
  }.items():
    monkeypatch.setenv(key, value)
  monkeypatch.setattr(
    request,
    "urlopen",
    lambda *_args, **_kwargs: io.BytesIO(
      json.dumps(
        {
          "tag_name": "v1.2.0",
          "immutable": True,
          "draft": False,
          "published_at": "2026-01-01T00:00:00Z",
        }
      ).encode()
    ),
  )
  run_resolution()
  result = dict(line.split("=", 1) for line in (tmp_path / "outputs").read_text().splitlines())
  assert result == {"source_sha": release, "version": "1.2.0", "release_tag": "v1.2.0", "state": "release"}
  assert release != dispatch


@pytest.mark.parametrize(
  "fault",
  ["mutable", "draft", "unpublished", "wrong-tag", "api-error", "version", "ancestry", "notes", "sha", "publisher"],
)
def test_actual_resolution_fails_closed_for_invalid_release_sources(publisher_environment, monkeypatch, fault):
  state = publisher_environment
  if fault in ("mutable", "draft", "unpublished", "wrong-tag"):
    key, value = {
      "mutable": ("immutable", False),
      "draft": ("draft", True),
      "unpublished": ("published_at", None),
      "wrong-tag": ("tag_name", "v9.9.9"),
    }[fault]
    state["release"][key] = value
  elif fault == "api-error":
    state["api_error"] = True
  else:
    name, value = {
      "version": ("RELEASE_SOURCE_VERSION", "1.1.1"),
      "ancestry": ("ANCESTOR", "false"),
      "notes": ("DOCUMENT_VERSION", "1.1.1"),
      "sha": ("RESOLVED_SHA", "main"),
      "publisher": ("CHECKOUT_SHA", "c" * 40),
    }[fault]
    monkeypatch.setenv(name, value)
  with pytest.raises((SystemExit, ValueError, RuntimeError, OSError, subprocess.CalledProcessError)):
    run_resolution()
  assert not (state["root"] / "github-output").exists()


@pytest.fixture
def publication_site(publisher_environment, monkeypatch):
  monkeypatch.setenv("CHECKOUT_SHA", "a" * 40)
  root = publisher_environment["root"]
  output = root / "build/docs-site"
  (output / "api").mkdir(parents=True)
  marker = (
    f'<aside class="source-context" data-source-sha="{"a" * 40}" data-version="1.2.0" '
    'data-state="release" data-release-tag="v1.2.0"><strong>Release v1.2.0</strong> · package version 1.2.0</aside>'
  )
  (output / "index.html").write_text(marker)
  (output / "api/index.html").write_text(marker + f"1.2.0 · release @ {'a' * 40}")
  manifest = {
    "schema": 2,
    "source_sha": "a" * 40,
    "version": "1.2.0",
    "state": "release",
    "release_tag": "v1.2.0",
    "inputs": {},
    "generators": {},
    "api_members": ["index.html"],
    "outputs": {},
  }
  return output, manifest


def seal_publication(output, manifest):
  manifest["outputs"] = {
    path.relative_to(output).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
    for path in output.rglob("*")
    if path.is_file() and path.name != "site-manifest.json"
  }
  (output / "site-manifest.json").write_text(json.dumps(manifest))


def run_independent_check():
  exec(compile(inline_python("build", 4), "<workflow-independent-identity>", "exec"), {})


def test_independent_publication_check_accepts_exact_release_source(publication_site):
  seal_publication(*publication_site)
  run_independent_check()


@pytest.mark.parametrize(
  "fault", ["source", "version", "state", "tag", "schema", "visible", "api", "checkout", "bytes", "self-consistent"]
)
def test_independent_publication_check_rejects_wrong_or_self_consistent_output(publication_site, monkeypatch, fault):
  output, manifest = publication_site
  if fault in ("source", "version", "state", "tag", "schema"):
    key, value = {
      "source": ("source_sha", "b" * 40),
      "version": ("version", "1.1.1"),
      "state": ("state", "unreleased development"),
      "tag": ("release_tag", "v1.1.1"),
      "schema": ("schema", 1),
    }[fault]
    manifest[key] = value
  elif fault in ("visible", "api"):
    path = output / "api/index.html"
    old, new = (
      ("Release v1.2.0", "Development / unreleased")
      if fault == "visible"
      else ("1.2.0 · release @", "1.1.1 · release @")
    )
    path.write_text(path.read_text().replace(old, new))
  elif fault == "checkout":
    monkeypatch.setenv("CHECKOUT_SHA", "b" * 40)
  elif fault == "self-consistent":
    manifest["source_sha"] = "b" * 40
    for path in output.rglob("*.html"):
      path.write_text(path.read_text().replace("a" * 40, "b" * 40))
  seal_publication(output, manifest)
  if fault == "bytes":
    (output / "extra.txt").write_text("not in the verified artifact")
  with pytest.raises(SystemExit):
    run_independent_check()
