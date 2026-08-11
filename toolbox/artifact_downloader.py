#!/usr/bin/env python3
#
# Helper to download build artifacts from GitHub.
#
#########################################################################################
#
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

import sys
import os
import shutil
import pprint
import argparse
import subprocess

from pathlib import Path
from typing import Final

# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import red_print, yellow_print, blue_print
from automation.github import GitHub

def artifact_workflow(workflow_name: str = "Build and Test on Multiple Platforms") -> GitHub.Workflow:
  """Find the workflow to download artifacts from."""
  multi_platform_workflow = list(filter(
    lambda wf: wf.name == workflow_name, 
    GitHub.list_workflows()
  ))

  if len(multi_platform_workflow) != 1:
    red_print(f'Cannot find the workflow "{workflow_name}"')
    red_print(f"Found {len(multi_platform_workflow)} workflows:")
    red_print(pprint.pformat(multi_platform_workflow))
    raise RuntimeError(f'Cannot find the workflow "{workflow_name}"')
  
  return multi_platform_workflow[0]


# A release ships the artifacts of the tagged commit from BOTH build legs: the platform
# packages and the wasm module (`celestial-wasm`). Each source names its expected minimum
# count -- a successful run with fewer artifacts must fail loudly: a release must never
# ship with artifacts silently missing. The minimums track the legs' upload steps; a PR
# that adds or removes an upload updates its number here.
ARTIFACT_SOURCES: Final[tuple[tuple[str, int], ...]] = (
  ("Build and Test on Multiple Platforms", 4),
  ("WASM Build and Golden Check", 1),
)


def release_commit_sha() -> str:
  """The commit the artifacts must have been built from.

  `git rev-parse HEAD` first: release.yml checks the tag out, so HEAD there is the tag's
  commit (already peeled, even for an annotated tag), and anywhere else HEAD is simply
  the commit the caller means. GITHUB_SHA is the fallback. The rev-parse runs at the
  repo root -- the caller's cwd may not be a repo at all.
  """
  try:
    ret = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True,
                         cwd=Path(__file__).parent.parent)
  except FileNotFoundError:  # no git binary at all -- fall through to GITHUB_SHA
    ret = None
  if ret is not None and ret.returncode == 0:
    return ret.stdout.strip()
  sha = os.environ.get("GITHUB_SHA")
  if sha:
    return sha
  raise RuntimeError("Cannot resolve the release commit: `git rev-parse HEAD` failed and GITHUB_SHA is unset")


def find_artifact_run(workflow: GitHub.Workflow, sha: str) -> GitHub.Run:
  """Find the run of `workflow` that built `sha` and succeeded.

  Fail loud when there is none -- never settle for "the latest run", which can belong
  to an unrelated, even failing, WIP branch: a release ships the artifacts of the
  commit it tags, or it does not ship.
  """
  runs, pages = GitHub.list_workflow_runs(workflow.id)
  for run in runs:
    if run.head_sha == sha and run.conclusion == "success":
      return run
  red_print(f'No successful "{workflow.name}" run found for commit {sha} '
            f"(scanned {pages} page(s), {len(runs)} runs)")
  raise RuntimeError(f'No successful "{workflow.name}" run for commit {sha}')


def parse_args() -> argparse.Namespace:
  """Parse the command line arguments."""
  parser = argparse.ArgumentParser(
    description="CelestialCalendar automation script for downloading artifacts."
  )
  parser.add_argument(
    "-id", "--run-id",
    type=int, 
    required=False,
    default=0,
    help="The ID of a single artifact run to download, skipping the dual-source lookup. "
         "If not specified, the successful runs that built the release commit are used."
  )
  parser.add_argument(
    "-s", "--save-to", 
    type=lambda arg: Path(arg).resolve(),
    required=True, 
    help="Directory path to save the downloaded artifacts."
  )
  parser.add_argument(
    "-p", "--parallel", 
    type=int, 
    default=4, 
    help="Number of parallel downloads (default: 4)."
  )
  parser.add_argument(
    "--unzip",
    action="store_true",
    help="Unzip the downloaded artifacts."
  )

  return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None: # Exception raised on failure.
  """Validate the command line arguments."""
  # Validate the number of parallel downloads.
  if args.parallel < 1:
    red_print(f"Invalid number of parallel downloads: {args.parallel}")
    raise RuntimeError(f"Invalid number of parallel downloads: {args.parallel}")

  # Validate the directory path.
  if args.save_to.exists() and args.save_to.is_file():
    red_print(f"Directory path is not a directory: {args.save_to}")
    raise RuntimeError(f"Directory path is not a directory: {args.save_to}")
  

def main() -> None:
  """Main function to download artifacts."""
  # Parse the command line arguments.
  args = parse_args()
  validate_args(args)

  downloaded_artifacts = []

  if args.run_id != 0:
    # --run-id pins one specific run and skips the dual-source lookup entirely
    # (and with it the per-source minimum -- the caller asked for this run deliberately).
    downloaded_artifacts = GitHub.download_artifacts(args.run_id, args.save_to, args.parallel)
  else:
    sha = release_commit_sha()
    for workflow_name, min_artifacts in ARTIFACT_SOURCES:
      run = find_artifact_run(artifact_workflow(workflow_name), sha)
      downloaded = GitHub.download_artifacts(run.id, args.save_to, args.parallel)
      if len(downloaded) < min_artifacts:
        red_print(
          f'"{workflow_name}" run {run.id} (commit {sha}) yielded {len(downloaded)} '
          f"artifact(s), expected at least {min_artifacts}"
        )
        raise RuntimeError(
          f'"{workflow_name}" run {run.id} has {len(downloaded)} of ≥{min_artifacts} '
          "artifacts -- a release must never ship with artifacts silently missing"
        )
      downloaded_artifacts.extend(downloaded)

  # Unzip the downloaded artifacts.
  if args.unzip:
    for artifact in downloaded_artifacts:
      # Get filename without extension (.zip)
      stem = artifact.stem
      extract_dir = args.save_to / stem
      extract_dir.mkdir(parents=True, exist_ok=True)

      shutil.unpack_archive(artifact, extract_dir=extract_dir)
      blue_print(f"# Unzipped {artifact}")

      artifact.unlink()
      yellow_print(f"# Deleted {artifact}")


if __name__ == "__main__":
  main()
