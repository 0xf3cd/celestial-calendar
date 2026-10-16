# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import os
import re
import shlex
import shutil
import subprocess

from pathlib import Path

import pytest

from toolbox.artifact_downloader import project_version

REPO = Path(__file__).resolve().parents[2]

# Each English guide and its Chinese translation show the same examples, character for character.
GUIDE_PAIRS = (
  ("README.md", "README_ZH.md"),
  ("bindings/python/README.md", "bindings/python/README_ZH.md"),
  ("bindings/javascript/README.md", "bindings/javascript/README_ZH.md"),
)
# The Chinese root guide links to the English native-runtime matrix, references and license instead of copying them.
ANCHORS_ONLY_IN = {"README.md": {"native-runtime", "references", "license"}, "README_ZH.md": {"references-and-license"}}
GUIDES = (REPO / "README.md", REPO / "README_ZH.md", *sorted(REPO.glob("bindings/*/README*.md")))
STATUS_VERSION = re.compile(r"(?:describes version|本文对应) [`*]*(\d+\.\d+\.\d+)")
FENCE_OPEN = re.compile(r"^[ \t]*(`{3,}|~{3,})")
ANCHOR = re.compile(r'<a id="([^"]+)"></a>')
LINK = re.compile(r"\]\(([^)\s]+)")
REPO_URL = re.compile(r"https://github\.com/0xf3cd/celestial-calendar/(?:blob|tree)/main/([^#]*)(?:#(.*))?")


def rendered(text: str) -> tuple[str, list[str]]:
  """Prose without HTML comments, and the fenced blocks (backtick or tilde) outside comments."""
  prose, blocks, block, fence, in_comment = [], [], [], "", False
  for line in text.splitlines():
    if fence:
      block.append(line)
      closing = line.strip()
      if closing.startswith(fence) and closing == fence[0] * len(closing):
        blocks.append("\n".join(block))
        block, fence = [], ""
      continue
    opening = None if in_comment else FENCE_OPEN.match(line)
    if opening:
      block, fence = [line], opening.group(1)
      continue
    prose.append(line)
    if "<!--" in line or "-->" in line:
      in_comment = line.rfind("<!--") > line.rfind("-->")
  assert not fence, "unbalanced code fence"
  return re.sub(r"<!--.*?-->", "", "\n".join(prose), flags=re.DOTALL), blocks


def code_blocks(text: str) -> list[str]:
  return rendered(text)[1]


def anchors(path: Path) -> set[str]:
  return set(ANCHOR.findall(rendered(path.read_text(encoding="utf-8"))[0]))


@pytest.mark.parametrize(("english", "chinese"), GUIDE_PAIRS)
def test_translated_guides_share_code_blocks(english, chinese):
  english_blocks = code_blocks((REPO / english).read_text(encoding="utf-8"))
  chinese_blocks = code_blocks((REPO / chinese).read_text(encoding="utf-8"))

  assert english_blocks
  assert chinese_blocks == english_blocks


@pytest.mark.parametrize(("english", "chinese"), GUIDE_PAIRS)
def test_translated_guides_share_anchors(english, chinese):
  english_anchors, chinese_anchors = anchors(REPO / english), anchors(REPO / chinese)

  assert english_anchors
  assert english_anchors - chinese_anchors == ANCHORS_ONLY_IN.get(english, set())
  assert chinese_anchors - english_anchors == ANCHORS_ONLY_IN.get(chinese, set())


@pytest.mark.parametrize("guide", GUIDES, ids=lambda path: str(path.relative_to(REPO)))
def test_guide_states_the_project_version(guide):
  prose = rendered(guide.read_text(encoding="utf-8"))[0]
  # docs/RELEASING.md, step 1: every guide names the version being released.
  assert STATUS_VERSION.findall(prose) == [project_version()]


def test_old_english_readme_points_to_the_guides():
  # Pages published with 0.7.0 and earlier link README_EN.md.
  prose = rendered((REPO / "README_EN.md").read_text(encoding="utf-8"))[0]
  assert "](README.md)" in prose and "](README_ZH.md)" in prose


def test_links_name_existing_files_and_explicit_anchors():
  documents = subprocess.run(
    ["git", "ls-files", "*.md"], cwd=REPO, capture_output=True, text=True, check=True
  ).stdout.split()
  assert "README.md" in documents
  failures = []
  for document in documents:
    for target in LINK.findall(rendered((REPO / document).read_text(encoding="utf-8"))[0]):
      repo_url = REPO_URL.fullmatch(target)
      if repo_url:
        path, fragment = REPO / repo_url.group(1), repo_url.group(2)
      elif re.match(r"[a-z]+:", target):
        continue
      else:
        file, _, fragment = target.partition("#")
        path = (REPO / document).parent / file if file else REPO / document
      if not path.exists():
        failures.append(f"{document}: {target} names no file")
      elif fragment and fragment not in anchors(path):
        failures.append(f'{document}: {target} names no <a id="{fragment}"></a> in {path.name}')
  assert not failures, "\n".join(failures)


def test_cpp_quickstart_compiles_and_runs(tmp_path):
  # README.md's C++ example is a whole program that needs only the headers.
  block = next(
    block for block in code_blocks((REPO / "README.md").read_text(encoding="utf-8")) if block.startswith("```cpp")
  )
  source = tmp_path / "quickstart.cpp"
  source.write_text("\n".join(block.splitlines()[1:-1]) + "\n", encoding="utf-8")
  compiler = shlex.split(os.environ.get("CXX", ""))
  if not compiler:
    resolved = shutil.which("clang++-22") or shutil.which("clang++") or shutil.which("g++")
    if not resolved:
      pytest.fail("The C++ quickstart requires a C++23 compiler")
    compiler = [resolved]
  binary = tmp_path / "quickstart"
  command = [*compiler, "-std=c++23", "-Isrc/astro", "-Isrc/calendar", "-Isrc/util", str(source), "-o", str(binary)]
  subprocess.run(command, cwd=REPO, check=True, capture_output=True, text=True)
  assert subprocess.check_output([str(binary)], text=True)


@pytest.mark.parametrize("opening", ["```sh", "~~~sh", "````sh"])
def test_unbalanced_fence_is_rejected(opening):
  with pytest.raises(AssertionError, match="unbalanced code fence"):
    code_blocks(f"```sh\nls\n```\n{opening}\n")


def test_tilde_fences_count_and_commented_fences_do_not():
  text = "~~~python\nprint(1)\n~~~\n<!--\n```sh\nhidden\n```\n-->\n```sh\nls\n```\n"
  assert code_blocks(text) == ["~~~python\nprint(1)\n~~~", "```sh\nls\n```"]


def test_a_shorter_or_different_fence_does_not_close_a_block():
  text = "````md\n```sh\nnested\n```\n~~~\n````\n"
  assert code_blocks(text) == ["````md\n```sh\nnested\n```\n~~~\n````"]


def test_prose_drops_comments_and_code():
  text = 'a<!-- [x](gone.md) -->b\n<!--\n<a id="hidden"></a>\n--> <!--\n-->\n```md\n[y](code.md)\n```\nc'
  assert rendered(text)[0] == "ab\n \nc"
