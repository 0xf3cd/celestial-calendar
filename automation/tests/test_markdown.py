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
import importlib.util
import json
import re
import shlex
import shutil
import subprocess

from functools import cache
from pathlib import Path
from urllib.parse import unquote

import pytest

from markdown_it.token import Token

from automation.markdown_source import link_targets, links, section_text, tokens, visible_text

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
PYTHON_READER = REPO / "bindings/python/test/consumer/doc_examples.py"
NODE_READER = REPO / "bindings/javascript/test/support/doc_examples.mjs"
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
# The checks read the parse in `automation.markdown_source`, never the source text. Raw HTML is held to what the
# guides use, comments and empty anchors: how a browser reads any other HTML is not something the parse can say.
COMMENTS = re.compile(r"\s*(?:<!--(?!-?>)(?:(?!--!?>).)*-->\s*)+", re.DOTALL)
ANCHOR = re.compile(r'<a id="([a-z0-9-]+)">')


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


def test_model_explanation_has_one_shared_home():
  for name in ("README.md", "README_ZH.md", "docs/DESIGN.md", "docs/MIGRATING_1_0.md", "docs/API.md"):
    assert any("ALGORITHMS.md" in target for target in link_targets(document(REPO / name))), name
  for name in ("python", "javascript"):
    for edition in ("README.md", "README_ZH.md"):
      parsed = document(REPO / "bindings" / name / edition)
      targets = link_targets(parsed)
      assert any(target.endswith("ALGORITHMS.md#lunar-algorithms") for target in targets)
      assert any(target.endswith("ALGORITHMS.md#delta-t-models") for target in targets)
      headers = [
        token.content
        for index, token in enumerate(parsed)
        if token.type == "inline" and parsed[index - 1].type == "th_open"
      ]
      assert "Lunar-year domain" not in headers and "Hard year domain" not in headers


def test_build_guide_preserves_moved_maintenance_navigation():
  parsed = document(REPO / "docs/BUILDING.md")
  targets = link_targets(parsed)
  assert {f"MAINTAINING.md#{anchor}" for anchor in ("ci-jobs", "artifacts", "todo")} <= set(targets)
  assert {"ci-jobs", "artifacts", "todo"} <= anchors(REPO / "docs/MAINTAINING.md")
  assert "Workflow · job" not in visible_text(parsed)
  assert "Workflow · job" in visible_text(document(REPO / "docs/MAINTAINING.md"))


@pytest.mark.parametrize("guide", GUIDES, ids=lambda path: str(path.relative_to(REPO)))
def test_source_guide_warns_about_unreleased_changes(guide):
  text = visible_text(document(guide))
  assert "unreleased changes" in text or "尚未发布的改动" in text


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
  blocks = [code for info, code in code_blocks(document(REPO / "README.md")) if info == "cpp"]
  assert len(blocks) == 1, "expected exactly one complete C++ quickstart"
  block = blocks[0]
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


@pytest.fixture
def python_reader():
  spec = importlib.util.spec_from_file_location("doc_examples_reader", PYTHON_READER)
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


def node_command():
  node = shutil.which("node")
  assert node, "documentation reader comparison requires Node"
  return node


@pytest.mark.parametrize("guide", GUIDES, ids=lambda path: str(path.relative_to(REPO)))
def test_installed_consumers_read_every_rendered_fence(python_reader, guide):
  text = guide.read_text(encoding="utf-8")
  canonical = code_blocks(document(guide))
  assert canonical
  assert python_reader.read_fences(text) == canonical
  node = subprocess.run(
    [node_command(), str(NODE_READER), "--fences-json", str(guide)],
    check=True,
    capture_output=True,
    encoding="utf-8",
  )
  assert json.loads(node.stdout) == [list(block) for block in canonical]


@pytest.mark.parametrize("guide", ["README.md", "bindings/python/README.md"])
def test_python_fences_are_all_classified(python_reader, guide):
  text = (REPO / guide).read_text(encoding="utf-8")
  selected = python_reader.programs(text)
  assert selected == [body for info, body in code_blocks(document(REPO / guide)) if info == "python"]
  with pytest.raises(AssertionError, match="unclassified"):
    python_reader.programs(text + "\n```unknown\nnew program\n```\n")
  with pytest.raises(AssertionError, match="missing Python"):
    python_reader.programs("```sh\npython example.py\n```\n")


def test_python_added_programs_execute_without_shared_imports(python_reader, tmp_path, monkeypatch):
  monkeypatch.setenv("PYTHONIOENCODING", "ascii")
  text = "```python\nimport math\nprint(math.pi > 3)\n```\n"
  text += "\n```python\nprint('冬至')\n```\n"
  selected = python_reader.programs(text)
  assert len(selected) == 2
  assert [python_reader.run_program(code, tmp_path) for code in selected] == ["True\n", "冬至\n"]
  with pytest.raises(AssertionError, match="NameError"):
    python_reader.run_program("print(math.pi)\n", tmp_path)
  with pytest.raises(AssertionError, match="displayed output mismatch"):
    python_reader.run_program("print('actual')\n", tmp_path, "wrong\n")
  with pytest.raises(AssertionError, match=r"\\u51ac") as failure:
    python_reader.run_program("raise RuntimeError('冬至')\n", tmp_path)
  assert str(failure.value).isascii()


def test_python_output_and_metadata_controls(python_reader):
  root = (REPO / "README.md").read_text(encoding="utf-8")
  expected = python_reader.expected_output(root)
  assert expected != python_reader.expected_output(root.replace(expected, "wrong output\n"))
  for text in ("", root + "\n```text\nextra\n```\n"):
    with pytest.raises(AssertionError, match="exactly one"):
      python_reader.expected_output(text)
  raw = "Name: celestial-calendar\r\n\r\n# Guide\r\n冬至\r\n"
  assert python_reader.metadata_description(raw) == "# Guide\n冬至\n"
  with pytest.raises(AssertionError, match="missing METADATA"):
    python_reader.metadata_description("Name: celestial-calendar\n")


def test_python_program_rejects_checkout_cwd_before_writing_or_launching(python_reader, tmp_path, monkeypatch):
  checkout = tmp_path / "checkout"
  cwd = checkout / "nested"
  cwd.mkdir(parents=True)
  monkeypatch.setattr(python_reader, "REPO", checkout)
  monkeypatch.setattr(python_reader.subprocess, "run", lambda *args, **kwargs: pytest.fail("child was launched"))
  with pytest.raises(AssertionError, match="outside the checkout"):
    python_reader.run_program("print('example')\n", cwd)
  assert not list(cwd.iterdir())


def test_python_provenance_rejects_checkout_and_nonmember(python_reader, tmp_path):
  package, native = tmp_path / "installed/__init__.py", tmp_path / "installed/native.so"
  record = {
    "package": str(package),
    "native": str(native),
    "prefix": str(tmp_path),
    "files": [str(package), str(native)],
  }
  python_reader.verify_provenance(record)
  with pytest.raises(AssertionError, match="distribution member"):
    python_reader.verify_provenance({**record, "files": [str(package)]})
  checkout = str(REPO / "bindings/python/src/celestial_calendar/__init__.py")
  with pytest.raises(AssertionError, match="checkout import"):
    python_reader.verify_provenance({**record, "package": checkout, "prefix": str(REPO), "files": [checkout]})


@pytest.mark.parametrize("skip", [False, True])
def test_python_main_reconciles_actual_execution_count(python_reader, tmp_path, monkeypatch, skip):
  source = PYTHON_READER.read_text(encoding="utf-8")
  if skip:
    assert source.count("for code in selected:") == 1
    source = source.replace("for code in selected:", "for code in selected[:1]:")
    exec(compile(source, str(PYTHON_READER), "exec"), python_reader.__dict__)
  root = tmp_path / "checkout"
  (root / "bindings/python").mkdir(parents=True)
  guide = "```python\nprint('one')\n```\n\n```python\nprint('two')\n```\n"
  (root / "bindings/python/README.md").write_text(guide, encoding="utf-8")
  (root / "README.md").write_text("```python\nprint('root')\n```\n\n```text\nroot\n```\n", encoding="utf-8")
  calls = []

  def run(code, cwd, expected=None):
    calls.append(code)
    if code.startswith("import importlib.metadata"):
      return json.dumps({"metadata": "Name: celestial-calendar\n\n" + guide})
    return expected or "unused\n"

  monkeypatch.setattr(python_reader, "REPO", root)
  monkeypatch.setattr(python_reader, "run_program", run)
  monkeypatch.setattr(python_reader, "verify_provenance", lambda record: None)
  if skip:
    with pytest.raises(AssertionError):
      python_reader.main()
  else:
    python_reader.main()
    assert calls[1:] == ["print('root')\n", "print('one')\n", "print('two')\n"]


def test_node_skipped_program_is_rejected(tmp_path):
  source = NODE_READER.read_text(encoding="utf-8")
  assert source.count("for (const code of selected)") == 1
  mutated = tmp_path / "doc_examples.mjs"
  mutated.write_text(source.replace("for (const code of selected)", "for (const code of selected.slice(0, 1))"))
  driver = f"""
import assert from 'node:assert/strict';
import {{ runPrograms }} from {json.dumps(mutated.as_uri())};
await assert.rejects(() => runPrograms('```js\\nconsole.log(1);\\n```\\n```js\\nconsole.log(2);\\n```\\n',
  {json.dumps(str(tmp_path))}, '@0xf3cd/celestial'), /1 !== 2/);
console.log('PASS skipped JavaScript program rejected');
"""
  result = subprocess.run(
    [node_command(), "--input-type=module", "-e", driver], cwd=tmp_path, capture_output=True, encoding="utf-8"
  )
  assert result.returncode == 0, result.stdout + result.stderr
  assert result.stdout == "PASS skipped JavaScript program rejected\n"


def test_node_program_coverage_isolation_and_output_controls(tmp_path):
  # Each rejection reaches a launched Node/Python runtime; a missing tool is not a detected example defect.
  guides = ["README.md", "bindings/javascript/README.md", "bindings/javascript-alias/README.md"]
  source = f"""
import assert from 'node:assert/strict';
import {{ readFile }} from 'node:fs/promises';
import {{ programs, readFences, expectedOutput, runPrograms }} from {json.dumps(NODE_READER.as_uri())};
const cwd = {json.dumps(str(tmp_path))};
const fence = (kind, body) => '```' + kind + '\\n' + body + '\\n```\\n';
const text = fence('js', 'import assert from "node:assert/strict"; assert(true);')
  + fence('js', 'console.log("冬至");');
assert.equal(programs(text).length, 2);
assert.equal(await runPrograms(text, cwd, '@0xf3cd/celestial'), 2);
assert.throws(() => programs(fence('sh', 'node example.mjs')), /missing JavaScript/);
assert.throws(() => programs(text + fence('unknown', 'new')), /unclassified/);
await assert.rejects(
  () => runPrograms(fence('js', 'console.log(missingImport);'), cwd, '@0xf3cd/celestial'), /ReferenceError/);
await assert.rejects(
  () => runPrograms(fence('js', 'console.log("actual");'), cwd, '@0xf3cd/celestial', 'wrong\\n'), /displayed output/);
assert.throws(() => expectedOutput(''), /exactly one/);
assert.throws(() => expectedOutput(fence('text', 'one') + fence('text', 'two')), /exactly one/);
assert.deepEqual(readFences('<!--\\n' + fence('js', 'hidden') + '-->\\n' + text).length, 2);
for (const guide of {json.dumps([str(REPO / name) for name in guides])}) {{
  const blocks = readFences(await readFile(guide, 'utf8'));
  assert.equal(programs(await readFile(guide, 'utf8')).length, blocks.filter(([info]) => info === 'js').length);
}}
console.log('PASS Node displayed-program controls');
"""
  result = subprocess.run(
    [node_command(), "--input-type=module", "-e", source], cwd=tmp_path, capture_output=True, encoding="utf-8"
  )
  assert result.returncode == 0, result.stdout + result.stderr
  assert result.stdout == "PASS Node displayed-program controls\n"


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


def test_links_are_the_rendered_links():
  text = "[a](x.md#1) and `[b](y.md)` and <!-- [c](z.md) --> and [`d`](w.md)\n"
  assert links(tokens(text)) == [("a", "x.md#1"), ("d", "w.md")]


def test_section_text_is_the_rendered_prose_of_one_section():
  text = "# T\n\n## Rules\n\nUnder MIT. <!-- not here -->\n\n```\nfenced\n```\n\n### Sub\n\nkept\n\n## Next\n\nother\n"
  assert section_text(text, "Rules") == "\nUnder MIT. \nSub\nkept"
  assert section_text(text, "Next") == "\nother"
  assert section_text(text, "Sub") is None  # only ## headings address a section
  assert section_text(text, "Missing") is None
  assert section_text("## A\n\none\n\n## A\n\ntwo\n", "A") is None
  assert section_text("# A\n\ntop\n", "A") is None  # not a level-2 heading


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
