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

from functools import cache
from pathlib import Path
from urllib.parse import unquote

import pytest

from markdown_it import MarkdownIt
from markdown_it.token import Token

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
STATUS_VERSION = re.compile(r"(?:describes version|本文对应) (\d+\.\d+\.\d+)")
# A link to this repository takes one of these spellings, so that a dead one cannot hide behind another: any other
# scheme, host, port, case, percent-escape or route is reported rather than recognised. Each names a path to check
# except the releases page; the home page shows README.md.
REPO_HOME = "https://github.com/0xf3cd/celestial-calendar"
REPO_ROUTES = {"blob/main": REPO, "tree/main": REPO, "actions/workflows": REPO / ".github/workflows"}
REPO_URL = re.compile(
  rf"{re.escape(REPO_HOME)}(?:(?P<releases>/releases)|(?:/(?P<route>{'|'.join(REPO_ROUTES)})/(?P<path>[^#?]*))?"
  r"(?:#(?P<fragment>.*))?)"
)
REPO_SUFFIXES = ("[#<fragment>]", "/releases", *(f"/{route}/<path>[#<fragment>]" for route in REPO_ROUTES))
# markdown-it with tables stands in for GitHub's GFM renderer, and the checks read its parse, never the source text.
# Raw HTML is held to what the guides use, comments and empty anchors: how a browser reads any other HTML is not
# something the parse can say.
MARKDOWN = MarkdownIt("commonmark").enable("table")
COMMENTS = re.compile(r"\s*(?:<!--(?!-?>)(?:(?!--!?>).)*-->\s*)+", re.DOTALL)
ANCHOR = re.compile(r'<a id="([a-z0-9-]+)">')


def tokens(text: str) -> list[Token]:
  """The block tokens and, after each inline one, its children."""
  return [part for token in MARKDOWN.parse(text) for part in (token, *(token.children or ()))]


@cache
def document(path: Path) -> list[Token]:
  return tokens(path.read_text(encoding="utf-8"))


def unsupported_html(parsed: list[Token]) -> list[str]:
  """Raw HTML other than comments and empty `<a id="..."></a>` anchors."""
  found, index = [], 0
  while index < len(parsed):
    token, following = parsed[index], parsed[index + 1] if index + 1 < len(parsed) else None
    if token.type == "html_inline" and ANCHOR.fullmatch(token.content) and following and following.content == "</a>":
      index += 2
      continue
    if token.type in ("html_block", "html_inline") and not COMMENTS.fullmatch(token.content):
      found.append(token.content.strip())
    index += 1
  return found


def anchors(path: Path) -> set[str]:
  return {
    match[1] for token in document(path) if token.type == "html_inline" and (match := ANCHOR.fullmatch(token.content))
  }


def code_blocks(parsed: list[Token]) -> list[tuple[str, str]]:
  return [(token.info, token.content) for token in parsed if token.type in ("fence", "code_block")]


def link_targets(parsed: list[Token]) -> list[str]:
  """Where the links and images point, percent-escaped as rendered."""
  return [
    token.attrs["href" if token.type == "link_open" else "src"]
    for token in parsed
    if token.type in ("link_open", "image")
  ]


def names_this_repository(text: str) -> bool:
  return "0xf3cd/celestial-calendar" in unquote(text).lower()


def misspelt_repo_url(target: str) -> bool:
  return names_this_repository(target) and not REPO_URL.fullmatch(target)


def resolve(name: str, target: str) -> tuple[Path, str] | None:
  """The path and fragment that a link in document `name` names, or None off the repository and for its releases."""
  # Split, then decode: an escaped `%23` belongs to the file name.
  repo_url = REPO_URL.fullmatch(target)
  if repo_url:
    if repo_url["releases"]:
      return None
    fragment = unquote(repo_url["fragment"] or "")
    if not repo_url["route"]:
      return REPO / "README.md", fragment
    return REPO_ROUTES[repo_url["route"]] / unquote(repo_url["path"]), fragment
  if re.match(r"[a-z][a-z0-9+.-]*:|//", target, re.IGNORECASE):
    return None
  file, _, fragment = target.partition("#")
  return ((REPO / name).parent / unquote(file) if file else REPO / name), unquote(fragment)


def link_problem(name: str, target: str) -> str | None:
  """What is wrong with a link in document `name`, or None for a link that resolves or is off the repository."""
  if misspelt_repo_url(target):
    suffixes = ", ".join(REPO_SUFFIXES)
    return f"{target} names this repository, which only {REPO_HOME} followed by one of these may do: {suffixes}"
  resolved = resolve(name, target)
  if resolved is None:
    return None
  path, fragment = resolved
  if not path.resolve().is_relative_to(REPO):
    return f"{target} resolves outside the repository; write the path relative to {name}"
  if path.resolve() not in tracked():
    return f"{target} names no file or directory that git tracks"
  if fragment and path.is_dir():
    return f"{target} puts a fragment on a directory"
  if fragment and fragment not in anchors(path):
    return f'{target} names no <a id="{fragment}"></a> in {path.name}'
  return None


def visible_text(parsed: list[Token]) -> str:
  """The text of paragraphs, headings and table cells, code spans included; link titles and comments are not text."""
  parts = []
  for token in parsed:
    if token.type == "inline":
      parts.append("\n")
    elif token.type in ("text", "code_inline"):
      parts.append(token.content)
    elif token.type in ("softbreak", "hardbreak"):
      parts.append(" ")
  return "".join(parts)


def bare_repo_urls(parsed: list[Token]) -> list[str]:
  """Words outside links that name this repository; GFM links a URL written as plain text, markdown-it does not."""
  found, depth = [], 0
  for token in parsed:
    depth += {"link_open": 1, "link_close": -1}.get(token.type, 0)
    if token.type == "text" and not depth:
      found += [word for word in token.content.split() if names_this_repository(word)]
  return found


def documents() -> list[str]:
  listing = subprocess.run(
    ["git", "ls-files", "-z", "*.md"], cwd=REPO, capture_output=True, text=True, check=True
  ).stdout
  return [file for file in listing.split("\0") if file]


@cache
def tracked() -> set[Path]:
  """The files git tracks and the directories above them, which GitHub shows; a checkout holds more."""
  listing = subprocess.run(["git", "ls-files", "-z"], cwd=REPO, capture_output=True, text=True, check=True).stdout
  files = [Path(file) for file in listing.split("\0") if file]
  return {REPO / path for file in files for path in (file, *file.parents)}


@pytest.mark.parametrize(("english", "chinese"), GUIDE_PAIRS)
def test_translated_guides_share_code_blocks(english, chinese):
  english_blocks, chinese_blocks = code_blocks(document(REPO / english)), code_blocks(document(REPO / chinese))

  assert english_blocks
  assert chinese_blocks == english_blocks


@pytest.mark.parametrize(("english", "chinese"), GUIDE_PAIRS)
def test_translated_guides_share_anchors(english, chinese):
  english_anchors, chinese_anchors = anchors(REPO / english), anchors(REPO / chinese)

  assert english_anchors
  assert english_anchors - chinese_anchors == ANCHORS_ONLY_IN.get(english, set())
  assert chinese_anchors - english_anchors == ANCHORS_ONLY_IN.get(chinese, set())


def test_chinese_root_guide_links_what_it_does_not_copy():
  targets = set(link_targets(document(REPO / "README_ZH.md")))
  assert {f"README.md#{anchor}" for anchor in ANCHORS_ONLY_IN["README.md"]} <= targets


@pytest.mark.parametrize("guide", GUIDES, ids=lambda path: str(path.relative_to(REPO)))
def test_guide_states_the_project_version(guide):
  # docs/RELEASING.md, step 1: every guide names the version being released.
  assert STATUS_VERSION.findall(visible_text(document(guide))) == [project_version()]


def test_old_english_readme_points_to_the_guides():
  # Pages published with 0.7.0 and earlier link README_EN.md.
  assert {"README.md", "README_ZH.md"} <= set(link_targets(document(REPO / "README_EN.md")))


def test_markdown_holds_no_raw_html_beyond_comments_and_anchors():
  failures = [
    f'{name}: {html!r} is neither a comment nor an empty <a id="..."></a>; write it in Markdown'
    for name in documents()
    for html in unsupported_html(document(REPO / name))
  ]
  assert not failures, "\n".join(failures)


def test_links_name_existing_paths_and_explicit_anchors():
  assert "README.md" in documents()
  failures = []
  for name in documents():
    parsed = document(REPO / name)
    failures += [
      f"{name}: {url} names this repository outside a link; link it or put it in a code span"
      for url in bare_repo_urls(parsed)
    ]
    failures += [f"{name}: {problem}" for target in link_targets(parsed) if (problem := link_problem(name, target))]
  assert not failures, "\n".join(failures)


def test_cpp_quickstart_compiles_and_runs(tmp_path):
  # README.md's C++ example is a whole program that needs only the headers.
  block = next(code for info, code in code_blocks(document(REPO / "README.md")) if info.split()[:1] == ["cpp"])
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
    ("[a][r]\n\n[r]:\n  z.md\n", ["z.md"]),
    ("`[c](code.md)`\n\n```md\n[f](fence.md)\n```\n\n<!-- [h](hidden.md) -->\n\n[unused]: unused.md\n", []),
    ("| `a | [t](t.md) | b` |\n|---|---|---|\n", ["t.md"]),
  ],
)
def test_link_targets_are_the_rendered_links(text, targets):
  assert link_targets(tokens(text)) == targets


@pytest.mark.parametrize(
  ("target", "file", "fragment"),
  [
    ("../README.md%23license", "README.md#license", ""),
    ("../w%20x.md#%E8%AE%B8%E5%8F%AF%E8%AF%81", "w x.md", "许可证"),
    ("#license", "DESIGN.md", "license"),
    ("../README.md#a:b", "README.md", "a:b"),
    ("https://github.com/0xf3cd/celestial-calendar/blob/main/docs/API.md#c-abi", "API.md", "c-abi"),
    ("https://github.com/0xf3cd/celestial-calendar/blob/main/w%20x.md#%E8%AE%B8%E5%8F%AF", "w x.md", "许可"),
    ("https://github.com/0xf3cd/celestial-calendar/tree/main/docs", "docs", ""),
    ("https://github.com/0xf3cd/celestial-calendar#cpp", "README.md", "cpp"),
    ("https://github.com/0xf3cd/celestial-calendar/actions/workflows/core_tests.yml", "core_tests.yml", ""),
  ],
)
def test_targets_resolve_to_a_file_and_fragment(target, file, fragment):
  path, anchor = resolve("docs/DESIGN.md", target)
  assert (path.name, anchor) == (file, fragment)


@pytest.mark.parametrize(
  "target",
  [
    "https://example.org/a#b",
    "//example.org/a",
    "mailto:a@example.org",
    "https://github.com/0xf3cd/celestial-calendar/releases",
  ],
)
def test_targets_off_the_repository_resolve_to_nothing(target):
  assert resolve("README.md", target) is None


def test_repository_root_url_resolves_to_the_root():
  assert resolve("docs/DESIGN.md", f"{REPO_HOME}/tree/main/#time-scales") == (REPO, "time-scales")


@pytest.mark.parametrize(
  ("target", "misspelt"),
  [
    ("https://github.com:443/0xf3cd/celestial-calendar/blob/main/x.md", True),
    ("https://github.com/0xf3c%64/celestial-calendar/blob/main/x.md", True),
    ("http://www.github.com/0xf3cd/celestial-calendar/blob/main/x.md", True),
    ("https://GitHub.com/0xF3CD/celestial-calendar", True),
    ("https://github.com/0xf3cd/celestial-calendar/blob/MAIN/x.md", True),
    ("https://raw.githubusercontent.com/0xf3cd/celestial-calendar/main/x.md", True),
    ("https://github.com/0xf3cd/celestial-calendar-fork", True),
    ("https://github.com/0xf3cd/celestial-calendar/raw/main/x.md", True),
    ("https://github.com/0xf3cd/celestial-calendar//blob/main/x.md", True),
    ("https://github.com/0xf3cd/celestial-calendar/blob/main/x.md?raw=1", True),
    ("https://github.com/0xf3cd/celestial-calendar/tree/main", True),
    ("https://github.com/0xf3cd/celestial-calendar/releases#v0.7.0", True),
    ("https://github.com/0xf3cd/celestial-calendar/releases/", True),
    ("https://github.com/0xf3cd/celestial-calendar", False),
    ("https://github.com/0xf3cd/celestial-calendar#cpp", False),
    ("https://github.com/0xf3cd/celestial-calendar/releases", False),
    ("https://github.com/0xf3cd/celestial-calendar/actions/workflows/core_tests.yml", False),
    ("https://github.com/0xf3cd/celestial-calendar/tree/main/bindings/javascript", False),
    ("../README.md#license", False),
  ],
)
def test_repository_links_take_one_spelling(target, misspelt):
  assert misspelt_repo_url(target) is misspelt


@pytest.mark.parametrize(
  ("target", "problem"),
  [
    ("http://github.com/0xf3cd/celestial-calendar/blob/main/README.md", "names this repository"),
    ("//github.com/0xf3cd/celestial-calendar/blob/main/README.md", "names this repository"),
    ("/usr", "resolves outside the repository; write the path relative to docs/DESIGN.md"),
    ("../../../../../../../../../../usr", "resolves outside the repository; write the path relative to docs/DESIGN.md"),
    (
      f"{REPO_HOME}/tree/main/../../../../../../../../../../usr",
      "resolves outside the repository; write the path relative to docs/DESIGN.md",
    ),
    ("../nonexistent.md", "names no file or directory that git tracks"),
    ("../.git", "names no file or directory that git tracks"),
    ("../src#layers", "puts a fragment on a directory"),
    (f"{REPO_HOME}/tree/main/docs#a", "puts a fragment on a directory"),
    ("../README.md#nonexistent", 'names no <a id="nonexistent"></a> in README.md'),
    (f"{REPO_HOME}#readme", 'names no <a id="readme"></a> in README.md'),
    (f"{REPO_HOME}/raw/main/README.md", "names this repository"),
    ("https://img.shields.io/github/stars/0xf3cd/celestial-calendar", "names this repository"),
    (f"{REPO_HOME}/actions/workflows/nonexistent.yml", "names no file or directory that git tracks"),
    ("../README.md#license", None),
    ("../src", None),
    ("../bindings", None),
    ("../src/util", None),
    ("../third_party", None),
    (f"{REPO_HOME}/tree/main/", None),
    (f"{REPO_HOME}/blob/main/README.md#license", None),
    ("https://example.org/a#b", None),
  ],
)
def test_link_problems_are_named(target, problem):
  found = link_problem("docs/DESIGN.md", target)
  assert found is None if problem is None else (found or "").startswith(f"{target} {problem}")


@pytest.mark.parametrize("suffix", REPO_SUFFIXES)
def test_listed_suffixes_are_the_ones_accepted(suffix):
  link = REPO_HOME + suffix.replace("<path>", "README.md").removesuffix("[#<fragment>]")
  assert not misspelt_repo_url(link)
  assert misspelt_repo_url(f"{link}#a") is not suffix.endswith("[#<fragment>]")


def test_misspelt_links_are_reported_with_every_suffix():
  problem = link_problem("README.md", f"{REPO_HOME}/raw/main/README.md")
  assert problem.endswith(f"{REPO_HOME} followed by one of these may do: {', '.join(REPO_SUFFIXES)}")
  listed = {suffix.removesuffix("[#<fragment>]").removesuffix("/<path>") for suffix in REPO_SUFFIXES}
  assert listed == {"", "/releases", *(f"/{route}" for route in REPO_ROUTES)}


def test_anchors_are_the_empty_anchor_tags(tmp_path):
  page = tmp_path / "page.md"
  page.write_text('<a id="a"></a>\n\n`<a id="code">`\n\n<!-- <a id="hidden"></a> -->\n', encoding="utf-8")
  assert anchors(page) == {"a"}


def test_code_blocks_are_the_rendered_blocks():
  text = "~~~sh\nls\n~~~\n\n- ```python\n  print(1)\n  ```\n\nInline `x`.\n\n    indented\n"
  assert code_blocks(tokens(text)) == [("sh", "ls\n"), ("python", "print(1)\n"), ("", "indented\n")]


def test_version_sentence_is_read_from_the_text_alone():
  text = '[status](https://e.org "describes version 9.9.9.") <!-- 本文对应 9.9.9 -->\n\n'
  text += "See `x`. This guide describes\nversion **0.8.0**.\n"
  assert visible_text(tokens(text)) == "\nstatus \nSee x. This guide describes version 0.8.0."
  assert STATUS_VERSION.findall(visible_text(tokens(text))) == ["0.8.0"]


def test_bare_repo_urls_are_found_outside_links_and_code():
  url, www, port = (
    "https://github.com/0xf3cd/celestial-calendar/blob/main/x.md",
    "www.github.com/0xf3cd/celestial-calendar/tree/main/y",
    "https://github.com:443/0xf3c%64/celestial-calendar",
  )
  text = f"<{url}> [{url}]({url}) `{url}`\n\nsee {url} and {www} or {port}\n"
  assert bare_repo_urls(tokens(text)) == [url, www, port]


@pytest.mark.parametrize(
  "text",
  [
    '<script>\n<a href="gone.md">x</a>\n</script>\n',
    "<a href='gone.md'>x</a>\n",
    "<pre><code>only here\n</pre>\n",
    '<p id="p">p</p>\n',
    "</a>\n\nhttps://github.com/0xf3cd/celestial-calendar/blob/main/gone.md\n",
    'Open <a id="x">anchor\n',
    "<!--\nh\n--> v <!--\nh\n-->\n",
    "a\n\n<!--\nunclosed\n",
    '<!-- a --!> <a href="gone.md">x</a> -->\n',
    "<!-->\n",
    '<a id="r5&amp;x"></a>\n',
    '<a id="Upper"></a>\n',
  ],
)
def test_other_raw_html_is_reported(text):
  assert unsupported_html(tokens(text))


def test_comments_and_empty_anchors_are_not_reported():
  text = (
    '<!--\n  Header.\n-->\n# T\n<!-- marker -->\n| a |\n|---|\n\n<a id="x"></a><a id="y"></a>\n## H <!-- note -->\n'
  )
  assert unsupported_html(tokens(text)) == []
