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
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

from automation.ai_workflows import AI_WORKFLOWS, check_ai_workflows


WORKFLOWS = Path(__file__).parents[2] / ".github" / "workflows"


@pytest.fixture
def workflows(tmp_path):
  for name in AI_WORKFLOWS:
    shutil.copyfile(WORKFLOWS / name, tmp_path / name)
  return tmp_path


def test_repository_ai_workflows():
  assert check_ai_workflows() == 0


@pytest.mark.parametrize(
  "name,path,value",
  [
    ("claude-review.yml", ("if",), "true"),
    ("claude-review.yml", ("steps", 0, "with", "ref"), "${{ github.ref }}"),
    ("claude-review.yml", ("steps", 0, "uses"), "actions/checkout@v7"),
    ("claude-review.yml", ("steps", 0, "with", "persist-credentials"), True),
    ("claude-review.yml", ("permissions", "id-token"), "read"),
    ("claude-review.yml", ("steps", -1, "uses"), "anthropics/claude-code-action@v1"),
    ("claude-review.yml", ("steps", -1, "with", "prompt"), ""),
    ("claude-review.yml", ("steps", -1, "with", "claude_args"), '--tools "Bash"'),
    ("claude.yml", ("steps", 0, "uses"), "actions/checkout@v7"),
    ("claude.yml", ("steps", 0, "with", "persist-credentials"), True),
    ("claude.yml", ("steps", -1, "with", "prompt"), "This silently changes the action mode."),
    ("claude.yml", ("steps", -1, "with", "include_comments_by_actor"), "*"),
    ("claude.yml", ("steps", -1, "with", "claude_args"), '--allowedTools "Read"'),
  ],
)
def test_ai_gate_rejects_startup_and_mode_drift(workflows, name, path, value):
  file = workflows / name
  workflow = yaml.safe_load(file.read_text(encoding="utf-8"))
  target = next(iter(workflow["jobs"].values()))
  for key in path[:-1]:
    target = target[key]
  target[path[-1]] = value
  file.write_text(yaml.safe_dump(workflow), encoding="utf-8")
  assert check_ai_workflows(workflows) == 1


@pytest.mark.parametrize(
  "name,old,new",
  [
    ("claude-review.yml", "gh pr diff ${{ inputs.pr_number }} --repo ${{ github.repository }}:*", "gh pr diff:*"),
    ("claude-review.yml", '--tools "Bash,Read,Glob,Grep"', '--tools "Bash,Read,Glob,Grep,Write"'),
    ("claude-review.yml", "Edit,MultiEdit,Write", "Edit,MultiEdit"),
    ("claude.yml", '"Bash,Edit,MultiEdit', '"Edit,MultiEdit'),
    ("claude.yml", "mcp__github_file_ops__commit_files,", ""),
    ("claude.yml", "mcp__github_ci__download_job_log,", ""),
    ("claude.yml", '--tools "Read,Glob,Grep,ToolSearch"', '--tools "Read,Glob,Grep"'),
    ("claude.yml", '--tools "Read,Glob,Grep,ToolSearch"', '--tools "Read,Glob,Grep,ToolSearch" --tools "Bash"'),
    ("claude.yml", '--tools "Read,Glob,Grep,ToolSearch"', '--tools "Read,Glob,Grep,ToolSearch" --mcp-config evil.json'),
  ],
)
def test_ai_gate_rejects_tool_expansion(workflows, name, old, new):
  file = workflows / name
  text = file.read_text(encoding="utf-8")
  assert old in text
  file.write_text(text.replace(old, new), encoding="utf-8")
  assert check_ai_workflows(workflows) == 1


def test_ai_gate_rejects_missing_action_and_divergent_pins(workflows):
  file = workflows / "claude.yml"
  text = file.read_text(encoding="utf-8")
  workflow = yaml.safe_load(text)
  workflow["jobs"]["claude"]["steps"].pop()
  file.write_text(yaml.safe_dump(workflow), encoding="utf-8")
  assert check_ai_workflows(workflows) == 1

  original = yaml.safe_load(text)["jobs"]["claude"]["steps"][-1]["uses"]
  file.write_text(text.replace(original, "anthropics/claude-code-action@" + "a" * 40), encoding="utf-8")
  assert check_ai_workflows(workflows) == 1


def test_ai_execution_recipes_preserve_triggers_permissions_and_credential_scope():
  for name in AI_WORKFLOWS:
    workflow = yaml.load((WORKFLOWS / name).read_text(encoding="utf-8"), Loader=yaml.BaseLoader)
    manual = name == "claude-review.yml"
    assert set(workflow) == {"name", "on", "jobs"}
    assert set(workflow["jobs"]) == {"review" if manual else "claude"}
    job = next(iter(workflow["jobs"].values()))
    assert set(job) == {"if", "runs-on", "permissions", "steps"}
    assert job["runs-on"] == "ubuntu-latest"
    assert job["permissions"] == (
      {"contents": "read", "pull-requests": "write", "id-token": "write"}
      if manual
      else {"contents": "read", "pull-requests": "read", "issues": "read", "id-token": "write"}
    )
    steps = job["steps"]
    assert len(steps) == (3 if manual else 2)
    assert set(steps[0]) == {"uses", "with"}
    assert steps[0]["with"] == (
      {"ref": "${{ github.event.repository.default_branch }}", "fetch-depth": "1", "persist-credentials": "false"}
      if manual
      else {"fetch-depth": "1", "persist-credentials": "false"}
    )
    action = steps[-1]
    assert set(action) == {"uses", "with"}
    assert set(action["with"]) == (
      {"anthropic_api_key", "prompt", "claude_args"}
      if manual
      else {"anthropic_api_key", "include_comments_by_actor", "claude_args"}
    )
    assert action["with"]["anthropic_api_key"] == "${{ secrets.ANTHROPIC_API_KEY }}"
    if manual:
      assert workflow["on"] == {
        "workflow_dispatch": {
          "inputs": {"pr_number": {"description": "PR number to review", "required": "true", "type": "number"}}
        }
      }
      assert set(steps[1]) == {"name", "run", "env"}
      assert steps[1]["env"] == {
        "PR_NUMBER": "${{ inputs.pr_number }}",
        "GH_TOKEN": "${{ secrets.GITHUB_TOKEN }}",
      }
    else:
      assert workflow["on"] == {
        "issue_comment": {"types": ["created"]},
        "issues": {"types": ["opened"]},
        "pull_request_review": {"types": ["submitted"]},
      }
      assert " ".join(job["if"].split()) == (
        "(github.event_name == 'issue_comment' && contains(github.event.comment.body, '@claude') "
        "&& github.event.comment.author_association == 'OWNER') || "
        "(github.event_name == 'pull_request_review' && contains(github.event.review.body, '@claude') "
        "&& github.event.review.author_association == 'OWNER') || "
        "(github.event_name == 'issues' && (contains(github.event.issue.body, '@claude') "
        "|| contains(github.event.issue.title, '@claude')) && github.event.issue.author_association == 'OWNER')"
      )


@pytest.mark.parametrize("fork", [False, True], ids=["same-repository", "fork-main"])
@pytest.mark.parametrize("old_checkout", [False, True], ids=["trusted-startup", "old-checkout-control"])
def test_manual_preparation_keeps_pr_startup_files_out(tmp_path, fork, old_checkout):
  workflow = yaml.safe_load((WORKFLOWS / "claude-review.yml").read_text(encoding="utf-8"))
  steps = workflow["jobs"]["review"]["steps"]
  assert steps[0]["with"]["ref"] == "${{ github.event.repository.default_branch }}"
  workspace = tmp_path / "trusted-main"
  workspace.mkdir()
  pr_tree = tmp_path / ("fork/main" if fork else "repository/pr-283")
  (pr_tree / ".claude").mkdir(parents=True)
  (pr_tree / "CLAUDE.md").write_text("PR-only instructions\n", encoding="utf-8")
  marker = tmp_path / "startup-hook-ran"
  command = f"{shlex.quote(sys.executable)} -c " + shlex.quote(
    f"from pathlib import Path; Path({str(marker)!r}).touch()"
  )
  (pr_tree / ".claude/settings.json").write_text(
    json.dumps({"hooks": {"SessionStart": [{"hooks": [{"type": "command", "command": command}]}]}}),
    encoding="utf-8",
  )
  bin_dir = tmp_path / "bin"
  bin_dir.mkdir()
  gh = bin_dir / "gh"
  gh.write_text(
    f"#!{sys.executable}\n"
    "import os, shutil, sys\n"
    "if sys.argv[1:] == ['pr', 'view', '283', '--repo', 'owner/repository', '--json', 'number']:\n"
    "  print('{\"number\": 283}')\n"
    "elif sys.argv[1:] == ['pr', 'checkout', '--detach', '283']:\n"
    "  shutil.copytree(os.environ['PR_TREE'], '.', dirs_exist_ok=True)\n"
    "else:\n"
    "  raise SystemExit('unexpected gh command')\n",
    encoding="utf-8",
  )
  gh.chmod(0o755)
  env = {
    "PATH": str(bin_dir) + os.pathsep + os.defpath,
    "PR_NUMBER": "283",
    "GITHUB_REPOSITORY": "owner/repository",
    "GH_TOKEN": "inert-test-token",
    "PR_TREE": str(pr_tree),
  }
  preparation = 'gh pr checkout --detach "$PR_NUMBER"' if old_checkout else steps[1]["run"]
  subprocess.run(["bash", "-e", "-c", preparation], cwd=workspace, env=env, check=True, capture_output=True)

  # Model command-hook loading with an inert sentinel, not the real CLI.
  startup = (
    "import json, subprocess\n"
    "from pathlib import Path\n"
    "settings = Path('.claude/settings.json')\n"
    "if settings.exists():\n"
    "  for entry in json.loads(settings.read_text())['hooks']['SessionStart']:\n"
    "    for hook in entry['hooks']:\n"
    "      subprocess.run(hook['command'], shell=True, check=True)\n"
  )
  subprocess.run([sys.executable, "-c", startup], cwd=workspace, env=env, check=True, capture_output=True)
  assert marker.exists() is old_checkout
  assert (workspace / "CLAUDE.md").exists() is old_checkout


@pytest.mark.parametrize("number", ["", "0", "-1", "1.5", "1e3", "1; exit 0"])
def test_manual_preparation_rejects_non_positive_integer_input(tmp_path, number):
  workflow = yaml.safe_load((WORKFLOWS / "claude-review.yml").read_text(encoding="utf-8"))
  preparation = workflow["jobs"]["review"]["steps"][1]["run"]
  # No gh executable is available: invalid input must exit before reaching it.
  result = subprocess.run(
    [shutil.which("bash"), "-e", "-c", preparation],
    cwd=tmp_path,
    env={"PATH": str(tmp_path), "PR_NUMBER": number},
    capture_output=True,
    text=True,
    check=False,
  )
  assert result.returncode == 1
  assert not result.stdout
  assert result.stderr == "PR number must be a positive integer.\n"


@pytest.mark.parametrize("quoted", [True, False], ids=["quoted-heredoc", "unquoted-control"])
def test_manual_comment_template_preserves_markdown(tmp_path, quoted):
  workflow = yaml.safe_load((WORKFLOWS / "claude-review.yml").read_text(encoding="utf-8"))
  prompt = workflow["jobs"]["review"]["steps"][-1]["with"]["prompt"]
  command = prompt.split("```sh\n")[1].split("```")[0]
  command = command.replace("${{ inputs.pr_number }}", "283").replace("${{ github.repository }}", "owner/repository")
  body = 'Don\'t expand `touch "$SIDE_EFFECT"` or $(printf expanded).\n'
  command = command.replace("<review text>\n", body)
  if not quoted:
    command = command.replace("<<'REVIEW'", "<<REVIEW")
  capture = "import json, sys; print(json.dumps({'args': sys.argv[1:], 'body': sys.stdin.read()}))"
  shell = f'gh() {{ {shlex.quote(sys.executable)} -c {shlex.quote(capture)} "$@"; }}\n' + command
  marker = tmp_path / "shell-substitution"
  result = subprocess.run(
    ["bash", "-e", "-c", shell],
    cwd=tmp_path,
    env={"PATH": os.defpath, "SIDE_EFFECT": str(marker)},
    capture_output=True,
    text=True,
    check=True,
  )
  received = json.loads(result.stdout)
  assert received["args"] == ["pr", "comment", "283", "--repo", "owner/repository", "--body-file", "-"]
  assert (received["body"] == body) is quoted
  assert marker.exists() is not quoted
