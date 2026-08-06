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

from typing import Final, List, Tuple

from . import paths
from .utils import green_print, red_print, yellow_print


# The two workflows that hand an agent a repo secret. Each keeps two settings whose removal
# breaks nothing that anyone would notice: the OIDC permission the action trades for its
# GitHub App token (#148 deleted it once -- claude[bot] went silent while every other check
# stayed green), and the SHA pin on the action itself (#144 -- a floating tag lets the only
# gate on this path be rewritten upstream). Neither has a test that would go red.
AI_WORKFLOWS: Final[Tuple[str, ...]] = (
  "claude.yml",
  "claude-review.yml",
)

PINNED_ACTION: Final[str] = "anthropics/claude-code-action"

OIDC_RE: Final[re.Pattern] = re.compile(r"^\s*id-token:\s*write\b", re.MULTILINE)
USES_RE: Final[re.Pattern] = re.compile(
  rf"^\s*-?\s*uses:\s*{re.escape(PINNED_ACTION)}@(\S+)", re.MULTILINE
)
SHA_RE: Final[re.Pattern] = re.compile(r"^[0-9a-f]{40}$")


def check_ai_workflows() -> int:
  """Hold the AI workflows to the two settings of theirs that fail silently.

  Both failures are silent by construction, which is why they get a gate rather than a
  comment: dropping `id-token: write` leaves every check green and only claude[bot] mute,
  and swapping the pinned SHA back to a tag changes nothing until the day upstream moves
  it. Pure parsing -- no build needed, any leg can run it.

  Not a general "pin every action" rule: the repo's other actions are first-party and none
  of them is handed a secret, so widening this gate would turn it into noise (#144).
  """
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

    text = path.read_text()

    if not OIDC_RE.search(text):
      failures.append(
        f"{name}: no `id-token: write`. The action trades it for its GitHub App token, so "
        f"removing it silences claude[bot] while the rest of CI stays green (#148)"
      )

    refs = USES_RE.findall(text)
    if not refs:
      # A gate that cannot find what it guards passes vacuously; say so instead.
      failures.append(
        f"{name}: no `uses: {PINNED_ACTION}` line. Either this check's parsing broke, or the "
        f"workflow no longer calls the action and belongs out of AI_WORKFLOWS"
      )
    failures.extend(
      f"{name}: {PINNED_ACTION} is at `{ref}`, not a commit SHA. It is the only third-party "
      f"action here handed a repo secret; a floating tag lets upstream rewrite it (#144)"
      for ref in refs if not SHA_RE.match(ref)
    )

  print("#" * 60)
  if failures:
    red_print(f"AI-workflow gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"AI workflows keep OIDC and a pinned action ({len(AI_WORKFLOWS)} files)")
  return 0
