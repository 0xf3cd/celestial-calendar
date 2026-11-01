# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import json
from pathlib import Path
import shutil
from urllib.parse import urlsplit

import pytest
import yaml

from automation.markdown_source import link_targets, tokens


ROOT = Path(__file__).resolve().parents[2]
SKILL = ROOT / "skills/using-celestial-calendar"
CASES = {"gregorian-lunar", "utc-jieqi", "chart-snapshot", "missing-interface", "unsupported-js-events"}
TOOLS = {"Read", "Glob", "Grep", "Skill", "Write", "Edit", "Bash"}


def frontmatter(text):
  if not text.startswith("---\n") or "\n---\n" not in text[4:]:
    raise ValueError("missing frontmatter")
  header, body = text[4:].split("\n---\n", 1)
  metadata = yaml.safe_load(header)
  if not isinstance(metadata, dict) or not body.strip():
    raise ValueError("expected metadata and body")
  return metadata, body


def validate_skill(text):
  metadata, body = frontmatter(text)
  description = metadata.get("description")
  if metadata.get("name") != "using-celestial-calendar" or not isinstance(description, str) or not description.strip():
    raise ValueError("missing skill identity")
  targets = link_targets(tokens(body))
  if not targets or any(urlsplit(target).scheme != "https" or not urlsplit(target).netloc for target in targets):
    raise ValueError("references must be absolute HTTPS URLs")
  if any(token.type == "fence" and token.info in {"python", "js", "javascript", "cpp", "c"} for token in tokens(body)):
    raise ValueError("link the shared executable examples")
  return targets


def validate_case(path):
  record = yaml.safe_load((path / "case.yaml").read_text(encoding="utf-8"))
  if record.get("schema_version") != "1.1" or record.get("name") != path.name or not record.get("expected_outcome"):
    raise ValueError("invalid case identity")
  settings, _ = frontmatter((path / "prompt.md").read_text(encoding="utf-8"))
  if set(settings) != {"tags", "max_turns", "timeout_seconds", "allowed_tools"}:
    raise ValueError("unexpected prompt settings")
  if set(settings["allowed_tools"]) != TOOLS or settings["max_turns"] != 20 or settings["timeout_seconds"] != 600:
    raise ValueError("unexpected case permissions or bounds")
  graders = sorted((path / "graders").glob("*.md"))
  if not graders:
    raise ValueError("missing graders")
  for grader in graders:
    metadata, _ = frontmatter(grader.read_text(encoding="utf-8"))
    if metadata != {"type": "llm", "focus": "trace", "weight": 1}:
      raise ValueError("invalid trace grader")


def test_single_file_installation_has_public_references(tmp_path):
  installed = tmp_path / "using-celestial-calendar/SKILL.md"
  installed.parent.mkdir()
  shutil.copyfile(SKILL / "SKILL.md", installed)
  targets = validate_skill(installed.read_text(encoding="utf-8"))
  required = {"docs/API.md", "docs/ALGORITHMS.md", "docs/UTC_CONVERSIONS.md", "docs/EPHEMERIS_EVENTS.md"}
  assert required <= {
    target.removeprefix("https://github.com/0xf3cd/celestial-calendar/blob/main/") for target in targets
  }
  assert sorted(p.name for p in installed.parent.iterdir()) == ["SKILL.md"]


@pytest.mark.parametrize("fault", ["relative-link", "identity", "api-fence"])
def test_skill_portability_controls(fault):
  text = (SKILL / "SKILL.md").read_text(encoding="utf-8")
  if fault == "relative-link":
    text += "\n[Guide](../../docs/API.md)\n"
  elif fault == "identity":
    text = text.replace("name: using-celestial-calendar", "name: contributor-skill", 1)
  else:
    text += "\n```python\nprint('duplicated example')\n```\n"
  with pytest.raises(ValueError):
    validate_skill(text)


def test_public_cases_follow_the_native_case_schema():
  cases = {p.name for p in (SKILL / "evals").iterdir() if p.is_dir()}
  assert cases == CASES
  for name in sorted(cases):
    validate_case(SKILL / "evals" / name)


@pytest.mark.parametrize("fault", ["missing-grader", "unknown-setting", "numeric-schema", "extra-tool"])
def test_case_structure_controls(tmp_path, fault):
  source = SKILL / "evals/gregorian-lunar"
  case = tmp_path / source.name
  shutil.copytree(source, case)
  if fault == "missing-grader":
    for path in (case / "graders").iterdir():
      path.unlink()
  elif fault == "numeric-schema":
    path = case / "case.yaml"
    path.write_text(path.read_text().replace('schema_version: "1.1"', "schema_version: 1.1"))
  else:
    path = case / "prompt.md"
    text = path.read_text()
    text = (
      text.replace("tags:", "unknown: true\ntags:")
      if fault == "unknown-setting"
      else text.replace("Bash]", "Bash, WebFetch]")
    )
    path.write_text(text)
  with pytest.raises(ValueError):
    validate_case(case)


def test_trigger_queries_include_balanced_near_misses():
  queries = json.loads((SKILL / "evals/triggers.json").read_text(encoding="utf-8"))
  assert len(queries) == len({row["query"] for row in queries}) == 20
  assert all(set(row) == {"query", "should_trigger"} and type(row["should_trigger"]) is bool for row in queries)
  assert sum(row["should_trigger"] for row in queries) == 10
  for expected in (True, False):
    assert any(row["should_trigger"] is expected and any(ord(char) > 127 for char in row["query"]) for row in queries)
