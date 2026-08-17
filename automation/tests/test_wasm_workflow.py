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

from pathlib import Path

import yaml


WORKFLOW = Path(__file__).parents[2] / ".github" / "workflows" / "wasm.yml"


def test_emsdk_source_is_pinned_even_on_cache_hits():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  env = workflow["env"]
  steps = workflow["jobs"]["wasm"]["steps"]
  cache_step = next(step for step in steps if step.get("name") == "Cache emsdk")
  install_step = next(step for step in steps if step.get("name") == "Install emsdk (cache miss only)")
  identity_step = next(step for step in steps if step.get("name") == "Verify emsdk source identity")

  assert env["EMSDK_VERSION"] == "6.0.6"
  assert env["EMSDK_COMMIT"] == "9981799f744be74ac67b1c1813ff172f63be0630"
  assert cache_step["with"]["key"] == "emsdk-${{ env.EMSDK_VERSION }}-${{ env.EMSDK_COMMIT }}-${{ runner.os }}"
  assert install_step["if"] == "steps.emsdk-cache.outputs.cache-hit != 'true'"
  assert 'git -C emsdk fetch --depth 1 origin "$EMSDK_COMMIT"' in install_step["run"]
  assert "git -C emsdk checkout --detach FETCH_HEAD" in install_step["run"]
  assert "if" not in identity_step
  assert "git -C emsdk rev-parse HEAD" in identity_step["run"]
  assert "git -C emsdk symbolic-ref --quiet HEAD" in identity_step["run"]


def test_npm_publish_stays_a_dry_run():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  job = workflow["jobs"]["wasm"]
  steps = job["steps"]
  publish_steps = [step for step in steps if "npm publish" in step.get("run", "")]

  assert len(publish_steps) == 1
  publish_step = publish_steps[0]
  assert "NPM_CONFIG_DRY_RUN" not in workflow.get("env", {})
  assert "NPM_CONFIG_DRY_RUN" not in job.get("env", {})
  dry_run_steps = [step for step in steps if "NPM_CONFIG_DRY_RUN" in step.get("env", {})]
  assert dry_run_steps == [publish_step]
  assert publish_step.get("env", {}).get("NPM_CONFIG_DRY_RUN") == "true"
  commands = [line.strip() for line in publish_step["run"].splitlines() if line.strip()]
  assert commands == ['npm publish --dry-run "${{ steps.npm-package.outputs.tarball }}"']
