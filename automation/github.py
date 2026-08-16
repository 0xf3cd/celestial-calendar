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

import os
import asyncio
import requests

from typing import Dict, List, Tuple
from pathlib import Path
from dataclasses import dataclass

from .utils import green_print, red_print, blue_print


OWNER = "0xf3cd" # Yes, it's me.
REPO  = "celestial-calendar"

# `requests` has no default timeout, so a server that accepts the connection and then goes quiet
# hangs the release tooling with no ceiling. The pair is (connect, read); the read half applies
# between bytes received, not to a whole transfer, so it stays short without capping big artifacts.
TIMEOUT = (10, 60)


def gen_headers() -> Dict[str, str]:
  """
  Generate headers for GitHub API requests.

  Returns:
    Dict[str, str]: A dictionary containing the authorization header.
  """
  github_token = os.environ.get("GITHUB_TOKEN")
  if not github_token:
    raise ValueError("GITHUB_TOKEN is not set in the environment. "
                     "It is required for downloading artifacts. "
                     "Please set your GitHub personal access token and try again.")
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
    id:         int
    name:       str
    state:      str
    created_at: str
    updated_at: str
    url:        str
  
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
      GitHub.Workflow(
        wf["id"],
        wf["name"],
        wf["state"],
        wf["created_at"],
        wf["updated_at"],
        wf["url"]
      ) 
      for wf in json_resp["workflows"]
    ]
  

  @dataclass
  class Run:
    """
    A class for representing a run in the repository.
    """
    id:          int
    name:        str
    run_number:  int
    status:      str
    conclusion:  str
    event:       str
    head_sha:    str
    workflow_id: int
    url:         str
    created_at:  str
    updated_at:  str

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
      url = (f"https://api.github.com/repos/{OWNER}/{REPO}"
             f"/actions/workflows/{workflow_id}/runs?per_page=100&page={page}")
      response = requests.get(url, headers=gen_headers(), timeout=TIMEOUT)
      response.raise_for_status()

      batch = response.json()["workflow_runs"]
      runs.extend(
        GitHub.Run(
          run["id"],
          run["name"],
          run["run_number"],
          run["status"],
          run["conclusion"] or "",  # null while the run is still in progress
          run["event"],
          run["head_sha"],
          run["workflow_id"],
          run["url"],
          run["created_at"],
          run["updated_at"]
        )
        for run in batch
      )
      if len(batch) < 100:
        return runs, page

    return runs, max_pages


  @staticmethod
  def get_artifacts_download_urls(run_id: int) -> List[Tuple[str, str]]:
    """
    Fetch the download URLs for all artifacts of a specific run.

    Args:
      run_id (int): The ID of the GitHub Actions run.

    Returns:
      List[Tuple[str, str]]: Artifact names and download URLs in API order. A list is
                            deliberate: duplicate names must remain visible to callers.
    """
    artifacts_url = f"https://api.github.com/repos/{OWNER}/{REPO}/actions/runs/{run_id}/artifacts?per_page=100"
    
    response = requests.get(artifacts_url, headers=gen_headers(), timeout=TIMEOUT)
    response.raise_for_status()
    artifacts = response.json()

    entries = [
      (artifact["name"], artifact["archive_download_url"])
      for artifact in artifacts["artifacts"]
    ]
    if artifacts["total_count"] != len(entries):
      raise RuntimeError(
        f"Artifact response for run {run_id} returned {len(entries)} of "
        f"{artifacts['total_count']} entries"
      )
    return entries

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
    download_dir.mkdir(parents=True, exist_ok=True)
    artifact = download_dir / f"{name}.zip"
    partial = artifact.with_suffix(".zip.part")
    if artifact.exists():
      raise FileExistsError(f"Refusing to overwrite artifact: {artifact}")
    if partial.exists():
      raise FileExistsError(f"Refusing to overwrite partial artifact: {partial}")

    try:
      with requests.get(download_url, headers=gen_headers(), stream=True, timeout=TIMEOUT) as response:
        response.raise_for_status()

        with partial.open("wb") as f:
          for chunk in response.iter_content(chunk_size=8192):
            f.write(chunk)
      partial.replace(artifact)
    except Exception:
      partial.unlink(missing_ok=True)
      raise

    green_print(f"# Downloaded {name}")
    return artifact


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
    if parallel < 1 or parallel > 20:
      raise ValueError("Invalid parallel value")

    names = [name for name, _ in artifact_urls]
    if len(names) != len(set(names)):
      raise RuntimeError(f"Duplicate artifact names are not safe to download: {names}")
    blue_print(f"# Downloading {len(artifact_urls)} artifacts...")

    queue = asyncio.Queue()
    for name, url in artifact_urls:
      queue.put_nowait((name, url))

    paths: List[Path] = []
    failures: List[str] = []

    async def coro():
      while not queue.empty():
        name, url = await queue.get()
        try:
          path = await asyncio.to_thread(GitHub.download_one_artifact, name, url, download_dir)
          paths.append(path)
        except Exception as e:
          # Record, keep draining (or queue.join() hangs), then fail the whole call below:
          # a release must never ship with artifacts silently missing.
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
      raise RuntimeError(f"{len(failures)} artifact(s) failed to download: {failures}")

    return paths

  @staticmethod
  async def async_download_artifacts(run_id: int, download_dir: Path, parallel: int) -> List[Path]:
    """Fetch and download all artifacts for one run."""
    artifact_urls = GitHub.get_artifacts_download_urls(run_id)
    return await GitHub.async_download_artifact_urls(artifact_urls, download_dir, parallel)

  @staticmethod
  def download_artifact_urls(
    artifact_urls: List[Tuple[str, str]], download_dir: Path, parallel: int = 4
  ) -> List[Path]:
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


  @dataclass
  class Asset:
    """
    A class representing a GitHub asset.
    """
    id: int
    name: str
    content_type: str
    size: int
    download_count: int
    url: str
    browser_download_url: str

  @dataclass
  class Release:
    """
    A class representing a GitHub release.
    """
    id: int
    tag_name: str
    draft: bool
    prerelease: bool
    created_at: str
    published_at: str
    name: str
    url: str
    html_url: str
    tarball_url: str
    zipball_url: str
    assets_url: str
    assets: List["GitHub.Asset"]

  @staticmethod
  def list_releases() -> List["GitHub.Release"]:
    """
    List all releases in the repository.

    Returns:
      List['GitHub.Release']: A list of GitHub releases.
    """
    url = f"https://api.github.com/repos/{OWNER}/{REPO}/releases"
    response = requests.get(url, headers=gen_headers(), timeout=TIMEOUT)
    response.raise_for_status()

    json_resp = response.json()
    return [
      GitHub.Release(
        release["id"],
        release["tag_name"],
        release["draft"],
        release["prerelease"],
        release["created_at"],
        release["published_at"],
        release["name"],
        release["url"],
        release["html_url"],
        release["tarball_url"],
        release["zipball_url"],
        release["assets_url"],
        [
          GitHub.Asset(
            asset["id"],
            asset["name"],
            asset["content_type"],
            asset["size"],
            asset["download_count"],
            asset["url"],
            asset["browser_download_url"]
          )
          for asset in release["assets"]
        ]
      )
      for release in json_resp
    ]
  
  @staticmethod
  def download_one_release_asset(name: str, download_url: str, download_dir: Path) -> Path:
    """
    Download a single release asset.

    Args:
      name (str): The name of the asset.
      download_url (str): The URL of the asset.
      download_dir (Path): The directory where the asset should be saved.

    Returns:
      Path: The path to the downloaded asset.
    """
    with requests.get(download_url, headers=gen_headers(), stream=True, timeout=TIMEOUT) as response:
      response.raise_for_status()

      download_dir.mkdir(parents=True, exist_ok=True)
      asset = download_dir / name

      with open(asset, "wb") as f:
        for chunk in response.iter_content(chunk_size=8192):
          f.write(chunk)

      green_print(f"# Downloaded {name}")
      return asset

  @staticmethod
  async def async_download_release(release_id: int, download_dir: Path, parallel: int) -> List[Path]:
    """
    Asynchronously fetch all assets and source code for a given release.

    Args:
      release_id (int): The ID of the GitHub release.
      download_dir (Path): The directory where assets should be saved.
      parallel (int): The number of parallel download tasks.

    Returns:
      List[Path]: The path to the downloaded assets.
    """
    if parallel < 1 or parallel > 20:
      raise ValueError("Invalid parallel value")
    
    releases = GitHub.list_releases()
    selected_release = None
    for release in releases:
      if release.id == release_id:
        selected_release = release
        break

    if selected_release is None:
      raise ValueError(f"Invalid release id: {release_id}")

    assets = selected_release.assets

    queue = asyncio.Queue()
    for asset in assets:
      queue.put_nowait((asset.name, asset.browser_download_url))

    tag_name = selected_release.tag_name
    archive_url = f"https://github.com/{OWNER}/{REPO}/archive/refs/tags/{tag_name}"
    queue.put_nowait(("src.zip", f"{archive_url}.zip"))
    queue.put_nowait(("src.tar.gz", f"{archive_url}.tar.gz"))

    paths: List[Path] = []

    async def coro():
      while not queue.empty():
        name, url = await queue.get()
        try:
          path = await asyncio.to_thread(GitHub.download_one_release_asset, name, url, download_dir)
          paths.append(path)
        except Exception as e:
          red_print(f"# Failed to download {name}: {e}")
        finally:
          queue.task_done() # Signal that the current task is done.

    tasks = [asyncio.create_task(coro()) for _ in range(parallel)]

    # Block until all items in the queue have been processed. 
    # This may be redundant though, since we will `asyncio.gather` later.
    await queue.join() 
    # Ensure all tasks are awaited.
    await asyncio.gather(*tasks)

    return paths

  @staticmethod
  def download_release(release_id: int, download_dir: Path, parallel: int = 4) -> List[Path]:
    """
    Download all assets and source code for a given release.

    Args:
      release_id (int): The ID of the GitHub release.
      download_dir (Path): The directory where assets should be saved.
      parallel (int): The number of parallel download tasks.

    Returns:
      List[Path]: The path to the downloaded assets.
    """
    return asyncio.run(GitHub.async_download_release(release_id, download_dir, parallel))
