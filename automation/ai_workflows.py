# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import re

from typing import Any, Dict, Final, List, Optional, Tuple

from . import paths
from .utils import green_print, red_print, yellow_print


# The workflows that hand an agent the API key, and the only ones this gate covers. A third
# one belongs in this tuple the day it appears; `release.yml` pins its publisher by hand and
# stays outside on purpose -- holding all 18 `actions/*@v7` references to a SHA would drown
# the gate in noise (#144).
AI_WORKFLOWS: Final[Tuple[str, ...]] = (
  "claude.yml",
  "claude-review.yml",
)

PINNED_ACTION: Final[str] = "anthropics/claude-code-action"

SHA_RE: Final[re.Pattern] = re.compile(r"^[0-9a-f]{40}$")


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


def check_ai_workflows() -> int:
  """Hold the AI workflows to two settings whose loss nothing else would report.

  Deleting `id-token: write` leaves every check green and only claude[bot] mute (#148);
  swapping the pinned SHA back to a tag changes nothing until the day upstream moves it
  (#144). Both are read from the parsed YAML, not from the file text: the permission is
  per-job and has to be checked on the job that calls the action, and a workflow's prose
  (`prompt:` blocks quote this repo's own CI) must not be able to satisfy the gate.
  Pure parsing -- no build needed, any leg can run it.
  """
  # Imported here, not at module scope: `automation/__init__` is on the import path of every
  # `project.py` invocation, including CI steps that never install Requirements.txt.
  import yaml

  print("#" * 60)
  yellow_print("Checking the AI workflows keep their OIDC permission and pinned action...")

  workflow_dir = paths.proj_root() / ".github" / "workflows"
  failures: List[str] = []

  for name in AI_WORKFLOWS:
    path = workflow_dir / name
    if not path.is_file():
      failures.append(
        f"{name}: not found. This gate names the workflows it guards -- drop the name from "
        f"AI_WORKFLOWS if the workflow is gone on purpose"
      )
      continue

    try:
      workflow: Dict[str, Any] = yaml.safe_load(path.read_text())
    except yaml.YAMLError as exc:
      failures.append(f"{name}: not valid YAML, so GitHub cannot run it either ({exc.__class__.__name__})")
      continue

    jobs = workflow.get("jobs") if isinstance(workflow, dict) else None
    calling_jobs = {
      job_id: job
      for job_id, job in (jobs or {}).items()
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
        if not SHA_RE.match(ref):
          failures.append(
            f"{name}: job `{job_id}` calls the action at `{ref}`, not a commit SHA. It is the "
            f"only third-party action here handed a repo secret; a floating tag lets upstream "
            f"rewrite it (#144)"
          )

  print("#" * 60)
  if failures:
    red_print(f"AI-workflow gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"AI workflows keep OIDC and a pinned action ({len(AI_WORKFLOWS)} files)")
  return 0
