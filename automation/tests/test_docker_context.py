# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

from fnmatch import fnmatchcase
from pathlib import Path

import pytest


RULES = (Path(__file__).resolve().parents[2] / ".dockerignore").read_text().splitlines()


@pytest.mark.parametrize(
  "relative",
  [".git", "build", ".venv", ".review", ".env", ".env.local", "credentials.json", "id_ed25519", "private.pem"],
)
def test_context_exclusion_patterns_cover_local_inputs(relative):
  assert any(fnmatchcase("fixture/" + relative, rule) for rule in RULES if rule and not rule.startswith("#"))


@pytest.mark.parametrize(
  "relative",
  ["src/tokenizer.hpp", "src/key_value.hpp", "automation/tests/test_cpp_token_digest.py", "Requirements.txt"],
)
def test_context_patterns_keep_legitimate_source_names(relative):
  assert not any(fnmatchcase("fixture/" + relative, rule) for rule in RULES if rule and not rule.startswith("#"))
