#!/usr/bin/env python3
#
# Helper to download artifacts (latest builds) from GitHub.
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

# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import (
  GitHub, red_print, yellow_print, blue_print,
)

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


def release_commit_sha() -> str:
  """The commit the artifacts must have been built from.

  In CI (release.yml runs on a tag push) GITHUB_SHA is the tag's commit; locally, fall
  back to `git rev-parse HEAD` -- release.yml checks the tag out, so HEAD there is the
  tag's commit, and anywhere else HEAD is simply the commit the caller means.
  """
  sha = os.environ.get("GITHUB_SHA")
  if sha:
    return sha
  ret = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True)
  if ret.returncode != 0:
    raise RuntimeError("Cannot resolve the release commit: GITHUB_SHA is unset and `git rev-parse HEAD` failed")
  return ret.stdout.strip()


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
    help="The ID of the artifact run. If not specified, the latest artifact run will be used."
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
  
  # Download artifacts.
  run_id = args.run_id
  if run_id == 0:
    run = find_artifact_run(artifact_workflow(), release_commit_sha())
    run_id = run.id
  
  downloaded_artifacts = GitHub.download_artifacts(run_id, args.save_to, args.parallel)

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
