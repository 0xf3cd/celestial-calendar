# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import re
import shlex
import json

from pathlib import Path
from typing import Any, Dict, Final, List, Optional, Tuple

from . import paths
from .utils import green_print, red_print, yellow_print
from .workflow_actions import load_workflow


# The workflows that hand an agent the API key, and the only ones this gate covers. A third
# one belongs in this tuple the day it appears; `release.yml` pins its publisher by hand and
# stays outside on purpose -- holding every first-party action reference to a SHA would
# drown the gate in noise (#144).
AI_WORKFLOWS: Final[Tuple[str, ...]] = (
  "claude.yml",
  "claude-review.yml",
)

PINNED_ACTION: Final[str] = "anthropics/claude-code-action"

SHA_RE: Final[re.Pattern] = re.compile(r"^[0-9a-f]{40}$")
DISPATCH_CONDITION: Final[str] = "github.ref == format('refs/heads/{0}', github.event.repository.default_branch)"


def _uses_target(step: Any) -> Optional[str]:
  """The action a step calls, or None if the step calls no action."""
  if not isinstance(step, dict):
    return None
  uses = step.get("uses")
  return uses if isinstance(uses, str) else None


def _grants_oidc(permissions: Any) -> bool:
  """Whether a `permissions:` value grants `id-token: write`."""
  if permissions == "write-all":
    return True
  return isinstance(permissions, dict) and permissions.get("id-token") == "write"


def validate_credential_policy(workflow: dict, manual: bool) -> None:
  def require(condition, message):
    if not condition:
      raise ValueError(message)

  require(len(workflow["jobs"]) == 1, "exactly one credentialed job is required")
  job = next(iter(workflow["jobs"].values()))
  require(set(workflow) <= {"name", "on", "jobs"}, "no additional workflow execution context")
  require(set(job) <= {"name", "if", "runs-on", "permissions", "steps"}, "no additional credentialed job context")
  require(job["runs-on"] == "ubuntu-latest", "credentialed jobs use the hosted runner")
  require(
    job["permissions"]
    == (
      {"contents": "read", "pull-requests": "write", "id-token": "write"}
      if manual
      else {"contents": "read", "pull-requests": "read", "issues": "read", "id-token": "write"}
    ),
    "credentialed permissions differ",
  )
  steps = job["steps"]
  require(len(steps) == (3 if manual else 2), "no extra credentialed steps")
  require(
    set(steps[0]["with"])
    == ({"fetch-depth", "persist-credentials", "ref"} if manual else {"fetch-depth", "persist-credentials"}),
    "checkout inputs differ",
  )
  require(
    set(steps[-1]["with"])
    == (
      {"anthropic_api_key", "claude_args", "prompt"}
      if manual
      else {"anthropic_api_key", "claude_args", "include_comments_by_actor"}
    ),
    "action inputs must not expand permissions, startup settings or executables",
  )
  for index, step in enumerate(steps):
    if manual and index == 1:
      require(set(step) <= {"name", "env", "run"}, "no additional preparation context")
      require(set(step["env"]) == {"PR_NUMBER", "GH_TOKEN"}, "preparation credential scope differs")
    else:
      require(set(step) <= {"name", "uses", "with"}, "no additional action execution context")
  require(steps[-1]["with"]["anthropic_api_key"] == "${{ secrets.ANTHROPIC_API_KEY }}", "API key consumer differs")
  expressions = re.findall(r"\$\{\{.*?\}\}", json.dumps(workflow))
  credentials = sorted(
    expression
    for expression in expressions
    if re.search(r"\bsecrets\b|\bgithub\b(?!\s*\.\s*(?!token\b)[a-z_]\w*)", expression, re.IGNORECASE)
  )
  require(
    credentials == sorted(["${{ secrets.ANTHROPIC_API_KEY }}"] + (["${{ secrets.GITHUB_TOKEN }}"] if manual else [])),
    "credentials must remain at their required consumers",
  )
  if manual:
    require(set(workflow["on"]) == {"workflow_dispatch"}, "manual trigger differs")
    require(steps[0]["with"]["ref"] == "${{ github.event.repository.default_branch }}", "untrusted checkout ref")
    number = workflow["on"]["workflow_dispatch"]["inputs"]["pr_number"]
    require(number["required"] == "true" and number["type"] == "number", "PR input must be a required number")
    require(steps[1]["env"]["PR_NUMBER"] == "${{ inputs.pr_number }}", "PR input wiring differs")
    require(steps[1]["env"]["GH_TOKEN"] == "${{ secrets.GITHUB_TOKEN }}", "preparation token differs")
  else:
    require(set(workflow["on"]) == {"issue_comment", "issues", "pull_request_review"}, "mention triggers differ")
    for event, kind in (("issue_comment", "created"), ("issues", "opened"), ("pull_request_review", "submitted")):
      require(workflow["on"][event]["types"] == [kind], f"{event} trigger types differ")
    require(
      " ".join(job["if"].split())
      == (
        "(github.event_name == 'issue_comment' && contains(github.event.comment.body, '@claude') "
        "&& github.event.comment.author_association == 'OWNER') || "
        "(github.event_name == 'pull_request_review' && contains(github.event.review.body, '@claude') "
        "&& github.event.review.author_association == 'OWNER') || "
        "(github.event_name == 'issues' && (contains(github.event.issue.body, '@claude') "
        "|| contains(github.event.issue.title, '@claude')) && github.event.issue.author_association == 'OWNER')"
      ),
      "mention authorization differs",
    )


def check_ai_workflows(workflow_dir: Path | None = None) -> int:
  """Check credentialed AI startup, action pins, permissions and tool restrictions.

  Deleting `id-token: write` leaves every check green and only claude[bot] mute (#148);
  swapping the pinned SHA back to a tag changes nothing until the day upstream moves it
  (#144). Both are read from the parsed YAML, not from the file text: the permission is
  per-job and has to be checked on the job that calls the action, and a workflow's prose
  (`prompt:` blocks quote this repo's own CI) must not be able to satisfy the gate.
  Pure parsing -- no build needed, so any leg that installs Requirements.txt can run it.
  """
  try:
    import yaml
  except ModuleNotFoundError:
    red_print("This check needs PyYAML: run `pip install -r Requirements.txt` first")
    return 1

  print("#" * 60)
  yellow_print("Checking AI workflow startup, action pins and tools...")

  workflow_dir = workflow_dir or paths.proj_root() / ".github" / "workflows"
  failures: List[str] = []
  action_refs = set()

  for name in AI_WORKFLOWS:
    path = workflow_dir / name
    if not path.is_file():
      failures.append(
        f"{name}: not found. This gate names the workflows it guards -- drop the name from "
        f"AI_WORKFLOWS if the workflow is gone on purpose"
      )
      continue

    try:
      workflow: Dict[str, Any] = load_workflow(path)
      validate_credential_policy(workflow, name == "claude-review.yml")
    except (OSError, ValueError, KeyError, TypeError, IndexError, AttributeError, yaml.YAMLError) as exc:
      failures.append(f"{name}: credential policy: {exc}")
      continue

    jobs = workflow.get("jobs") if isinstance(workflow, dict) else None
    calling_jobs = {
      job_id: job
      for job_id, job in (jobs if isinstance(jobs, dict) else {}).items()
      if isinstance(job, dict)
      for step in (job.get("steps") or [])
      if (_uses_target(step) or "").split("@")[0] == PINNED_ACTION
    }

    if not calling_jobs:
      # A gate that cannot find what it guards passes vacuously; say so instead.
      failures.append(
        f"{name}: no job calls {PINNED_ACTION}. Either this check's parsing broke, or the "
        f"workflow no longer calls the action and belongs out of AI_WORKFLOWS"
      )
      continue

    for job_id, job in calling_jobs.items():
      # A job's `permissions:` replaces the workflow-level block outright, it does not merge.
      permissions = job["permissions"] if "permissions" in job else workflow.get("permissions")
      if not _grants_oidc(permissions):
        failures.append(
          f"{name}: job `{job_id}` calls the action without `id-token: write`. The action "
          f"trades it for its GitHub App token, so the job silences claude[bot] while the "
          f"rest of CI stays green (#148)"
        )

      for step in job["steps"]:
        uses = _uses_target(step) or ""
        if uses.split("@")[0] != PINNED_ACTION:
          continue
        ref = uses.partition("@")[2]
        action_refs.add(ref)
        if not SHA_RE.match(ref):
          failures.append(
            f"{name}: job `{job_id}` calls the action at `{ref}`, not a commit SHA. It is the "
            f"only third-party action here handed a repo secret; a floating tag lets upstream "
            f"rewrite it (#144)"
          )

        manual = name == "claude-review.yml"
        checkouts = [s for s in job["steps"] if (_uses_target(s) or "").startswith("actions/checkout@")]
        if len(checkouts) != 1 or job["steps"][0] != checkouts[0]:
          failures.append(f"{name}: exactly one initial checkout is required")
          continue
        checkout = checkouts[0]
        if not SHA_RE.fullmatch(checkout["uses"].partition("@")[2]):
          failures.append(f"{name}: the credentialed checkout must use a full commit SHA")
        if checkout.get("with", {}).get("persist-credentials") != "false":
          failures.append(f"{name}: checkout must not persist credentials")
        if manual and (
          job.get("if") != DISPATCH_CONDITION
          or checkout.get("with", {}).get("ref") != "${{ github.event.repository.default_branch }}"
        ):
          failures.append(f"{name}: dispatch and checkout must use the trusted default branch")

        inputs = step.get("with", {})
        if bool(inputs.get("prompt")) != manual:
          failures.append(f"{name}: preserve manual agent mode and mention tag mode")
        if not manual and inputs.get("include_comments_by_actor") != "0xf3cd, claude":
          failures.append(f"{name}: preserve mention comment filtering")
        try:
          tokens = shlex.split(inputs.get("claude_args", ""))
          args = dict(zip(tokens[::2], tokens[1::2], strict=True))
        except (TypeError, ValueError):
          failures.append(f"{name}: Claude arguments must be flag/value pairs")
          continue
        expected_flags = {"--tools", "--disallowedTools"} | (
          {"--model", "--allowedTools"} if manual else {"--append-system-prompt"}
        )
        if set(args) != expected_flags or len(args) * 2 != len(tokens):
          failures.append(f"{name}: Claude tool flags differ or repeat")
          continue
        tools = {"Read", "Glob", "Grep"} | ({"Bash"} if manual else {"ToolSearch"})
        denied = {"Edit", "MultiEdit", "Write", "NotebookEdit", "Agent", "Task"}
        if not manual:
          denied |= {
            "Bash",
            "mcp__github_ci__get_ci_status",
            "mcp__github_ci__get_workflow_run_details",
            "mcp__github_ci__download_job_log",
            "mcp__github_file_ops__commit_files",
            "mcp__github_file_ops__delete_files",
          }
        if set(args["--tools"].split(",")) != tools or set(args["--disallowedTools"].split(",")) != denied:
          failures.append(f"{name}: available tools or explicit denials differ")
        if manual:
          target = "${{ inputs.pr_number }} --repo ${{ github.repository }}"
          allowed = {f"Bash(gh pr {command} {target}:*)" for command in ("view", "diff", "comment")}
          if set(args["--allowedTools"].split(",")) != allowed:
            failures.append(f"{name}: shell commands must be bound to the requested repository and PR")
        elif not args["--append-system-prompt"].strip():
          failures.append(f"{name}: mention instructions must be appended without replacing tag mode")

  if len(action_refs) > 1:
    failures.append("AI workflows must use the same Claude action commit")

  print("#" * 60)
  if failures:
    red_print(f"AI-workflow gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"AI workflow startup, pins and tools satisfy policy ({len(AI_WORKFLOWS)} files)")
  return 0
