# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import re

from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[2]

# Each English guide and its Chinese translation show the same examples, character for character.
GUIDE_PAIRS = (
  ("README.md", "README_ZH.md"),
  ("bindings/python/README.md", "bindings/python/README_ZH.md"),
  ("bindings/javascript/README.md", "bindings/javascript/README_ZH.md"),
)
FENCE = re.compile(r"^[ \t]*```.*?^[ \t]*```[ \t]*$", re.MULTILINE | re.DOTALL)


def code_blocks(text: str) -> list[str]:
  blocks = FENCE.findall(text)
  fence_lines = sum(1 for line in text.splitlines() if line.lstrip().startswith("```"))
  assert fence_lines == 2 * len(blocks), "unbalanced code fence"
  return blocks


@pytest.mark.parametrize(("english", "chinese"), GUIDE_PAIRS)
def test_translated_guides_share_code_blocks(english, chinese):
  english_blocks = code_blocks((REPO / english).read_text(encoding="utf-8"))
  chinese_blocks = code_blocks((REPO / chinese).read_text(encoding="utf-8"))

  assert english_blocks
  assert chinese_blocks == english_blocks


def test_unbalanced_fence_is_rejected():
  with pytest.raises(AssertionError, match="unbalanced code fence"):
    code_blocks("```sh\nls\n```\n```python\n")
