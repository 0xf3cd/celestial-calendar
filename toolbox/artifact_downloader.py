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
import shutil
import pprint
import argparse

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
    red_print('Cannot find the workflow "Build and Test on Multiple Platforms"')
    red_print(f"Found {len(multi_platform_workflow)} workflows:")
    red_print(pprint.pformat(multi_platform_workflow))
    raise RuntimeError('Cannot find the workflow "Build and Test on Multiple Platforms"')
  
  return multi_platform_workflow[0]


def latest_artifact_run() -> GitHub.Run:
  """Find the latest artifact run."""
  workflow = artifact_workflow()

  artifact_runs = list(filter(
    lambda run: run.workflow_id == workflow.id, 
    GitHub.list_runs()
  ))

  if len(artifact_runs) == 0:
    red_print("Cannot find any artifact runs")
    raise RuntimeError("Cannot find any artifact runs")
  
  # Find the latest run.
  latest_run = sorted(artifact_runs, key=lambda run: run.created_at, reverse=True)[0]

  # Ensure the run is successful.
  if latest_run.status != "completed":
    red_print(f"Cannot download artifacts from a non-completed run: {latest_run}")
    raise RuntimeError("Cannot download artifacts from a non-completed run")
  
  return latest_run


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
    run = latest_artifact_run()
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
