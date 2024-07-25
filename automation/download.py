# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
# 
# Author: Ningqi Wang (0xf3cd)
# Email : nq.maigre@gmail.com
# Repo  : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
# 
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import os
import asyncio
import requests

from typing import Dict
from pathlib import Path

from .utils import green_print, red_print, blue_print


def gen_headers() -> Dict[str, str]:
  """
  Generate headers for GitHub API requests.

  Returns:
    Dict[str, str]: A dictionary containing the authorization header.
  """
  github_token = os.environ.get('GITHUB_TOKEN')
  if not github_token:
    raise ValueError('GITHUB_TOKEN is not set in the environment. '
                     'It is required for downloading artifacts. '
                     'Please set your GitHub personal access token and try again.')
  return {'Authorization': f'token {github_token}'}


def get_download_urls(run_id: str) -> Dict[str, str]:
  """
  Fetch the download URLs for all artifacts of a specific run.

  Args:
    run_id (str): The ID of the GitHub Actions run.

  Returns:
    Dict[str, str]: A dictionary mapping artifact names to their download URLs.
  """
  artifacts_url = f'https://api.github.com/repos/0xf3cd/celestial-calendar/actions/runs/{run_id}/artifacts'
  
  response = requests.get(artifacts_url, headers=gen_headers())
  response.raise_for_status()
  artifacts = response.json()

  return {
    artifact['name']: artifact['archive_download_url']
    for artifact in artifacts['artifacts']
  }


def download_one_artifact(name: str, download_url: str, download_dir: Path) -> None:
  """
  Download a single artifact and save it to the specified directory.

  Args:
    name (str): The name of the artifact.
    download_url (str): The download URL of the artifact.
    download_dir (Path): The directory where the artifact should be saved.

  Returns:
    None
  """
  with requests.get(download_url, headers=gen_headers(), stream=True) as response:
    response.raise_for_status()

    download_dir.mkdir(parents=True, exist_ok=True)
    zip_dir = download_dir / f"{name}.zip"  # Ensure the file has a .zip extension

    with zip_dir.open('wb') as f:
      for chunk in response.iter_content(chunk_size=8192):
        f.write(chunk)

    green_print(f'# Downloaded {name}')


async def async_download_artifacts(run_id: str, download_dir: Path, parallel: int = 4) -> None:
  """
  Asynchronously fetch all artifacts for a given run.

  Args:
    run_id (str): The ID of the GitHub Actions run.
    download_dir (Path): The directory where artifacts should be saved.
    parallel (int, optional): The number of parallel download tasks. Default is 4.

  Returns:
    None
  """
  if parallel < 1 or parallel > 20:
    raise ValueError('Invalid parallel value')

  artifact_urls = get_download_urls(run_id)
  blue_print(f'# Downloading {len(artifact_urls)} artifacts...')

  queue = asyncio.Queue()
  for name, url in artifact_urls.items():
    queue.put_nowait((name, url))

  async def coro():
    while True:
      try:
        name, url = await queue.get()
        await asyncio.to_thread(download_one_artifact, name, url, download_dir)
      except Exception as e:
        red_print(f'# Failed to download {name}: {e}')
      finally:
        queue.task_done()  # Signal that the current task is done.

  tasks = [asyncio.create_task(coro()) for _ in range(parallel)]

  # Block until all items in the queue have been processed. 
  # This may be redundant though, since we `asyncio.gather` later.
  await queue.join() 
  # Ensure all tasks are awaited.
  await asyncio.gather(*tasks, return_exceptions=True)


def download_artifacts(run_id: str, download_dir: Path, parallel: int = 4) -> None:
  """
  Fetching artifacts in parallel.

  Args:
    run_id (str): The ID of the GitHub Actions run.
    download_dir (Path): The directory where artifacts should be saved.
    parallel (int, optional): The number of parallel download tasks. Default is 4.

  Returns:
    None
  """
  asyncio.run(async_download_artifacts(run_id, download_dir, parallel))
