# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import re
import shlex

from pathlib import Path
from typing import Any, Dict, Final, List, Optional, Tuple

from . import paths
from .utils import green_print, red_print, yellow_print


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
      workflow: Dict[str, Any] = yaml.safe_load(path.read_text(encoding="utf-8"))
    except yaml.YAMLError as exc:
      failures.append(f"{name}: not valid YAML, so GitHub cannot run it either ({exc.__class__.__name__})")
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
        if checkout.get("with", {}).get("persist-credentials") is not False:
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
        tools = {"Read", "Glob", "Grep"} | ({"Bash"} if manual else set())
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
