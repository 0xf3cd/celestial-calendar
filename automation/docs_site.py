# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Build and verify the development documentation from a clean, exact source revision."""

import ast
import hashlib
import json
import posixpath
import re
import shutil
import subprocess
from fnmatch import fnmatchcase
from html import escape
from html.parser import HTMLParser
from importlib.metadata import version as package_version
from pathlib import Path
from urllib.parse import quote, unquote, urljoin, urlsplit, urlunsplit

from . import paths
from .build import build_docs
from .utils import green_print, red_print


REPOSITORY = "https://github.com/0xf3cd/celestial-calendar"
STATE = "unreleased development"
GUIDES = (
  "README.md",
  "README_ZH.md",
  "docs/ALGORITHMS.md",
  "docs/API.md",
  "docs/BUILDING.md",
  "docs/CHANGELOG.md",
  "docs/DESIGN.md",
  "docs/EPHEMERIS_EVENTS.md",
  "docs/MAINTAINING.md",
  "docs/MIGRATING_1_0.md",
  "docs/RELEASE_NOTES.md",
  "docs/RELEASING.md",
  "docs/UTC_CONVERSIONS.md",
  "docs/VALIDATION.md",
  "bindings/javascript/README.md",
  "bindings/javascript/README_ZH.md",
  "bindings/javascript-alias/README.md",
  "bindings/python/README.md",
  "bindings/python/README_ZH.md",
  "docs/examples/chart_snapshot/README.md",
)
API_ORIGINS = {"index.html": "docs/API.md", "md_docs_2EPHEMERIS__EVENTS.html": "docs/EPHEMERIS_EVENTS.md"}
TERMS = ("LICENSE", "THIRD_PARTY_NOTICES.txt")
NAVIGATION = (
  ("Start", "index.html"),
  ("Python", "bindings/python/README.html"),
  ("JavaScript", "bindings/javascript/README.html"),
  ("C++", "README.html"),
  ("UTC conversions", "docs/UTC_CONVERSIONS.html"),
  ("Events", "docs/EPHEMERIS_EVENTS.html"),
  ("Models", "docs/ALGORITHMS.html"),
  ("Design", "docs/DESIGN.html"),
  ("Validation", "docs/VALIDATION.html"),
  ("API reference", "api/index.html"),
)


def digest(data: bytes) -> str:
  return hashlib.sha256(data).hexdigest()


def git(root: Path, *args: str) -> bytes:
  return subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True).stdout


def doxygen_inputs(root: Path, tracked: list[str]) -> set[str]:
  """Select files from supported root-relative Doxyfile inputs, conservatively ignoring exclusions."""
  selection = {}
  keys = {"INPUT", "FILE_PATTERNS", "RECURSIVE"}
  for line in (root / "docs/Doxyfile").read_text(encoding="utf-8").splitlines():
    text = line.strip()
    if text.startswith("@INCLUDE"):
      raise ValueError("site snapshot does not support Doxyfile includes")
    key = re.match(r"[A-Z_]+", text)
    if key is None or key[0] not in keys | {"INPUT_FILTER", "FILTER_PATTERNS"}:
      continue
    assignment = re.fullmatch(r"([A-Z_]+)\s*=\s*([^\\$]*)", text)
    if assignment is None or key[0] in selection:
      raise ValueError(f"unsupported Doxyfile selection: {key[0]}")
    values = assignment[2].split()
    if key[0] not in keys:
      if values:
        raise ValueError(f"site snapshot does not support Doxyfile {key[0]}")
      continue
    selection[key[0]] = values
  if set(selection) != keys or selection["RECURSIVE"] not in (["YES"], ["NO"]):
    raise ValueError("Doxyfile requires explicit INPUT, FILE_PATTERNS and RECURSIVE=YES/NO")
  patterns = selection["FILE_PATTERNS"]
  if not selection["INPUT"] or not patterns or any("/" in value or '"' in value for value in patterns):
    raise ValueError("unsupported Doxyfile INPUT or FILE_PATTERNS")
  selected = set()
  for name in selection["INPUT"]:
    if not re.fullmatch(r"[\w./-]+", name) or Path(name).is_absolute() or ".." in Path(name).parts:
      raise ValueError(f"unsupported Doxyfile INPUT: {name}")
    path = root / name
    if not path.exists() or path.is_symlink():
      raise ValueError(f"Doxyfile INPUT is missing or linked: {name}")
    recursive = selection["RECURSIVE"] == ["YES"]
    candidates = set(path.rglob("*") if recursive else path.glob("*")) if path.is_dir() else {path}
    if path.is_dir():
      candidates |= {
        root / file
        for file in tracked
        if (root / file).is_relative_to(path) and (recursive or (root / file).parent == path)
      }
    for candidate in candidates:
      if candidate.is_symlink():
        raise ValueError(f"Doxyfile input contains a linked path: {candidate.relative_to(root)}")
      if not candidate.is_dir() and (
        path.is_file() or any(fnmatchcase(candidate.name, pattern) for pattern in patterns)
      ):
        selected.add(candidate.relative_to(root).as_posix())
  return selected


def source_snapshot(root: Path, sha: str | None = None) -> dict:
  """Require the repository root and committed bytes, including documentation build inputs."""
  if Path(git(root, "rev-parse", "--show-toplevel").decode().strip()).resolve() != root.resolve():
    raise ValueError("site source must be the exact repository root")
  head = git(root, "rev-parse", "HEAD").decode().strip()
  if not re.fullmatch(r"[0-9a-f]{40}", head) or (sha is not None and sha != head):
    raise ValueError("site source SHA does not match HEAD")
  if git(root, "status", "--porcelain", "--untracked-files=all"):
    raise ValueError("site requires a clean tracked/index/untracked source tree")
  tracked = git(root, "ls-files", "-z").decode().split("\0")[:-1]
  selected = sorted(
    set(GUIDES)
    | set(TERMS)
    | doxygen_inputs(root, tracked)
    | {
      name
      for name in tracked
      if name in ("project.py", "Requirements.txt", "docs/Doxyfile")
      or name.startswith(("automation/", "docs/site/", ".github/workflows/"))
      or (
        name.startswith(("src/astro/", "src/calendar/", "src/util/", "src/shared_lib/"))
        and name.endswith((".hpp", ".h"))
      )
    }
  )
  inputs = {}
  for name in selected:
    if name not in tracked:
      raise ValueError(f"site input is not committed: {name}")
    path = root / name
    if path.is_symlink() or not path.is_file() or path.stat().st_nlink != 1:
      raise ValueError(f"site input is not an ordinary single-link file: {name}")
    data = path.read_bytes()
    if data != git(root, "show", f"{head}:{name}"):
      raise ValueError(f"site input differs from source SHA: {name}")
    inputs[name] = digest(data)
  return {"source_sha": head, "inputs": inputs}


def project_version(root: Path) -> str:
  tree = ast.parse((root / "project.py").read_text(encoding="utf-8"))
  return next(
    ast.literal_eval(node.value)
    for node in tree.body
    if isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name) and node.target.id == "BUILD_VERSION"
  )


def relative_url(page: str, target: str) -> str:
  return quote(posixpath.relpath(target, posixpath.dirname(page) or "."), safe="/._-")


def repository_source(target: str) -> str | None:
  """The source named by a live repository URL; GitHub service/UI routes remain external."""
  url, repository = urlsplit(target), urlsplit(REPOSITORY)
  if (url.scheme, url.netloc) != (repository.scheme, repository.netloc):
    return None
  route = url.path.removeprefix(repository.path)
  if route == url.path:
    return None
  if route in ("", "/", "/tree/main", "/tree/main/", "/blob/main"):
    return "README.md"
  if route.startswith(("/blob/main/", "/tree/main/")):
    path = unquote(route.split("/", 3)[3])
    if not path:
      raise ValueError(f"repository source URL has no file: {target}")
    return path
  return None


def source_destination(root: Path, origin: str, target: str, page: str, sha: str) -> str:
  """Resolve repository identity before choosing a hosted route or a pinned source link."""
  url = urlsplit(target)
  if url.scheme or url.netloc:
    if url.scheme not in ("http", "https", "mailto") or (url.netloc and not url.scheme):
      raise ValueError(f"unsafe source scheme: {origin}: {target}")
    path = repository_source(target)
    if path is None:
      return target
  else:
    if not url.path:
      return target
    decoded = unquote(url.path)
    if decoded.startswith("/") or "\\" in decoded or "\0" in decoded:
      raise ValueError(f"invalid source path: {origin}: {target}")
    path = posixpath.normpath(posixpath.join(posixpath.dirname(origin), decoded))
  if path == ".." or path.startswith(("../", "/")) or "\\" in path or "\0" in path:
    raise ValueError(f"source path escapes repository: {origin}: {target}")
  if not (root / path).exists() or not git(root, "ls-files", "--error-unmatch", "--", path):
    raise ValueError(f"missing source destination: {origin}: {target}")
  if path in GUIDES:
    result = relative_url(page, Path(path).with_suffix(".html").as_posix())
  else:
    result = f"{REPOSITORY}/{'tree' if (root / path).is_dir() else 'blob'}/{sha}/{quote(path)}"
  return urlunsplit((*urlsplit(result)[:3], url.query, url.fragment))


class HtmlPage(HTMLParser):
  """Inspect real HTML destinations and IDs without interpreting scripts as markup."""

  def __init__(self, text: str):
    super().__init__(convert_charrefs=False)
    self.ids, self.targets, self.tags = set(), [], []
    self.feed(text)

  def handle_starttag(self, tag, attrs):
    attributes = dict(attrs)
    if identifier := attributes.get("id"):
      if identifier in self.ids:
        raise ValueError(f"duplicate HTML fragment: {identifier}")
      self.ids.add(identifier)
    for key in ("href", "src"):
      if attributes.get(key):
        self.targets.append(attributes[key])
    self.tags.append((tag, attributes, self.getpos(), self.get_starttag_text()))

  handle_startendtag = handle_starttag


def raw_anchors(parsed) -> set[str]:
  from .markdown_source import ANCHOR, COMMENTS

  anchors = set()
  opened = False
  for token in parsed:
    for part in (token, *(token.children or ())):
      if part.type not in ("html_inline", "html_block"):
        if opened and part.type != "inline":
          raise ValueError("raw source anchors must be empty")
        continue
      text = part.content
      # Raw HTML is a closed set: comments and empty <a id="..."> anchors.
      uncommented = COMMENTS.sub("", text)
      rest = ANCHOR.sub("", uncommented)
      rest = rest.replace("</a>", "")
      if rest.strip():
        raise ValueError(f"unsupported raw source HTML: {text!r}")
      for anchor in re.finditer(f"{ANCHOR.pattern}|</a>", uncommented):
        if (anchor[0] == "</a>") != opened:
          raise ValueError("raw source anchors must be paired and empty")
        opened = not opened
      for identifier in ANCHOR.findall(uncommented):
        if identifier in anchors:
          raise ValueError(f"duplicate explicit fragment: {identifier}")
        anchors.add(identifier)
  if opened:
    raise ValueError("unclosed raw source anchor")
  return anchors


def render_markdown(root: Path, source: str, sha: str, *, page: str | None = None) -> tuple[str, str, str]:
  from . import markdown_source

  def validate_link(target):
    url = urlsplit(target)
    if url.scheme and url.scheme not in ("https", "http", "mailto"):
      raise ValueError(f"unsafe source scheme: {source}: {target}")
    return markdown_source.MARKDOWN.validateLink(target)

  parsed = markdown_source.block_tokens((root / source).read_text(encoding="utf-8"), validate_link=validate_link)
  reserved = raw_anchors(parsed)
  used, headings, title = set(reserved), [], source
  explicit_used = set()
  page = page or Path(source).with_suffix(".html").as_posix()
  for index, token in enumerate(parsed):
    if token.type == "heading_open":
      label = parsed[index + 1].content
      slug = re.sub(r"[^\w\s-]", "", label.lower()).strip().replace(" ", "-") or "section"
      identifier = slug
      if slug in reserved and slug not in explicit_used:
        explicit_used.add(slug)
      else:
        suffix = 0
        while identifier in used:
          suffix += 1
          identifier = f"{slug}-{suffix}"
        token.attrSet("id", identifier)
        used.add(identifier)
      headings.append(f'<a href="#{escape(identifier, quote=True)}">{escape(label)}</a>')
      if len(headings) == 1:
        title = label
    for part in (token, *(token.children or ())):
      if part.type in ("link_open", "image"):
        key = "href" if part.type == "link_open" else "src"
        part.attrSet(key, source_destination(root, source, part.attrs[key], page, sha))
  body = markdown_source.MARKDOWN.renderer.render(parsed, markdown_source.MARKDOWN.options, {})
  return title, body, "\n".join(headings)


def context_html(page: str, sha: str, version: str, source: str) -> str:
  return (
    f'<aside class="source-context" data-source-sha="{sha}" data-version="{version}" data-state="{STATE}">'
    f"<strong>Development / unreleased</strong> · package version {escape(version)}"
    f'<br><a href="{REPOSITORY}/blob/{sha}/{quote(source)}">Source {sha}</a>'
    f' · <a href="{REPOSITORY}/releases">Published releases</a>'
    f' · <a href="{relative_url(page, "index.html")}">Documentation</a></aside>'
  )


def page_html(root: Path, page: str, sha: str, version: str, source: str, title: str, body: str, toc: str) -> str:
  template = (root / "docs/site/page.html").read_text(encoding="utf-8")
  navigation = "\n".join(f'<a href="{relative_url(page, target)}">{escape(label)}</a>' for label, target in NAVIGATION)
  values = {
    "title": escape(title),
    "style": relative_url(page, "site.css"),
    "navigation": navigation,
    "context": context_html(page, sha, version, source),
    "body": body,
    "toc": toc,
    "license": relative_url(page, "LICENSE"),
    "notices": relative_url(page, "THIRD_PARTY_NOTICES.txt"),
  }
  return re.sub(r"\{\{(\w+)\}\}", lambda match: values[match[1]], template)


def rewrite_api(root: Path, name: str, text: str, sha: str, version: str) -> str:
  parsed = HtmlPage(text)
  offsets, offset = [], 0
  for line in text.splitlines(keepends=True):
    offsets.append(offset)
    offset += len(line)
  replacements = []
  for tag, attrs, (line, column), raw in parsed.tags:
    changed = raw
    for key in ("href", "src"):
      target = attrs.get(key, "")
      url = urlsplit(target)
      source_style = bool(
        url.path
        and not url.scheme
        and not url.netloc
        and not url.path.endswith((".html", ".js", ".css", ".png", ".svg", ".map"))
        and Path(url.path).name not in TERMS
      )
      if source_style or repository_source(target) is not None:
        origin = API_ORIGINS.get(name)
        if source_style and origin is None:
          raise ValueError(f"unknown API source-link origin: {name}: {target}")
        destination = source_destination(root, origin or "docs/API.md", target, "api/" + name, sha)
        pattern = rf"(\b{key}\s*=\s*)([\"\'])(.*?)\2"
        changed = re.sub(
          pattern,
          lambda match, destination=destination: match[1] + match[2] + escape(destination, quote=True) + match[2],
          changed,
          count=1,
          flags=re.DOTALL,
        )
    if tag == "head":
      changed += '<link rel="stylesheet" href="' + relative_url("api/" + name, "site.css") + '">'
    if tag == "body":
      changed += context_html("api/" + name, sha, version, API_ORIGINS.get(name, "docs/API.md"))
    if changed != raw:
      start = offsets[line - 1] + column
      replacements.append((start, start + len(raw), changed))
  for start, end, replacement in reversed(replacements):
    text = text[:start] + replacement + text[end:]
  return text


def members(directory: Path) -> dict[str, str]:
  if directory.is_symlink():
    raise ValueError("site output root is a symlink")
  result, directories = {}, set()
  for path in sorted(directory.rglob("*")):
    name = path.relative_to(directory).as_posix()
    if path.is_symlink() or (path.is_file() and path.stat().st_nlink != 1):
      raise ValueError(f"site output contains a symlink or hardlink: {name}")
    if path.is_dir():
      directories.add(name)
      continue
    if not path.is_file() or path.stat().st_size == 0:
      raise ValueError(f"site output is not a nonempty regular file: {name}")
    result[name] = digest(path.read_bytes())
  expected_directories = {parent.as_posix() for name in result for parent in Path(name).parents if parent != Path(".")}
  if directories != expected_directories:
    raise ValueError("site output contains an extra empty directory")
  return result


def crawl(output: Path) -> dict:
  inventory = members(output)
  pages = {name: HtmlPage((output / name).read_text(encoding="utf-8")) for name in inventory if name.endswith(".html")}
  counts = {}
  for prefix in ("/", "/celestial-calendar/"):
    count = 0
    for name, parsed in pages.items():
      base = "https://docs.invalid" + prefix + name
      for target in parsed.targets:
        url = urlsplit(target)
        if url.scheme or url.netloc:
          if url.scheme not in ("https", "http", "mailto") or (url.netloc and not url.scheme):
            raise ValueError(f"unsafe output URL: {name}: {target}")
          if repository_source(target) is not None:
            raise ValueError(f"unpinned source URL: {name}: {target}")
          continue
        decoded = unquote(url.path)
        if decoded.startswith("/") or "\\" in decoded:
          raise ValueError(f"nonrelative output URL: {name}: {target}")
        resolved = urlsplit(urljoin(base, target))
        path = unquote(resolved.path)
        if not path.startswith(prefix):
          raise ValueError(f"output URL escapes deployment prefix: {name}: {target}")
        member = path[len(prefix) :]
        if member not in inventory:
          raise ValueError(f"missing output destination: {name}: {target}")
        if resolved.fragment and (member not in pages or unquote(resolved.fragment) not in pages[member].ids):
          raise ValueError(f"missing output fragment: {name}: {target}")
        count += 1
    counts[prefix] = count
  return {"html_pages": len(pages), "local_destinations": counts}


def assemble(root: Path, output: Path, api: Path, snapshot: dict, version: str, generators: dict) -> None:
  sha = snapshot["source_sha"]
  api_members = members(api)
  for name in API_ORIGINS:
    if name not in api_members:
      raise ValueError(f"required API source-origin page missing: {name}")
  for name in api_members:
    if name.endswith("_source.html"):
      raise ValueError("site API must not include source listings")
    target = output / "api" / name
    target.parent.mkdir(parents=True, exist_ok=True)
    if name.endswith(".html"):
      target.write_text(
        rewrite_api(root, name, (api / name).read_text(encoding="utf-8"), sha, version), encoding="utf-8"
      )
    else:
      shutil.copyfile(api / name, target)
  for source in GUIDES:
    page = Path(source).with_suffix(".html").as_posix()
    title, body, toc = render_markdown(root, source, sha)
    target = output / page
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(page_html(root, page, sha, version, source, title, body, toc), encoding="utf-8")
  title, body, toc = render_markdown(root, "docs/site/index.md", sha, page="index.html")
  (output / "index.html").write_text(
    page_html(root, "index.html", sha, version, "docs/site/index.md", title, body, toc), encoding="utf-8"
  )
  for name in TERMS:
    shutil.copyfile(root / name, output / name)
  shutil.copyfile(root / "docs/site/site.css", output / "site.css")
  manifest = {
    "schema": 1,
    **snapshot,
    "version": version,
    "state": STATE,
    "generators": generators,
    "api_members": sorted(api_members),
    "outputs": members(output),
  }
  (output / "site-manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def validate_site(root: Path, output: Path, sha: str | None = None) -> dict:
  snapshot = source_snapshot(root, sha)
  manifest = json.loads((output / "site-manifest.json").read_text(encoding="utf-8"))
  if set(manifest) != {"schema", "source_sha", "inputs", "version", "state", "generators", "api_members", "outputs"}:
    raise ValueError("site manifest has missing or unexpected fields")
  version = project_version(root)
  if (
    manifest.get("schema"),
    manifest.get("source_sha"),
    manifest.get("inputs"),
    manifest.get("version"),
    manifest.get("state"),
  ) != (1, snapshot["source_sha"], snapshot["inputs"], version, STATE):
    raise ValueError("site manifest source/version/state/input mismatch")
  if manifest.get("generators") != generator_versions():
    raise ValueError("site generator version mismatch")
  actual = members(output)
  actual.pop("site-manifest.json")
  expected_names = {
    "index.html",
    "site.css",
    *TERMS,
    *(Path(source).with_suffix(".html").as_posix() for source in GUIDES),
    *("api/" + name for name in manifest["api_members"]),
  }
  if set(actual) != expected_names or actual != manifest.get("outputs"):
    raise ValueError("site output member/byte inventory mismatch")
  sha = snapshot["source_sha"]
  for name in TERMS:
    for target in (output / name, output / "api" / name):
      if target.read_bytes() != (root / name).read_bytes():
        raise ValueError(f"site attribution differs: {name}")
  for name in actual:
    if not name.endswith(".html"):
      continue
    text = (output / name).read_text(encoding="utf-8")
    expected = {"data-source-sha": sha, "data-version": version, "data-state": STATE}
    if not any(
      all(attrs.get(key) == value for key, value in expected.items()) for _, attrs, _, _ in HtmlPage(text).tags
    ):
      raise ValueError(f"site page source/version/state missing: {name}")
    if name.startswith("api/") and name != "api/doxygen_crawl.html":
      if f"{version} · {STATE} @ {sha}" not in text:
        raise ValueError(f"site API project identity missing: {name}")
  for source in (*GUIDES, "docs/site/index.md"):
    page = "index.html" if source == "docs/site/index.md" else Path(source).with_suffix(".html").as_posix()
    title, body, toc = render_markdown(root, source, sha, page=page)
    if (output / page).read_text(encoding="utf-8") != page_html(root, page, sha, version, source, title, body, toc):
      raise ValueError(f"site guide differs from source content: {source}")
  return crawl(output)


def generator_versions() -> dict:
  return {
    "doxygen": subprocess.run(["doxygen", "--version"], check=True, capture_output=True, text=True).stdout.strip(),
    "markdown-it-py": package_version("markdown-it-py"),
  }


def build_site(version: str) -> int:
  """Generate and verify a separate site without replacing the legacy API artifact."""
  root = paths.proj_root()
  output, api = paths.build_dir() / "docs-site", paths.build_dir() / "site-api-docs"
  try:
    snapshot = source_snapshot(root)
    if version != project_version(root):
      raise ValueError("requested site version differs from source")
    if output.is_symlink() or api.is_symlink():
      raise ValueError("documentation output roots must not be symlinks")
    if output.exists():
      shutil.rmtree(output)
    output.mkdir(parents=True)
    label = f"{version} · {STATE} @ {snapshot['source_sha']}"
    result = build_docs(label, output=api)
    if result:
      return result
    assemble(root, output, api / "html", snapshot, version, generator_versions())
    result = validate_site(root, output, snapshot["source_sha"])
    green_print(f"# Development site: build/docs-site/index.html ({result})")
    return 0
  except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
    red_print(f"# Documentation site failed: {error}")
    return 1
