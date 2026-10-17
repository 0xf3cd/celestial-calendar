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
# Markdown here holds HTML comments of two shapes only: a plain-text header that opens the file, and a one-line
# marker such as `<!-- native-runtime-matrix -->`. Neither can hold a link or an anchor, so the checks below read the
# source as it is. Fences open and close in column 0. Anything else is refused rather than guessed at.
HEADER_COMMENT = re.compile(r"\A<!--[^<>\[\]]*-->\n")
MARKER_COMMENT = re.compile(r"^<!-- [a-z0-9-]+ -->$", re.MULTILINE)
FENCE = re.compile(r"^([ \t>]*)(`{3,}|~{3,})(.*)$")
ANCHOR = re.compile(r'<a id="([^"]+)"></a>')
# Inline links and images, reference definitions and HTML links, anywhere in a file, code included.
LINK_TARGET = re.compile(r'\]\(([^)\s]+)|^ {0,3}\[[^\]]+\]:[ \t]*(\S+)|href="([^"]+)"', re.MULTILINE)
REPO_URL = re.compile(r"https://github\.com/0xf3cd/celestial-calendar/(?:blob|tree)/main/([^#]*)(?:#(.*))?")


def without_comments(text: str) -> str:
  header = HEADER_COMMENT.match(text)
  rest = MARKER_COMMENT.sub("", text[header.end() :] if header else text)
  assert "<!--" not in rest and "-->" not in rest, "only a plain-text header comment and one-line marker comments"
  return rest


def split(text: str) -> tuple[str, list[str]]:
  """The prose without comments, and the fenced code blocks."""
  without_comments(text)
  prose, blocks, block, fence = [], [], [], ""
  for number, line in enumerate(text.splitlines(), 1):
    match = FENCE.match(line)
    if fence:
      block.append(line)
      closing = match and match[2][0] == fence[0] and len(match[2]) >= len(fence) and not match[3].strip()
      if closing and not match[1]:
        blocks.append("\n".join(block))
        block, fence = [], ""
      else:
        assert not (closing and len(match[1]) <= 3 and set(match[1]) == {" "}), f"line {number}: indented closing fence"
      continue
    if match:
      assert not match[1], f"line {number}: fences start in column 0"
      assert not (match[2][0] == "`" and "`" in match[3]), f"line {number}: backtick in a backtick fence's info string"
      block, fence = [line], match[2]
      continue
    prose.append(line)
  assert not fence, "unbalanced code fence"
  return without_comments("\n".join(prose)), blocks


def code_blocks(text: str) -> list[str]:
  return split(text)[1]


def anchors(path: Path) -> set[str]:
  return set(ANCHOR.findall(split(path.read_text(encoding="utf-8"))[0]))


def link_targets(text: str) -> list[str]:
  return [next(group for group in match.groups() if group) for match in LINK_TARGET.finditer(text)]


def documents() -> list[str]:
  return subprocess.run(
    ["git", "ls-files", "*.md"], cwd=REPO, capture_output=True, text=True, check=True
  ).stdout.split()


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
  prose = split(guide.read_text(encoding="utf-8"))[0]
  # docs/RELEASING.md, step 1: every guide names the version being released.
  assert STATUS_VERSION.findall(prose) == [project_version()]


def test_old_english_readme_points_to_the_guides():
  # Pages published with 0.7.0 and earlier link README_EN.md.
  prose = split((REPO / "README_EN.md").read_text(encoding="utf-8"))[0]
  assert "](README.md)" in prose and "](README_ZH.md)" in prose


def test_markdown_keeps_to_the_supported_comments_and_fences():
  failures = []
  for document in documents():
    try:
      split((REPO / document).read_text(encoding="utf-8"))
    except AssertionError as error:
      failures.append(f"{document}: {error}")
  assert not failures, "\n".join(failures)


def test_links_name_existing_files_and_explicit_anchors():
  assert "README.md" in documents()
  failures = []
  for document in documents():
    for target in link_targets((REPO / document).read_text(encoding="utf-8")):
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
    block
    for block in code_blocks((REPO / "README.md").read_text(encoding="utf-8"))
    if re.fullmatch(r"(`{3,}|~{3,})cpp\s*", block.splitlines()[0])
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


def test_tilde_fences_count():
  text = "~~~python\nprint(1)\n~~~\n```sh\nls\n```\n"
  assert code_blocks(text) == ["~~~python\nprint(1)\n~~~", "```sh\nls\n```"]


def test_a_shorter_or_different_fence_does_not_close_a_block():
  text = "````md\n```sh\nnested\n```\n~~~\n````\n"
  assert code_blocks(text) == ["````md\n```sh\nnested\n```\n~~~\n````"]


def test_prose_drops_the_header_and_marker_comments_and_code():
  text = "<!--\n  Header (C) 2026.\n-->\n# T\n<!-- native-runtime-matrix -->\n| a |\n```md\n[y](code.md)\n```\nc\n"
  assert split(text) == ("# T\n\n| a |\nc", ["```md\n[y](code.md)\n```"])


@pytest.mark.parametrize(
  "text",
  [
    "a\n<!-- [x](README.md#license) -->\n",
    "label<!-- [label](README.md#native-runtime) -->\n",
    "a\n<!--\nhidden\n",
    "a\n<!--\nhidden\n--> visible <!--\nhidden\n-->\n",
    "The token `<!--` is code.\n",
    "<!-- [x](README.md) -->\n# T\n",
    "# T\n<!--\n  Header.\n-->\n",
    "- item\n\n  ```sh\n  ls\n  ```\n",
    "> ```sh\n> ls\n> ```\n",
    "```sh\nls\n  ```\n",
    "```a`b\nls\n```\n",
  ],
)
def test_unsupported_markdown_is_refused(text):
  with pytest.raises(AssertionError):
    split(text)


def test_link_targets_cover_inline_reference_and_html_links():
  text = '[a](x.md) ![i](y.png#f)\n[r]: z.md\n   [s]: <w.md>\n<a href="v.md">v</a>\n`[c](code.md)`\n'
  assert link_targets(text) == ["x.md", "y.png#f", "z.md", "<w.md>", "v.md", "code.md"]
