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
import subprocess
import sys
from pathlib import Path, PurePosixPath, PureWindowsPath

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
    ".gitattributes": ".gitattributes text eol=lf\n",
    "project.py": 'BUILD_VERSION: str = "1.0.0"\n',
    "docs/Doxyfile": "INPUT = src\nFILE_PATTERNS = *.hpp *.h *.md\nRECURSIVE = YES\n",
    "docs/guide.md": '# Guide\n\n<a id="units"></a>\n## Units\n\nUTC, UT1 and TT; pre-1972 approximation.\n\n'
    "[Python](../bindings/python/README.md#call)\n\n[External](https://example.org/doc#x)\n"
    f"[Mail](mailto:reader@example.org)\n[Releases]({site.REPOSITORY}/releases)\n\n"
    '```python\nprint(".md#units")\n```\n',
    "bindings/python/README.md": '# Python\n\n<a id="call"></a>\n## Call\n\n[Guide](../../docs/guide.md#units)\n',
    "docs/site/index.md": "# Documentation\n\n[Guide](../guide.md)\n",
    "src/example.hpp": "// source only\n",
    "README.md": '# Root\n\n<a id="cpp"></a>\n## C++\n',
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


@pytest.fixture(params=[PurePosixPath, PureWindowsPath], ids=["posix", "windows"])
def page_path_flavour(sources, monkeypatch, request):
  assemble(sources)

  class PagePath(type(Path())):
    # Only suffix-derived page spelling varies; fixture and production filesystem I/O stay native.
    def with_suffix(self, suffix):
      return request.param(super().with_suffix(suffix).as_posix())

  monkeypatch.setattr(site, "Path", PagePath)


def test_resolver_and_api_urls_use_posix_page_paths(sources, page_path_flavour):
  root = sources[0]
  for origin, target, page, expected in (
    ("docs/API.md", "guide.md#units", "docs/API.html", "guide.html#units"),
    ("docs/guide.md", "../bindings/python/README.md#call", "docs/guide.html", "../bindings/python/README.html#call"),
    (
      "bindings/python/README.md",
      "../../docs/guide.md#units",
      "bindings/python/README.html",
      "../../docs/guide.html#units",
    ),
  ):
    assert site.source_destination(root, origin, target, page, SHA) == expected
  api = site.rewrite_api(root, "index.html", '<a href="guide.md#units">Guide</a>', SHA, VERSION)
  assert 'href="../docs/guide.html#units"' in api


def test_rendered_links_use_posix_page_paths(sources, page_path_flavour):
  root = sources[0]
  for source, expected in (
    ("docs/guide.md", "../bindings/python/README.html#call"),
    ("bindings/python/README.md", "../../docs/guide.html#units"),
  ):
    _, body, _ = site.render_markdown(root, source, SHA)
    assert expected in site.HtmlPage(body).targets, body
    assert "%5C" not in body and "\\" not in body


def test_site_inventory_and_rerender_use_posix_page_paths(sources, page_path_flavour):
  root, api, output, generators, _ = sources
  shutil.rmtree(output)
  output.mkdir()
  site.assemble(root, output, api, site.source_snapshot(root), VERSION, generators)
  expected = {
    "index.html",
    "site.css",
    *site.TERMS,
    "docs/guide.html",
    "bindings/python/README.html",
    *("api/" + name for name in site.members(api)),
  }
  actual = site.members(output)
  actual.pop("site-manifest.json")
  assert set(actual) == expected, f"non-POSIX hosted page keys: {set(actual) - expected}"
  assert json.loads((output / "site-manifest.json").read_text())["outputs"].keys() == actual.keys()
  result = site.validate_site(root, output, SHA)
  assert result["html_pages"] == 7
  assert result["local_destinations"]["/"] == result["local_destinations"]["/celestial-calendar/"] > 10


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


@pytest.mark.parametrize("fault", ["sha", "root", "dirty", "guide-byte", "template-byte", "attributes-byte", "symlink"])
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
    name = {"template-byte": "docs/site/page.html", "attributes-byte": ".gitattributes"}.get(fault, "docs/guide.md")
    path = root / name
    if fault == "symlink":
      path.unlink()
      path.symlink_to(root / "LICENSE")
    else:
      path.write_bytes(path.read_bytes() + b" ")
  with pytest.raises(ValueError):
    site.source_snapshot(directory, sha)


def test_site_inputs_keep_committed_bytes_in_autocrlf_checkout(tmp_path):
  sha = site.git(ROOT, "rev-parse", "HEAD").decode().strip()
  policy = tmp_path / "candidate.attributes"
  # Before commit, this external file supplies the candidate checkout policy to an immutable HEAD.
  policy.write_bytes((ROOT / ".gitattributes").read_bytes())
  clone = tmp_path / "repo"
  subprocess.run(["git", "clone", "--shared", "--no-checkout", str(ROOT), str(clone)], check=True, capture_output=True)
  site.git(clone, "config", "core.autocrlf", "true")
  site.git(clone, "config", "core.attributesFile", str(policy))
  site.git(clone, "checkout", "--detach", sha)
  assert site.git(clone, "status", "--porcelain", "--untracked-files=all") == b""
  snapshot = site.source_snapshot(clone, sha)
  assert ".gitattributes" in snapshot["inputs"]
  names = "\0".join(snapshot["inputs"]) + "\0"
  attributes = (
    subprocess.run(
      ["git", "-C", str(clone), "check-attr", "-z", "--stdin", "eol"],
      input=names.encode(),
      check=True,
      capture_output=True,
    )
    .stdout.decode()
    .split("\0")[:-1]
  )
  assert all(value == "lf" for value in attributes[2::3]), dict(zip(attributes[::3], attributes[2::3], strict=True))
  for name, expected in snapshot["inputs"].items():
    data = (clone / name).read_bytes()
    assert data == site.git(clone, "show", f"{sha}:{name}")
    assert site.digest(data) == expected
  binary = b"binary\0line\r\n"
  for name in ("automation/asset.bin", "docs/site/asset.bin"):
    hashes = [
      subprocess.run(
        ["git", "-C", str(clone), "hash-object", option, "--stdin"], input=binary, check=True, capture_output=True
      ).stdout
      for option in ("--no-filters", f"--path={name}")
    ]
    assert hashes[0] == hashes[1], f"binary checkout policy must not convert {name}"


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
  "target", ["#absent", "guide.html#absent", "/docs/guide.html", "../../escape.html", "javascript:alert(1)", "//host/x"]
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
    "[bad](//host/x)",
    '<a id="Upper"></a>',
    '<a id="a_b"></a>',
    '<a id="中文"></a>',
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


@pytest.mark.parametrize(
  "suffix", ["", "/", "?tab=readme#cpp", "/?tab=readme#cpp", "/tree/main", "/tree/main/", "/blob/main"]
)
def test_repository_home_and_root_tree_name_the_hosted_readme(sources, monkeypatch, suffix):
  root = sources[0]
  monkeypatch.setattr(site, "GUIDES", (*site.GUIDES, "README.md"))
  target = site.REPOSITORY + suffix
  result = site.source_destination(root, "docs/guide.md", target, "docs/guide.html", SHA)
  expected = "../README.html" + ("?tab=readme#cpp" if "?" in suffix else "")
  assert result == expected
  assert site.repository_source(target) == "README.md"
  api = site.rewrite_api(root, "symbol.html", f'<a href="{target}">Home</a><code>{target}</code>', SHA, VERSION)
  assert f'<a href="{expected}">Home</a>' in api
  assert f"<code>{target}</code>" in api


@pytest.mark.parametrize(
  "suffix", ["", "/", "#cpp", "/tree/main/", "/blob/main/docs/API.md", "/tree/main/src/", "/blob/main"]
)
def test_crawl_rejects_every_live_source_spelling(sources, suffix):
  _, _, output = assemble(sources)
  page = output / "docs/guide.html"
  page.write_text(page.read_text().replace("</article>", f'<a href="{site.REPOSITORY}{suffix}">live</a></article>'))
  with pytest.raises(ValueError, match="unpinned source URL"):
    site.crawl(output)


def test_trailing_slash_blob_root_still_rejects_an_empty_file(sources):
  root, _, output = assemble(sources)
  target = site.REPOSITORY + "/blob/main/"
  with pytest.raises(ValueError, match="repository source URL has no file"):
    site.source_destination(root, "docs/guide.md", target, "docs/guide.html", SHA)
  with pytest.raises(ValueError, match="repository source URL has no file"):
    site.rewrite_api(root, "symbol.html", f'<a href="{target}">Empty</a>', SHA, VERSION)
  (output / "index.html").write_text(f'<a href="{target}">Empty</a>')
  with pytest.raises(ValueError, match="repository source URL has no file"):
    site.crawl(output)


@pytest.mark.parametrize(
  "target",
  [
    "https://example.org/doc#x",
    "mailto:reader@example.org",
    site.REPOSITORY + "/releases",
    site.REPOSITORY + "/issues/398",
    site.REPOSITORY + "/actions/workflows/core_tests.yml",
    site.REPOSITORY + "/actions/runs/123",
  ],
)
def test_external_and_github_service_links_survive_render_and_crawl(sources, target):
  root, _, output = assemble(sources)
  assert site.source_destination(root, "docs/guide.md", target, "docs/guide.html", SHA) == target
  assert site.repository_source(target) is None
  (root / "docs/guide.md").write_text(f"# Links\n\n[External]({target})\n")
  _, body, _ = site.render_markdown(root, "docs/guide.md", SHA)
  assert f'href="{target}"' in body
  page = output / "docs/guide.html"
  page.write_text(page.read_text().replace("</article>", body + "</article>"))
  assert site.crawl(output)["html_pages"] == 7
  assert (
    site.rewrite_api(root, "symbol.html", f'<a href="{target}">UI</a>', SHA, VERSION) == f'<a href="{target}">UI</a>'
  )


def test_protocol_relative_source_destination_is_rejected(sources):
  with pytest.raises(ValueError, match="unsafe source scheme"):
    site.source_destination(sources[0], "docs/guide.md", "//host/x", "docs/guide.html", SHA)


@pytest.mark.parametrize("fault", ["dirty", "site-link", "api-link", "nonzero", "os-error", "process-error", "version"])
def test_site_build_failures_keep_legacy_api_and_fail_closed(sources, monkeypatch, fault):
  root = sources[0]
  monkeypatch.setattr(site.paths, "proj_root", lambda: root)
  legacy = root / "build/api-docs/html/index.html"
  legacy.parent.mkdir(parents=True)
  legacy.write_text("legacy numeric version")
  calls = []

  def generate(*args, **kwargs):
    calls.append((args, kwargs))
    if fault == "os-error":
      raise FileNotFoundError("missing doxygen executable")
    if fault == "process-error":
      raise subprocess.CalledProcessError(3, ["doxygen", "--version"])
    return 2

  monkeypatch.setattr(site, "build_docs", generate)
  if fault == "dirty":
    original = site.git
    monkeypatch.setattr(site, "git", lambda root, *args: b" M source" if args[0] == "status" else original(root, *args))
  elif fault in ("site-link", "api-link"):
    name = "docs-site" if fault == "site-link" else "site-api-docs"
    (root / "build" / name).symlink_to(legacy.parent, target_is_directory=True)
  assert site.build_site("9.9.9" if fault == "version" else VERSION) == (2 if fault == "nonzero" else 1)
  assert len(calls) == (1 if fault in ("nonzero", "os-error", "process-error") else 0)
  assert legacy.read_text() == "legacy numeric version"


@pytest.mark.parametrize("fault", ["os-error", "process-error"])
def test_site_catches_generator_version_query_failures(sources, monkeypatch, fault):
  root, api, _, _, _ = sources
  monkeypatch.setattr(site.paths, "proj_root", lambda: root)
  legacy = root / "build/api-docs/html/index.html"
  legacy.parent.mkdir(parents=True)
  legacy.write_text("legacy numeric version")

  def generate(*args, output):
    shutil.copytree(api, output / "html")
    return 0

  def query():
    if fault == "os-error":
      raise FileNotFoundError("missing doxygen executable")
    raise subprocess.CalledProcessError(3, ["doxygen", "--version"])

  monkeypatch.setattr(site, "build_docs", generate)
  monkeypatch.setattr(site, "generator_versions", query)
  assert site.build_site(VERSION) == 1
  assert legacy.read_text() == "legacy numeric version"


@pytest.mark.parametrize("fault", ["exit-code", "missing-executable", "process-error"])
def test_site_propagates_actual_doxygen_generation_failures(sources, monkeypatch, fault):
  root = sources[0]
  monkeypatch.setattr(site.paths, "proj_root", lambda: root)
  monkeypatch.setattr(build, "check_tool", lambda *args, **kwargs: True)
  legacy = root / "build/api-docs/html/index.html"
  legacy.parent.mkdir(parents=True)
  legacy.write_text("legacy numeric version")

  def run(command, **kwargs):
    assert command == ["doxygen", str(root / "build/site-api-docs/Doxyfile")]
    assert kwargs["cwd"] == root
    if fault == "missing-executable":
      raise FileNotFoundError("missing doxygen executable")
    if fault == "process-error":
      raise subprocess.CalledProcessError(7, command)
    return ProcReturn(7, "", "generation failed")

  monkeypatch.setattr(build, "run_cmd", run)
  assert site.build_site(VERSION) == (7 if fault == "exit-code" else 1)
  assert legacy.read_text() == "legacy numeric version"


@pytest.mark.parametrize(
  "selection,name",
  [
    ("INPUT = src\nFILE_PATTERNS = *.md\nRECURSIVE = YES\n", "src/astro/notes.md"),
    ("INPUT = extra\nFILE_PATTERNS = *.cpp\nRECURSIVE = NO\n", "extra/example.cpp"),
    ("INPUT = src/example.hpp\nFILE_PATTERNS = *.md\nRECURSIVE = NO\n", "src/example.hpp"),
  ],
)
def test_doxygen_selection_changes_are_in_the_exact_snapshot(sources, selection, name):
  root, _, _, _, files = sources
  files["docs/Doxyfile"] = selection
  (root / "docs/Doxyfile").write_text(selection)
  files[name] = "selected source\n"
  path = root / name
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(files[name])
  assert name in site.source_snapshot(root)["inputs"]
  path.write_bytes(path.read_bytes() + b" ")
  with pytest.raises(ValueError, match="input differs from source SHA"):
    site.source_snapshot(root)


def test_doxygen_nonrecursive_selection_and_missing_or_untracked_markdown(sources):
  root, _, _, _, files = sources
  path = root / "src/nested/notes.md"
  path.parent.mkdir()
  path.write_text("source\n")
  files[path.relative_to(root).as_posix()] = "source\n"
  no_recursion = "INPUT = src\nFILE_PATTERNS = *.md\nRECURSIVE = NO\n"
  (root / "docs/Doxyfile").write_text(no_recursion)
  assert "src/nested/notes.md" not in site.doxygen_inputs(root, list(files))
  (root / "docs/Doxyfile").write_text(files["docs/Doxyfile"])
  path.unlink()
  with pytest.raises(ValueError, match="not an ordinary single-link file"):
    site.source_snapshot(root)
  path.write_text("source\n")
  del files["src/nested/notes.md"]
  with pytest.raises(ValueError, match="site input is not committed"):
    site.source_snapshot(root)


@pytest.mark.parametrize(
  "selection",
  [
    "INPUT += src\n",
    "INPUT = $(SOURCES)\n",
    "INPUT = src/*\n",
    'INPUT = "src"\n',
    "INPUT = ../outside\n",
    "INPUT = absent\n",
    "@INCLUDE = other.conf\n",
    "INPUT_FILTER = generator.py\n",
    "RECURSIVE = MAYBE\n",
    "FILE_PATTERNS = src/*.hpp\n",
  ],
)
def test_unsupported_doxygen_selection_fails_with_context(sources, selection):
  root = sources[0]
  assert site.source_snapshot(root)["source_sha"] == SHA
  config = sources[4]["docs/Doxyfile"]
  key = selection.split()[0]
  if key in ("INPUT", "FILE_PATTERNS", "RECURSIVE"):
    config = "\n".join(selection.strip() if line.startswith(key + " ") else line for line in config.splitlines())
  else:
    config += selection
  (root / "docs/Doxyfile").write_text(config)
  with pytest.raises(ValueError, match="Doxyfile"):
    site.doxygen_inputs(root, list(sources[4]))


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
