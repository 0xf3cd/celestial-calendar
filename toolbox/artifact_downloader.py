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
import hashlib
import re
import shutil
import pprint
import argparse
import subprocess
import tempfile
import zipfile

from collections import Counter

from pathlib import Path
from typing import Final

# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import red_print, yellow_print, blue_print
from automation.github import GitHub
from toolbox.release_validation import validate_release_archives

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


# A release takes one exact inventory from each independent build leg. Missing, extra, or
# duplicate artifacts are all contract drift; checking only a minimum would bless the wrong run.
ARTIFACT_SOURCES: Final[tuple[tuple[str, frozenset[str]], ...]] = (
  (
    "Build and Test on Multiple Platforms",
    frozenset({"linux_amd64", "linux_arm64", "macos_arm64", "windows_x86_64"}),
  ),
  ("WASM Build and Golden Check", frozenset({"celestial-wasm"})),
  (
    "Python Wheels",
    frozenset(
      {
        "celestial-python-manylinux-x86_64",
        "celestial-python-manylinux-aarch64",
        "celestial-python-macos-arm64",
        "celestial-python-windows-amd64",
      }
    ),
  ),
)

PYTHON_ARTIFACTS: Final[dict[str, tuple[str, str]]] = {
  "celestial-python-manylinux-x86_64": ("manylinux", "x86_64"),
  "celestial-python-manylinux-aarch64": ("manylinux", "aarch64"),
  "celestial-python-macos-arm64": ("macos", "arm64"),
  "celestial-python-windows-amd64": ("windows", "amd64"),
}


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
  """Find the dispatched run of `workflow` that built `sha` and succeeded.

  Fail loud when there is none -- never settle for "the latest run", which can belong
  to an unrelated, even failing, WIP branch: a release ships the artifacts of the
  commit it tags, or it does not ship.
  """
  runs, pages = GitHub.list_workflow_runs(workflow.id)
  for run in runs:
    if run.event == "workflow_dispatch" and run.head_sha == sha and run.conclusion == "success":
      return run
  red_print(f'No successful dispatched "{workflow.name}" run found for commit {sha} '
            f"(scanned {pages} page(s), {len(runs)} runs)")
  raise RuntimeError(f'No successful dispatched "{workflow.name}" run for commit {sha}')


def validate_artifact_inventory(
  workflow_name: str,
  run_id: int,
  artifact_urls: list[tuple[str, str]],
  expected_names: frozenset[str],
  seen_names: set[str],
) -> None:
  """Require one exact, duplicate-free artifact set before any download starts."""
  names = [name for name, _ in artifact_urls]
  duplicates = sorted(name for name, count in Counter(names).items() if count > 1)
  actual = set(names)
  missing = sorted(expected_names - actual)
  extra = sorted(actual - expected_names)
  collisions = sorted(actual & seen_names)
  if duplicates or missing or extra or collisions:
    raise RuntimeError(
      f'Invalid artifact inventory for "{workflow_name}" run {run_id}: '
      f"duplicates={duplicates}, missing={missing}, extra={extra}, cross-source={collisions}"
    )
  seen_names.update(actual)


def project_version() -> str:
  """Read the version from project.py without importing the build driver."""
  project = (Path(__file__).parent.parent / "project.py").read_text(encoding="utf-8")
  match = re.search(r'BUILD_VERSION: Final\[str\] = "([^"]+)"', project)
  if match is None:
    raise RuntimeError("Cannot parse BUILD_VERSION from project.py")
  return match.group(1)


def validate_wheel_platform(wheel_name: str, artifact_name: str, version: str) -> None:
  """Match one artifact name to its one permitted wheel platform tag."""
  match = re.fullmatch(rf"celestial_calendar-{re.escape(version)}-py3-none-(.+)\.whl", wheel_name)
  if match is None:
    raise RuntimeError(f"Unexpected wheel filename in {artifact_name}: {wheel_name}")
  tags = match.group(1).split(".")
  family, architecture = PYTHON_ARTIFACTS[artifact_name]
  if family == "manylinux":
    expected = f"manylinux_2_28_{architecture}"
    valid = expected in tags
    for tag in tags:
      tag_match = re.fullmatch(rf"manylinux_(\d+)_(\d+)_{architecture}", tag)
      valid = valid and tag_match is not None
      if tag_match is not None:
        valid = valid and (int(tag_match.group(1)), int(tag_match.group(2))) <= (2, 28)
  elif family == "macos":
    valid = tags == ["macosx_14_0_arm64"]
  else:
    valid = tags == ["win_amd64"]
  if not valid:
    raise RuntimeError(f"Wheel {wheel_name} does not match artifact {artifact_name}")


def flatten_python_artifacts(downloaded_artifacts: list[Path], save_to: Path) -> list[Path]:
  """Validate all Python archives, then flatten their wheels and digests atomically by file."""
  archives = {path.stem: path for path in downloaded_artifacts if path.stem in PYTHON_ARTIFACTS}
  if set(archives) != set(PYTHON_ARTIFACTS):
    missing = sorted(set(PYTHON_ARTIFACTS) - set(archives))
    raise RuntimeError(f"Missing downloaded Python artifact archives: {missing}")

  version = project_version()
  payloads: dict[str, bytes] = {}
  for artifact_name, archive_path in archives.items():
    with zipfile.ZipFile(archive_path) as archive:
      files = [name for name in archive.namelist() if not name.endswith("/")]
      if len(files) != len(set(files)):
        raise RuntimeError(f"Duplicate archive member in {artifact_name}: {files}")
      wheels = [name for name in files if name.endswith(".whl")]
      if len(wheels) != 1 or any(Path(name).name != name for name in files):
        raise RuntimeError(f"{artifact_name} must contain one root-level wheel and sidecar")
      wheel_name = wheels[0]
      sidecar_name = f"{wheel_name}.sha256"
      if set(files) != {wheel_name, sidecar_name}:
        raise RuntimeError(f"Unexpected payload in {artifact_name}: {sorted(files)}")
      validate_wheel_platform(wheel_name, artifact_name, version)

      wheel = archive.read(wheel_name)
      digest = hashlib.sha256(wheel).hexdigest()
      expected_sidecar = f"{digest}  {wheel_name}\n".encode()
      sidecar = archive.read(sidecar_name)
      if sidecar != expected_sidecar:
        raise RuntimeError(f"SHA-256 sidecar mismatch in {artifact_name}")
      for name, content in ((wheel_name, wheel), (sidecar_name, sidecar)):
        if name in payloads:
          raise RuntimeError(f"Duplicate flattened Python payload basename: {name}")
        payloads[name] = content

  destinations = [save_to / name for name in payloads]
  existing = [str(path) for path in destinations if path.exists()]
  if existing:
    raise FileExistsError(f"Refusing to overwrite flattened Python payloads: {existing}")

  flattened = []
  with tempfile.TemporaryDirectory(prefix=".celestial-python-artifacts-", dir=save_to) as temporary:
    staging = Path(temporary)
    for name, content in payloads.items():
      (staging / name).write_bytes(content)
    for name in payloads:
      destination = save_to / name
      (staging / name).replace(destination)
      flattened.append(destination)

  for archive_path in archives.values():
    archive_path.unlink()
    yellow_print(f"# Deleted {archive_path} after flattening")
  return flattened


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
    help="The ID of a single artifact run to download, skipping the release-source lookup. "
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
    # --run-id pins one specific run and skips release inventory validation entirely.
    downloaded_artifacts = GitHub.download_artifacts(args.run_id, args.save_to, args.parallel)
  else:
    sha = release_commit_sha()
    plans = []
    seen_names: set[str] = set()
    for workflow_name, expected_names in ARTIFACT_SOURCES:
      run = find_artifact_run(artifact_workflow(workflow_name), sha)
      artifact_urls = GitHub.get_artifacts_download_urls(run.id)
      validate_artifact_inventory(workflow_name, run.id, artifact_urls, expected_names, seen_names)
      plans.append(artifact_urls)

    zip_destinations = [args.save_to / f"{name}.zip" for plan in plans for name, _ in plan]
    existing = [str(path) for path in zip_destinations if path.exists()]
    if existing:
      raise FileExistsError(f"Refusing to overwrite downloaded artifacts: {existing}")

    for artifact_urls in plans:
      downloaded = GitHub.download_artifact_urls(artifact_urls, args.save_to, args.parallel)
      downloaded_artifacts.extend(downloaded)

    validate_release_archives(downloaded_artifacts, project_version())
    downloaded_artifacts.extend(flatten_python_artifacts(downloaded_artifacts, args.save_to))

  # Unzip the downloaded artifacts.
  if args.unzip:
    for artifact in downloaded_artifacts:
      if not artifact.exists() or artifact.suffix != ".zip":
        continue
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
