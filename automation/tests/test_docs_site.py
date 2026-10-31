# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import json
import os
import shutil
import sys
from pathlib import Path

import pytest

import project
from automation import build, docs_site as site, markdown_source
from automation.utils import ProcReturn

ROOT = Path(__file__).parents[2]
SHA = "1" * 40
VERSION = "1.0.0"


@pytest.fixture
def sources(tmp_path, monkeypatch):
  root, api, output = (tmp_path / name for name in ("repo", "api", "output"))
  for directory in (root, api, output):
    directory.mkdir()
  guides = ("docs/guide.md", "bindings/python/README.md")
  monkeypatch.setattr(site, "GUIDES", guides)
  monkeypatch.setattr(
    site, "NAVIGATION", (("Start", "index.html"), ("Guide", "docs/guide.html"), ("API", "api/index.html"))
  )
  files = {
    "project.py": 'BUILD_VERSION: str = "1.0.0"\n',
    "docs/Doxyfile": "INPUT = src\n",
    "docs/guide.md": '# Guide\n\n<a id="units"></a>\n## Units\n\nUTC, UT1 and TT; pre-1972 approximation.\n\n'
    '[Python](../bindings/python/README.md#call)\n\n```python\nprint(".md#units")\n```\n',
    "bindings/python/README.md": '# Python\n\n<a id="call"></a>\n## Call\n\n[Guide](../../docs/guide.md#units)\n',
    "docs/site/index.md": "# Documentation\n\n[Guide](../guide.md)\n",
    "src/example.hpp": "// source only\n",
    "LICENSE": "project terms\n",
    "THIRD_PARTY_NOTICES.txt": "retained terms\n",
    "docs/site/page.html": (ROOT / "docs/site/page.html").read_text(),
    "docs/site/site.css": (ROOT / "docs/site/site.css").read_text(),
  }
  for name, text in files.items():
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")

  def git(directory, *args):
    if args == ("rev-parse", "--show-toplevel"):
      return str(root).encode()
    if args == ("rev-parse", "HEAD"):
      return SHA.encode()
    if args[0] == "status":
      return b""
    if args == ("ls-files", "-z"):
      return ("\0".join(files) + "\0").encode()
    if args[0] == "show":
      return files[args[1].split(":", 1)[1]].encode()
    assert args[:3] == ("ls-files", "--error-unmatch", "--")
    return args[-1].encode()

  monkeypatch.setattr(site, "git", git)
  generators = {"doxygen": "1.15.0", "markdown-it-py": "4.2.0"}
  monkeypatch.setattr(site, "generator_versions", lambda: generators)
  label = f"{VERSION} · {site.STATE} @ {SHA}"
  for name in (*site.API_ORIGINS, "symbol.html", "doxygen_crawl.html"):
    link = '<a href="guide.md#units">Guide</a>' if name == "index.html" else '<a href="symbol.html#symbol">Symbol</a>'
    text = f'<html><head><title>API</title></head><body><div>{label}</div><div id="symbol"></div>{link}</body></html>'
    (api / name).write_text(text)
  for name in site.TERMS:
    shutil.copyfile(root / name, api / name)
  return root, api, output, generators, files


def assemble(sources):
  root, api, output, generators, _ = sources
  site.assemble(root, output, api, site.source_snapshot(root), VERSION, generators)
  assert site.validate_site(root, output, SHA)["html_pages"] == 7
  return root, api, output


def reseal(output):
  path = output / "site-manifest.json"
  manifest = json.loads(path.read_text())
  manifest["outputs"] = {name: value for name, value in site.members(output).items() if name != path.name}
  path.write_text(json.dumps(manifest))


def test_site_is_opt_in_and_uses_actual_version(monkeypatch):
  for arguments, expected in ((["--all"], False), (["--site"], True), (["--docs"], False)):
    monkeypatch.setattr(sys, "argv", ["project.py", *arguments])
    args = project.parse_args()
    tasks = project.create_tasks(args)
    assert args.site is expected
    assert ("Generate documentation site" in [task.name for task in tasks]) is expected
    if expected:
      versions = []
      monkeypatch.setattr(project, "build_site", lambda value, versions=versions: versions.append(value) or 0)
      assert tasks[0].func() == 0 and versions == [project.BUILD_VERSION]


def test_shared_nonflattened_parse_preserves_source_parser():
  text = "# Title\n\n[link](https://example.org)\n"
  parsed = markdown_source.block_tokens(text)
  assert markdown_source.tokens(text) == [part for token in parsed for part in (token, *(token.children or ()))]
  validator = markdown_source.MARKDOWN.validateLink
  markdown_source.block_tokens(text, validate_link=lambda _: True)
  assert markdown_source.MARKDOWN.validateLink == validator


def test_template_inserts_source_content_without_reinterpreting_it(sources):
  text = site.page_html(sources[0], "index.html", SHA, VERSION, "docs/guide.md", "Title", "<code>{{toc}}</code>", "TOC")
  assert "<code>{{toc}}</code>" in text


@pytest.mark.parametrize("fault", ["sha", "root", "dirty", "guide-byte", "template-byte", "symlink"])
def test_source_identity_rejects_uncommitted_bytes_and_wrong_identity(sources, monkeypatch, fault):
  root, _, _, _, _ = sources
  assert site.source_snapshot(root, SHA)["source_sha"] == SHA
  sha, directory = SHA, root
  if fault == "sha":
    sha = "2" * 40
  elif fault == "root":
    directory = root / "docs"
  elif fault == "dirty":
    original = site.git
    monkeypatch.setattr(site, "git", lambda root, *args: b" M source" if args[0] == "status" else original(root, *args))
  else:
    path = root / ("docs/site/page.html" if fault == "template-byte" else "docs/guide.md")
    if fault == "symlink":
      path.unlink()
      path.symlink_to(root / "LICENSE")
    else:
      path.write_bytes(path.read_bytes() + b" ")
  with pytest.raises(ValueError):
    site.source_snapshot(directory, sha)


def test_complete_site_identity_terms_content_and_determinism(sources):
  root, api, output = assemble(sources)
  before = {path.relative_to(output): path.read_bytes() for path in output.rglob("*") if path.is_file()}
  assert "timestamp" not in json.loads((output / "site-manifest.json").read_text())
  html = (output / "docs/guide.html").read_text()
  assert "print(&quot;.md#units&quot;)" in html
  assert 'href="../bindings/python/README.html#call"' in html
  assert "UTC, UT1 and TT; pre-1972 approximation." in html
  assert "guide.md#units" not in (output / "api/index.html").read_text()
  shutil.rmtree(output)
  output.mkdir()
  site.assemble(root, output, api, site.source_snapshot(root), VERSION, sources[3])
  assert before == {path.relative_to(output): path.read_bytes() for path in output.rglob("*") if path.is_file()}
  report = site.validate_site(root, output, SHA)
  assert report["local_destinations"]["/"] == report["local_destinations"]["/celestial-calendar/"] > 10


@pytest.mark.parametrize(
  "fault",
  [
    "source",
    "state",
    "version",
    "api-marker",
    "page",
    "asset",
    "extra",
    "symlink",
    "hardlink",
    "license",
    "notice",
    "timestamp",
    "content",
  ],
)
def test_real_validator_rejects_output_defects_from_valid_baseline(sources, fault):
  root, _, output = assemble(sources)
  page = output / ("api/symbol.html" if fault == "api-marker" else "docs/guide.html")
  text = page.read_text()
  if fault in ("source", "state", "version", "api-marker", "content"):
    old, new = {
      "source": (SHA, "2" * 40),
      "state": (site.STATE, "stable"),
      "version": (VERSION, "9.9.9"),
      "api-marker": (f"{VERSION} · {site.STATE} @ {SHA}", VERSION),
      "content": ("UTC, UT1 and TT; pre-1972 approximation.", "UTC"),
    }[fault]
    page.write_text(text.replace(old, new))
    reseal(output)
  elif fault in ("page", "asset", "license", "notice"):
    (
      output
      / {"page": "docs/guide.html", "asset": "site.css", "license": "LICENSE", "notice": "api/THIRD_PARTY_NOTICES.txt"}[
        fault
      ]
    ).unlink()
  elif fault == "extra":
    (output / "stale.html").write_text("stale")
    reseal(output)
  elif fault == "symlink":
    (output / "linked").symlink_to(root / "LICENSE")
  elif fault == "hardlink":
    os.link(output / "LICENSE", output / "linked")
  else:
    page.write_text(text + "<!-- generated today -->")
  with pytest.raises(ValueError):
    site.validate_site(root, output, SHA)


@pytest.mark.parametrize(
  "target", ["#absent", "guide.html#absent", "/docs/guide.html", "../../escape.html", "javascript:alert(1)"]
)
def test_complete_crawl_checks_fragments_schemes_and_both_prefixes(sources, target):
  _, _, output = assemble(sources)
  page = output / "docs/guide.html"
  page.write_text(page.read_text().replace("</article>", f'<a href="{target}">broken</a></article>'))
  with pytest.raises(ValueError):
    site.crawl(output)


def test_explicit_inline_block_unicode_and_repeated_heading_ids(sources):
  root = sources[0]
  (root / "docs/guide.md").write_text(
    '<a id="unrelated"></a>\n\n# A!\n\n<a id="a"></a>\n\n## A!\n\n'
    '## Added\n\n## Added\n\n<a id="added-1"></a>\n\n## Added\n\n## 中文！\n'
  )
  _, body, toc = site.render_markdown(root, "docs/guide.md", SHA)
  parsed = site.HtmlPage(body)
  assert parsed.ids == {"unrelated", "a", "a-1", "added", "added-1", "added-2", "added-3", "中文"}
  assert toc.count('href="#a"') == 1
  assert "added-2" in toc and "added-3" in toc


@pytest.mark.parametrize(
  "text",
  [
    "<script>alert(1)</script>",
    '<img src="x">',
    '<a id="x">not empty</a>',
    '<a id="x"></a>\n\n<a id="x"></a>',
    "[bad](javascript:alert)",
    "![bad](data:text/html,test)",
    '<a href="javascript:alert(1)">bad</a>',
  ],
)
def test_unsupported_raw_html_and_unsafe_source_fail(sources, text):
  root = sources[0]
  (root / "docs/guide.md").write_text(text)
  with pytest.raises(ValueError):
    site.render_markdown(root, "docs/guide.md", SHA)


def test_empty_block_anchors_and_comments_are_supported(sources):
  root = sources[0]
  (root / "docs/guide.md").write_text('<!-- comment -->\n\n<a id="block">\n</a>\n\n# Title\n\n<a id="inline"></a>\n')
  _, body, _ = site.render_markdown(root, "docs/guide.md", SHA)
  assert {"block", "inline", "title"} == site.HtmlPage(body).ids


def test_html_crawl_rejects_duplicate_fragments(sources):
  _, _, output = assemble(sources)
  page = output / "docs/guide.html"
  page.write_text(page.read_text().replace("</article>", '<a id="units"></a></article>'))
  with pytest.raises(ValueError, match="duplicate HTML fragment"):
    site.crawl(output)


@pytest.mark.parametrize("fault", ["timestamp-field", "deleted-source", "empty-directory"])
def test_manifest_and_page_context_are_required(sources, fault):
  root, _, output = assemble(sources)
  if fault == "timestamp-field":
    path = output / "site-manifest.json"
    manifest = json.loads(path.read_text())
    manifest["generated_at"] = "today"
    path.write_text(json.dumps(manifest))
  elif fault == "deleted-source":
    path = output / "api/symbol.html"
    path.write_text(path.read_text().replace(f'data-source-sha="{SHA}"', ""))
    reseal(output)
  else:
    (output / "stale-directory").mkdir()
  with pytest.raises(ValueError):
    site.validate_site(root, output, SHA)


def test_builder_removes_only_owned_stale_site_and_keeps_legacy_output(sources, monkeypatch):
  root, api, _, _, _ = sources
  monkeypatch.setattr(site.paths, "proj_root", lambda: root)
  output = root / "build/docs-site"
  output.mkdir(parents=True)
  (output / "stale.txt").write_text("stale")
  legacy = root / "build/api-docs/html/index.html"
  legacy.parent.mkdir(parents=True)
  legacy.write_text("legacy numeric version")

  def generate(label, *, output):
    assert label == f"{VERSION} · {site.STATE} @ {SHA}"
    shutil.copytree(api, output / "html")
    return 0

  monkeypatch.setattr(site, "build_docs", generate)
  assert site.build_site(VERSION) == 0
  assert not (output / "stale.txt").exists()
  assert legacy.read_text() == "legacy numeric version"


def test_source_identity_wins_over_relative_and_absolute_url_spelling(sources):
  root = sources[0]
  for relative, absolute, expected in (
    ("guide.md#units", site.REPOSITORY + "/blob/main/docs/guide.md#units", "../docs/guide.html#units"),
    (
      "../src/example.hpp",
      site.REPOSITORY + "/blob/main/src/example.hpp",
      site.REPOSITORY + f"/blob/{SHA}/src/example.hpp",
    ),
  ):
    assert site.source_destination(root, "docs/API.md", relative, "api/index.html", SHA) == expected
    assert site.source_destination(root, "docs/API.md", absolute, "api/index.html", SHA) == expected
  with pytest.raises(ValueError):
    site.source_destination(root, "docs/API.md", "../../outside", "api/index.html", SHA)
  with pytest.raises(ValueError):
    site.rewrite_api(root, "unknown.html", '<a href="guide.md">Guide</a>', SHA, VERSION)
  raw = '<script>const example = "guide.md";</script><code>guide.md</code><a href="symbol.html#symbol">X</a>'
  assert site.rewrite_api(root, "symbol.html", raw, SHA, VERSION) == raw


@pytest.mark.parametrize("fault", ["missing-origin", "symlink", "stale-source"])
def test_api_copy_rejects_invalid_input_trees(sources, fault):
  root, api, output, generators, _ = sources
  assemble(sources)
  shutil.rmtree(output)
  output.mkdir()
  if fault == "missing-origin":
    (api / "index.html").unlink()
  elif fault == "symlink":
    (api / "linked").symlink_to(api / "index.html")
  else:
    (api / "bad_source.html").write_text("source")
  with pytest.raises(ValueError):
    site.assemble(root, output, api, site.source_snapshot(root), VERSION, generators)


def test_site_doxygen_override_keeps_root_relative_inputs_and_legacy_artifact(sources, monkeypatch):
  root = sources[0]
  monkeypatch.setattr(build.paths, "proj_root", lambda: root)
  monkeypatch.setattr(build, "check_tool", lambda *args, **kwargs: True)
  legacy = root / "build/api-docs/html/index.html"
  legacy.parent.mkdir(parents=True)
  legacy.write_text("legacy")
  output = root / "build/site-api-docs"

  def run(command, **kwargs):
    assert command == ["doxygen", str(output / "Doxyfile")]
    assert kwargs["cwd"] == root
    assert kwargs["env"]["BUILD_VERSION"] == "development label"
    config = (output / "Doxyfile").read_text()
    assert config.startswith((root / "docs/Doxyfile").read_text())
    assert config.endswith(f'OUTPUT_DIRECTORY = "{output.as_posix()}"\n')
    return ProcReturn(2, "", "deliberate stop after invocation check")

  monkeypatch.setattr(build, "run_cmd", run)
  assert build.build_docs("development label", output=output) == 2
  assert legacy.read_text() == "legacy"
