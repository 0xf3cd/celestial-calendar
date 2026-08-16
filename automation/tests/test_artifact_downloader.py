# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

import automation.github as github_module

from automation.github import GitHub
from toolbox.artifact_downloader import find_artifact_run


class Response:
  def raise_for_status(self):
    pass

  def json(self):
    return {
      "workflow_runs": [{
        "id": 7,
        "name": "WASM Build and Golden Check",
        "run_number": 11,
        "status": "completed",
        "conclusion": "success",
        "event": "workflow_dispatch",
        "head_sha": "tagged-sha",
        "workflow_id": 13,
        "url": "https://example.invalid/run/7",
        "created_at": "2026-08-15T00:00:00Z",
        "updated_at": "2026-08-15T00:01:00Z",
      }],
    }


def run(run_id: int, event: str) -> GitHub.Run:
  return GitHub.Run(
    run_id,
    "WASM Build and Golden Check",
    run_id,
    "completed",
    "success",
    event,
    "tagged-sha",
    13,
    f"https://example.invalid/run/{run_id}",
    "2026-08-15T00:00:00Z",
    "2026-08-15T00:01:00Z",
  )


def test_workflow_run_event_is_parsed(monkeypatch):
  monkeypatch.setattr(github_module, "gen_headers", lambda: {})
  monkeypatch.setattr(github_module.requests, "get", lambda *_args, **_kwargs: Response())

  runs, pages = GitHub.list_workflow_runs(13)

  assert pages == 1
  assert runs[0].event == "workflow_dispatch"


def test_artifact_lookup_rejects_pull_request_run(monkeypatch):
  pull_request = run(8, "pull_request")
  dispatched = run(7, "workflow_dispatch")
  monkeypatch.setattr(GitHub, "list_workflow_runs", lambda _workflow_id: ([pull_request, dispatched], 1))
  workflow = GitHub.Workflow(13, "WASM Build and Golden Check", "active", "", "", "")

  assert find_artifact_run(workflow, "tagged-sha") is dispatched
