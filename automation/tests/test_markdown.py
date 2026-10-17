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

from collections import Counter
from functools import cache
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote

import pytest

from markdown_it import MarkdownIt

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
STATUS_VERSION = re.compile(r"(?:describes\s+version|本文对应)\s+(\d+\.\d+\.\d+)")
REPO_URL = re.compile(r"https://github\.com/0xf3cd/celestial-calendar/(?:blob|tree)/main/([^#]*)(?:#(.*))?")
# GitHub renders CommonMark plus GFM tables. The checks below read the HTML this renders, never the source text.
MARKDOWN = MarkdownIt("commonmark").enable("table")


class Page(HTMLParser):
  """A Markdown text as its reader gets it: the links, ids, code blocks and text of the rendered HTML."""

  def __init__(self, markdown: str):
    super().__init__()
    self.targets: list[str] = []
    self.ids: set[str] = set()
    self.blocks: list[tuple[str, str]] = []
    self.bare_urls: list[str] = []
    self._text: list[str] = []
    self._inside: Counter[str] = Counter()
    self._block: tuple[str, list[str]] | None = None
    self.feed(MARKDOWN.render(markdown))
    self.close()
    self.text = "".join(self._text)

  def handle_starttag(self, tag, attrs):
    self._inside[tag] += 1
    attributes = dict(attrs)
    if attributes.get("id"):
      self.ids.add(attributes["id"])
    target = {"a": "href", "img": "src"}.get(tag)
    if target and attributes.get(target):
      self.targets.append(unquote(attributes[target]))
    if tag == "code" and self._inside["pre"]:
      self._block = ((attributes.get("class") or "").removeprefix("language-"), [])

  def handle_endtag(self, tag):
    self._inside[tag] -= 1
    if tag == "code" and self._block:
      self.blocks.append((self._block[0], "".join(self._block[1])))
      self._block = None

  def handle_data(self, data):
    self._text.append(data)
    if self._block:
      self._block[1].append(data)
    # GitHub links a repository URL written as plain text; this parser does not, so it must be a link here.
    if not (self._inside["a"] or self._inside["code"]):
      self.bare_urls += [word for word in data.split() if REPO_URL.search(word)]


@cache
def page(path: Path) -> Page:
  return Page(path.read_text(encoding="utf-8"))


def documents() -> list[str]:
  return subprocess.run(
    ["git", "ls-files", "*.md"], cwd=REPO, capture_output=True, text=True, check=True
  ).stdout.split()


@pytest.mark.parametrize(("english", "chinese"), GUIDE_PAIRS)
def test_translated_guides_share_code_blocks(english, chinese):
  english_blocks, chinese_blocks = page(REPO / english).blocks, page(REPO / chinese).blocks

  assert english_blocks
  assert chinese_blocks == english_blocks


@pytest.mark.parametrize(("english", "chinese"), GUIDE_PAIRS)
def test_translated_guides_share_anchors(english, chinese):
  english_anchors, chinese_anchors = page(REPO / english).ids, page(REPO / chinese).ids

  assert english_anchors
  assert english_anchors - chinese_anchors == ANCHORS_ONLY_IN.get(english, set())
  assert chinese_anchors - english_anchors == ANCHORS_ONLY_IN.get(chinese, set())


def test_chinese_root_guide_links_what_it_does_not_copy():
  targets = set(page(REPO / "README_ZH.md").targets)
  assert {f"README.md#{anchor}" for anchor in ANCHORS_ONLY_IN["README.md"]} <= targets


@pytest.mark.parametrize("guide", GUIDES, ids=lambda path: str(path.relative_to(REPO)))
def test_guide_states_the_project_version(guide):
  # docs/RELEASING.md, step 1: every guide names the version being released.
  assert STATUS_VERSION.findall(page(guide).text) == [project_version()]


def test_old_english_readme_points_to_the_guides():
  # Pages published with 0.7.0 and earlier link README_EN.md.
  assert {"README.md", "README_ZH.md"} <= set(page(REPO / "README_EN.md").targets)


def test_links_name_existing_files_and_explicit_anchors():
  assert "README.md" in documents()
  failures = []
  for name in documents():
    failures += [f"{name}: write {url} as a [text](url) link" for url in page(REPO / name).bare_urls]
    for target in page(REPO / name).targets:
      repo_url = REPO_URL.fullmatch(target)
      if repo_url:
        path, fragment = REPO / repo_url.group(1), repo_url.group(2)
      elif re.match(r"[a-z]+:", target):
        continue
      else:
        file, _, fragment = target.partition("#")
        path = (REPO / name).parent / file if file else REPO / name
      if not path.exists():
        failures.append(f"{name}: {target} names no file")
      elif fragment and fragment not in page(path).ids:
        failures.append(f'{name}: {target} names no <a id="{fragment}"></a> in {path.name}')
  assert not failures, "\n".join(failures)


def test_cpp_quickstart_compiles_and_runs(tmp_path):
  # README.md's C++ example is a whole program that needs only the headers.
  block = next(code for language, code in page(REPO / "README.md").blocks if language == "cpp")
  source = tmp_path / "quickstart.cpp"
  source.write_text(block, encoding="utf-8")
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


@pytest.mark.parametrize(
  ("text", "targets"),
  [
    ("[a](x.md) ![i](y.png#f) <https://e.org/>\n", ["x.md", "y.png#f", "https://e.org/"]),
    ("[a](<../w x.md>) [b](z.md#许可证)\n", ["../w x.md", "z.md#许可证"]),
    ("[a][r]\n\n[r]:\n  z.md\n", ["z.md"]),
    ("<a href='s.md'>s</a> <a href=u.md>u</a>\n\n<p><img src=\"i.png\"></p>\n", ["s.md", "u.md", "i.png"]),
    ("`[c](code.md)`\n\n```md\n[f](fence.md)\n```\n\n<!-- [h](hidden.md) -->\n\n[unused]: unused.md\n", []),
    ("<!--\nh\n--> v <!--\nh\n-->\n\n[x](swallowed.md)\n", []),
    ("| `a | [t](t.md) | b` |\n|---|---|---|\n", ["t.md"]),
  ],
)
def test_targets_are_the_rendered_links(text, targets):
  assert Page(text).targets == targets


def test_ids_are_those_of_rendered_tags():
  text = '<a id="a"></a>\n\n`<a id="code"></a>`\n\n<!-- <a id="hidden"></a> -->\n<p id="p">p</p>\n'
  assert Page(text).ids == {"a", "p"}


def test_blocks_are_the_rendered_code_blocks():
  text = "~~~sh\nls\n~~~\n\n- ```python\n  print(1)\n  ```\n\n<pre>\n```sh\nraw\n```\n</pre>\n\n    indented\n"
  assert Page(text).blocks == [("sh", "ls\n"), ("python", "print(1)\n"), ("", "indented\n")]


def test_version_sentence_is_read_from_the_text_alone():
  text = '[status](https://e.org "describes version 9.9.9.") <!-- 本文对应 9.9.9 -->\n\n'
  text += "See `x`. This guide describes\nversion **0.8.0**.\n"
  assert Page(text).text == "status \nSee x. This guide describes\nversion 0.8.0.\n"
  assert STATUS_VERSION.findall(Page(text).text) == ["0.8.0"]


def test_bare_repo_urls_are_found_outside_links_and_code():
  url = "https://github.com/0xf3cd/celestial-calendar/blob/main/x.md"
  assert Page(f"<{url}> [{url}]({url}) `{url}`\n\nsee {url}\n").bare_urls == [url]
