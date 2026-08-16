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


def test_npm_publish_stays_a_dry_run():
  workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
  publish_steps = [step for step in workflow["jobs"]["wasm"]["steps"] if "npm publish" in step.get("run", "")]

  assert len(publish_steps) == 1
  publish_step = publish_steps[0]
  assert publish_step.get("env", {}).get("NPM_CONFIG_DRY_RUN") == "true"
  commands = [line.strip() for line in publish_step["run"].splitlines() if line.strip()]
  assert commands == ['npm publish --dry-run "${{ steps.npm-package.outputs.tarball }}"']
