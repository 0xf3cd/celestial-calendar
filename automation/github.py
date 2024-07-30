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

from typing import Dict, List
from pathlib import Path
from dataclasses import dataclass

from .utils import green_print, red_print, blue_print


OWNER = '0xf3cd' # Yes, it's me.
REPO  = 'celestial-calendar'


def gen_headers() -> Dict[str, str]:
  '''
  Generate headers for GitHub API requests.

  Returns:
    Dict[str, str]: A dictionary containing the authorization header.
  '''
  github_token = os.environ.get('GITHUB_TOKEN')
  if not github_token:
    raise ValueError('GITHUB_TOKEN is not set in the environment. '
                     'It is required for downloading artifacts. '
                     'Please set your GitHub personal access token and try again.')
  return {'Authorization': f'Bearer {github_token}'}


class GitHub:
  '''
  A class for interacting with the GitHub API.
  '''

  @dataclass
  class Workflow:
    '''
    A class for representing a workflow in the repository.
    '''
    id:         str
    name:       str
    state:      str
    created_at: str
    updated_at: str
    url:        str
  
  @staticmethod
  def list_workflows() -> List[Workflow]:
    '''
    List all workflows in the repository.

    Returns:
      Dict[str, str]: A dictionary mapping workflow names to their IDs.
    '''
    url = f'https://api.github.com/repos/{OWNER}/{REPO}/actions/workflows'
    response = requests.get(url, headers=gen_headers())
    response.raise_for_status()

    json_resp = response.json()
    return [
      GitHub.Workflow(
        wf['id'],
        wf['name'],
        wf['state'],
        wf['created_at'],
        wf['updated_at'],
        wf['url']
      ) 
      for wf in json_resp['workflows']
    ]
  

  @dataclass
  class Run:
    '''
    A class for representing a run in the repository.
    '''
    id:          int
    name:        str
    run_number:  int
    status:      str
    workflow_id: int
    url:         str
    created_at:  str
    updated_at:  str

  @staticmethod
  def list_runs() -> List[Run]:
    '''
    List all runs in the repository.

    Returns:
      List[str]: A list of run IDs.
    '''
    url = f'https://api.github.com/repos/{OWNER}/{REPO}/actions/runs'
    response = requests.get(url, headers=gen_headers())
    response.raise_for_status()

    json_resp = response.json()
    return [
      GitHub.Run(
        run['id'], 
        run['name'],
        run['run_number'],
        run['status'],
        run['workflow_id'],
        run['url'],
        run['created_at'],
        run['updated_at']
      )
      for run in json_resp['workflow_runs']
    ]


  @staticmethod
  def get_artifacts_download_urls(run_id: int) -> Dict[str, str]:
    '''
    Fetch the download URLs for all artifacts of a specific run.

    Args:
      run_id (str): The ID of the GitHub Actions run.

    Returns:
      Dict[str, str]: A dictionary mapping artifact names to their download URLs.
    '''
    artifacts_url = f'https://api.github.com/repos/{OWNER}/{REPO}/actions/runs/{run_id}/artifacts'
    
    response = requests.get(artifacts_url, headers=gen_headers())
    response.raise_for_status()
    artifacts = response.json()

    return {
      artifact['name']: artifact['archive_download_url']
      for artifact in artifacts['artifacts']
    }

  @staticmethod
  def download_one_artifact(name: str, download_url: str, download_dir: Path) -> Path:
    '''
    Download a single artifact and save it to the specified directory.

    Args:
      name (str): The name of the artifact.
      download_url (str): The download URL of the artifact.
      download_dir (Path): The directory where the artifact should be saved.

    Returns:
      Path: The path to the downloaded artifact.
    '''
    with requests.get(download_url, headers=gen_headers(), stream=True) as response:
      response.raise_for_status()

      download_dir.mkdir(parents=True, exist_ok=True)
      artifact = download_dir / f"{name}.zip"  # Ensure the file has a .zip extension

      with artifact.open('wb') as f:
        for chunk in response.iter_content(chunk_size=8192):
          f.write(chunk)

      green_print(f'# Downloaded {name}')
      return artifact


  @staticmethod
  async def async_download_artifacts(run_id: int, download_dir: Path, parallel: int) -> List[Path]:
    '''
    Asynchronously fetch all artifacts for a given run.

    Args:
      run_id (str): The ID of the GitHub Actions run.
      download_dir (Path): The directory where artifacts should be saved.
      parallel (int): The number of parallel download tasks.

    Returns:
      List[Path]: The path to the downloaded artifacts.
    '''
    if parallel < 1 or parallel > 20:
      raise ValueError('Invalid parallel value')

    artifact_urls = GitHub.get_artifacts_download_urls(run_id)
    blue_print(f'# Downloading {len(artifact_urls)} artifacts...')

    queue = asyncio.Queue()
    for name, url in artifact_urls.items():
      queue.put_nowait((name, url))

    paths: List[Path] = []

    async def coro():
      while not queue.empty():
        name, url = await queue.get()
        try:
          path = await asyncio.to_thread(GitHub.download_one_artifact, name, url, download_dir)
          paths.append(path)
        except Exception as e:
          red_print(f'# Failed to download {name}: {e}')
        finally:
          queue.task_done()  # Signal that the current task is done.

    tasks = [asyncio.create_task(coro()) for _ in range(parallel)]

    # Block until all items in the queue have been processed. 
    # This may be redundant though, since we `asyncio.gather` later.
    await queue.join() 
    # Ensure all tasks are awaited.
    await asyncio.gather(*tasks)

    return paths


  @staticmethod
  def download_artifacts(run_id: int, download_dir: Path, parallel: int = 4) -> List[Path]:
    '''
    Fetching artifacts in parallel.

    Args:
      run_id (str): The ID of the GitHub Actions run.
      download_dir (Path): The directory where artifacts should be saved.
      parallel (int, optional): The number of parallel download tasks. Default is 4.

    Returns:
      List[Path]: The path to the downloaded artifacts.
    '''
    return asyncio.run(GitHub.async_download_artifacts(run_id, download_dir, parallel))
