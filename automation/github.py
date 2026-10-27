# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import os
import asyncio
import requests

from typing import Dict, List, Tuple
from pathlib import Path
from dataclasses import dataclass

from .utils import green_print, red_print, blue_print


OWNER = "0xf3cd"  # Yes, it's me.
REPO = "celestial-calendar"

# `requests` has no default timeout, so a server that accepts the connection and then goes quiet
# hangs the release tooling with no ceiling. The pair is (connect, read); the read half applies
# between bytes received, not to a whole transfer, so it stays short without capping big artifacts.
TIMEOUT = (10, 60)


def _download_file(download_url: str, destination: Path) -> None:
  """Download one URL without exposing a partial file as the destination."""
  destination.parent.mkdir(parents=True, exist_ok=True)
  partial = destination.with_name(f"{destination.name}.part")
  if destination.exists():
    raise FileExistsError(f"Refusing to overwrite download: {destination}")
  if partial.exists():
    raise FileExistsError(f"Refusing to overwrite partial download: {partial}")

  try:
    with requests.get(download_url, headers=gen_headers(), stream=True, timeout=TIMEOUT) as response:
      response.raise_for_status()
      with partial.open("wb") as f:
        for chunk in response.iter_content(chunk_size=8192):
          f.write(chunk)
    partial.replace(destination)
  except Exception:
    partial.unlink(missing_ok=True)
    raise


def gen_headers() -> Dict[str, str]:
  """
  Generate headers for GitHub API requests.

  Returns:
    Dict[str, str]: A dictionary containing the authorization header.
  """
  github_token = os.environ.get("GITHUB_TOKEN")
  if not github_token:
    raise ValueError(
      "GITHUB_TOKEN is not set in the environment. "
      "It is required for downloading artifacts. "
      "Please set your GitHub personal access token and try again."
    )
  return {"Authorization": f"Bearer {github_token}"}


class GitHub:
  """
  A class for interacting with the GitHub API.
  """

  @dataclass
  class Workflow:
    """
    A class for representing a workflow in the repository.
    """

    id: int
    name: str
    state: str
    created_at: str
    updated_at: str
    url: str

  @staticmethod
  def list_workflows() -> List[Workflow]:
    """
    List all workflows in the repository.

    Returns:
      Dict[str, str]: A dictionary mapping workflow names to their IDs.
    """
    url = f"https://api.github.com/repos/{OWNER}/{REPO}/actions/workflows"
    response = requests.get(url, headers=gen_headers(), timeout=TIMEOUT)
    response.raise_for_status()

    json_resp = response.json()
    return [
      GitHub.Workflow(wf["id"], wf["name"], wf["state"], wf["created_at"], wf["updated_at"], wf["url"])
      for wf in json_resp["workflows"]
    ]

  @dataclass
  class Run:
    """
    A class for representing a run in the repository.
    """

    id: int
    name: str
    run_number: int
    status: str
    conclusion: str
    event: str
    head_sha: str
    workflow_id: int
    url: str
    created_at: str
    updated_at: str

  @staticmethod
  def _run(data) -> Run:
    return GitHub.Run(
      data["id"],
      data["name"],
      data["run_number"],
      data["status"],
      data["conclusion"] or "",
      data["event"],
      data["head_sha"],
      data["workflow_id"],
      data["url"],
      data["created_at"],
      data["updated_at"],
    )

  @staticmethod
  def get_workflow_run(run_id: int) -> Run:
    """Get one workflow run by its immutable ID."""
    url = f"https://api.github.com/repos/{OWNER}/{REPO}/actions/runs/{run_id}"
    response = requests.get(url, headers=gen_headers(), timeout=TIMEOUT)
    response.raise_for_status()
    return GitHub._run(response.json())

  # Pagination cap for list_workflow_runs: 10 pages x 100 runs reaches far further back
  # than a release ever needs to look; a caller that still finds nothing must say how
  # many pages it scanned.
  MAX_RUN_PAGES = 10

  @staticmethod
  def list_workflow_runs(workflow_id: int, max_pages: int = MAX_RUN_PAGES) -> Tuple[List[Run], int]:
    """List the runs of ONE workflow, paginated (100 per page, newest first).

    Returns the runs and the number of pages scanned. The repo-wide
    `.../actions/runs` endpoint is deliberately NOT the path here: it pages the whole
    repo's history, and the workflow we care about can be a handful of entries inside
    the first page -- client-side filtering of a global list silently narrows the
    search window, so a "not found" answer from it means nothing.
    """
    runs: List[GitHub.Run] = []
    for page in range(1, max_pages + 1):
      url = f"https://api.github.com/repos/{OWNER}/{REPO}/actions/workflows/{workflow_id}/runs?per_page=100&page={page}"
      response = requests.get(url, headers=gen_headers(), timeout=TIMEOUT)
      response.raise_for_status()

      batch = response.json()["workflow_runs"]
      runs.extend(GitHub._run(run) for run in batch)
      if len(batch) < 100:
        return runs, page

    return runs, max_pages

  @dataclass
  class WorkflowArtifact:
    """One immutable artifact uploaded by a workflow run."""

    id: int
    name: str
    size: int
    digest: str
    archive_download_url: str

  @staticmethod
  def get_workflow_artifacts(run_id: int) -> List[WorkflowArtifact]:
    """
    Fetch the download URLs for all artifacts of a specific run.

    Args:
      run_id (int): The ID of the GitHub Actions run.

    Returns artifacts in API order. A list is deliberate: duplicate names must remain
    visible to callers.
    """
    artifacts_url = f"https://api.github.com/repos/{OWNER}/{REPO}/actions/runs/{run_id}/artifacts?per_page=100"

    response = requests.get(artifacts_url, headers=gen_headers(), timeout=TIMEOUT)
    response.raise_for_status()
    artifacts = response.json()

    entries = [
      GitHub.WorkflowArtifact(
        artifact["id"],
        artifact["name"],
        artifact["size_in_bytes"],
        artifact.get("digest") or "",
        artifact["archive_download_url"],
      )
      for artifact in artifacts["artifacts"]
    ]
    if artifacts["total_count"] != len(entries):
      raise RuntimeError(
        f"Artifact response for run {run_id} returned {len(entries)} of {artifacts['total_count']} entries"
      )
    return entries

  @staticmethod
  def get_artifacts_download_urls(run_id: int) -> List[Tuple[str, str]]:
    """Fetch artifact names and download URLs while preserving API order."""
    return [(artifact.name, artifact.archive_download_url) for artifact in GitHub.get_workflow_artifacts(run_id)]

  @staticmethod
  def download_one_artifact(name: str, download_url: str, download_dir: Path) -> Path:
    """
    Download a single artifact and save it to the specified directory.

    Args:
      name (str): The name of the artifact.
      download_url (str): The download URL of the artifact.
      download_dir (Path): The directory where the artifact should be saved.

    Returns:
      Path: The path to the downloaded artifact.
    """
    artifact = download_dir / f"{name}.zip"
    _download_file(download_url, artifact)

    green_print(f"# Downloaded {name}")
    return artifact

  @staticmethod
  def _validate_parallel(parallel: int) -> None:
    if parallel < 1 or parallel > 20:
      raise ValueError("Invalid parallel value")

  @staticmethod
  async def _async_download_urls(
    download_urls: List[Tuple[str, str]], download_dir: Path, parallel: int, download_one, noun: str
  ) -> List[Path]:
    queue = asyncio.Queue()
    for name, url in download_urls:
      queue.put_nowait((name, url))

    paths: List[Path] = []
    failures: List[str] = []

    async def coro():
      while not queue.empty():
        name, url = await queue.get()
        try:
          path = await asyncio.to_thread(download_one, name, url, download_dir)
          paths.append(path)
        except Exception as e:
          # Keep draining before raising; otherwise queue.join() waits forever.
          red_print(f"# Failed to download {name}: {e}")
          failures.append(f"{name}: {e}")
        finally:
          queue.task_done()  # Signal that the current task is done.

    tasks = [asyncio.create_task(coro()) for _ in range(parallel)]

    # Block until all items in the queue have been processed.
    # This may be redundant though, since we will `asyncio.gather` later.
    await queue.join()
    # Ensure all tasks are awaited.
    await asyncio.gather(*tasks)

    if failures:
      raise RuntimeError(f"{len(failures)} {noun}(s) failed to download: {failures}")

    return paths

  @staticmethod
  async def async_download_artifact_urls(
    artifact_urls: List[Tuple[str, str]], download_dir: Path, parallel: int
  ) -> List[Path]:
    """
    Asynchronously download the given artifact names and URLs.

    Args:
      artifact_urls (List[Tuple[str, str]]): Artifact names and URLs to download.
      download_dir (Path): The directory where artifacts should be saved.
      parallel (int): The number of parallel download tasks.

    Returns:
      List[Path]: The path to the downloaded artifacts.
    """
    GitHub._validate_parallel(parallel)
    names = [name for name, _ in artifact_urls]
    if len(names) != len(set(names)):
      raise RuntimeError(f"Duplicate artifact names are not safe to download: {names}")
    blue_print(f"# Downloading {len(artifact_urls)} artifacts...")
    return await GitHub._async_download_urls(
      artifact_urls,
      download_dir,
      parallel,
      GitHub.download_one_artifact,
      "artifact",
    )

  @staticmethod
  async def async_download_artifacts(run_id: int, download_dir: Path, parallel: int) -> List[Path]:
    """Fetch and download all artifacts for one run."""
    artifact_urls = GitHub.get_artifacts_download_urls(run_id)
    return await GitHub.async_download_artifact_urls(artifact_urls, download_dir, parallel)

  @staticmethod
  def download_artifact_urls(artifact_urls: List[Tuple[str, str]], download_dir: Path, parallel: int = 4) -> List[Path]:
    """Download a pre-validated artifact inventory in parallel."""
    return asyncio.run(GitHub.async_download_artifact_urls(artifact_urls, download_dir, parallel))

  @staticmethod
  def download_artifacts(run_id: int, download_dir: Path, parallel: int = 4) -> List[Path]:
    """
    Fetching artifacts in parallel.

    Args:
      run_id (int): The ID of the GitHub Actions run.
      download_dir (Path): The directory where artifacts should be saved.
      parallel (int, optional): The number of parallel download tasks. Default is 4.

    Returns:
      List[Path]: The path to the downloaded artifacts.
    """
    return asyncio.run(GitHub.async_download_artifacts(run_id, download_dir, parallel))
