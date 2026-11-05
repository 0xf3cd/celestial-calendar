# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import json
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

from automation.markdown_source import tokens
from toolbox import docs_links, package_docs


ROOT = Path(__file__).resolve().parents[2]
SHA = "a" * 40
READMES = (
  "bindings/python/README.md",
  "bindings/javascript/README.md",
  "bindings/javascript-alias/README.md",
  "bindings/python-aliases/README.md.in",
)


@pytest.mark.parametrize("name", READMES)
def test_generated_readme_is_only_context_and_literal_url_changes(name):
  source = (ROOT / name).read_text()
  before, rest = source.split("<!-- package-context -->")
  _, after = rest.split("<!-- /package-context -->")
  expected = (
    before + f"Documentation for package version 1.2.0 at [source {SHA}]({docs_links.REPOSITORY}/tree/{SHA})." + after
  )
  for route in ("blob", "tree"):
    expected = expected.replace(f"{docs_links.REPOSITORY}/{route}/main/", f"{docs_links.REPOSITORY}/{route}/{SHA}/")
  for language in ("python", "javascript"):
    expected = expected.replace(
      f"{docs_links.HOSTED}/bindings/{language}/README.html",
      f"{docs_links.REPOSITORY}/blob/{SHA}/bindings/{language}/README.md",
    )
  actual = package_docs.render_readme(source, "1.2.0", SHA)
  assert actual == expected

  def code(text):
    return [t.content for t in tokens(text) if t.type in ("fence", "code_block", "code_inline")]

  assert code(actual) == code(source)
  assert "/blob/main/" not in actual and "/tree/main/" not in actual
  assert docs_links.HOSTED not in actual


def test_literal_rewrite_preserves_markdown_layout_fragments_and_unrelated_routes():
  prefix = docs_links.REPOSITORY + "/blob/main/"
  text = (
    f"[link][ref]\n\n[ref]: <{prefix}docs/a(b).md?x=1#part> 'Title'\n"
    f"![image]({prefix}docs/a.png)\n"
    f"<a href='{prefix}docs/a.md#part'>link</a>\n"
    f"[Releases]({docs_links.REPOSITORY}/releases) [Elsewhere](https://example.org/a(b).md#part)\n"
  )
  assert docs_links.pin_package_links(text, SHA) == text.replace(prefix, f"{docs_links.REPOSITORY}/blob/{SHA}/")


@pytest.mark.parametrize("sha", ["main", "a" * 39, "A" * 40, "a" * 40 + "\n", "../main"])
def test_invalid_documentation_sha_is_rejected(sha):
  with pytest.raises(ValueError):
    docs_links.pinned_source("README.md", sha)


@pytest.mark.parametrize("path", ["../README.md", "/README.md", "docs/../README.md", "docs\\README.md", "a\0b"])
def test_pinned_source_rejects_escaping_paths(path):
  with pytest.raises(ValueError):
    docs_links.pinned_source(path, SHA)


def test_local_unversioned_build_is_explicitly_developmental(tmp_path, monkeypatch):
  monkeypatch.delenv(package_docs.SOURCE_SHA_ENV, raising=False)
  assert package_docs.source_sha(tmp_path) is None
  text = "# Guide\n\n<!-- package-context -->old<!-- /package-context -->\n"
  readme = package_docs.render_readme(text, "1.2.0", None)
  assert "Local development build" in readme
  assert "documentation links follow main and may differ from these sources" in readme
  assert "/blob/main/" in package_docs.documentation_url("python", None)


def test_container_can_receive_an_explicit_sha_without_git_metadata(tmp_path, monkeypatch):
  monkeypatch.setenv(package_docs.SOURCE_SHA_ENV, SHA)
  assert package_docs.source_sha(tmp_path) == SHA
  monkeypatch.setenv(package_docs.SOURCE_SHA_ENV, "main")
  with pytest.raises(ValueError):
    package_docs.source_sha(tmp_path)


@pytest.mark.parametrize(
  "dirty,supplied,expected_error",
  [(False, None, False), (True, None, False), (False, "b" * 40, True), (True, SHA, True)],
)
def test_git_checkout_and_explicit_sha_must_agree(tmp_path, monkeypatch, dirty, supplied, expected_error):
  (tmp_path / ".git").mkdir()
  monkeypatch.delenv(package_docs.SOURCE_SHA_ENV, raising=False)
  if supplied is not None:
    monkeypatch.setenv(package_docs.SOURCE_SHA_ENV, supplied)

  def check_output(command, **kwargs):
    if command[-1] == "--show-toplevel":
      return str(tmp_path) + "\n"
    if command[-1] == "HEAD":
      return SHA + "\n"
    return " M README.md\n" if dirty else ""

  monkeypatch.setattr(package_docs.subprocess, "check_output", check_output)
  if expected_error:
    with pytest.raises(ValueError, match="clean checkout"):
      package_docs.source_sha(tmp_path)
  else:
    assert package_docs.source_sha(tmp_path) == (None if dirty else SHA)


def test_metadata_provider_owns_all_generated_fields_and_accepts_no_fixed_sha(monkeypatch):
  monkeypatch.setattr(package_docs, "source_sha", lambda: SHA)
  metadata = package_docs.dynamic_metadata({}, {"version": "1.2.0"})
  assert metadata["urls"]["Documentation"] == f"{docs_links.REPOSITORY}/blob/{SHA}/bindings/python/README.md"
  assert metadata["urls"]["Repository"] == docs_links.REPOSITORY
  assert metadata["readme"]["content-type"] == "text/markdown"
  assert "package version 1.2.0" in metadata["readme"]["text"]
  with pytest.raises(ValueError, match="fixed source"):
    package_docs.dynamic_metadata({"source-sha": SHA}, {"version": "1.2.0"})


def test_metadata_module_imports_without_automation_or_third_party_packages():
  result = subprocess.run(
    [
      sys.executable,
      "-S",
      "-c",
      "import json,sys; import toolbox.package_docs; print(json.dumps(sorted(sys.modules)))",
    ],
    cwd=ROOT,
    capture_output=True,
    text=True,
    check=True,
  )
  modules = json.loads(result.stdout)
  assert not any(name.startswith(("automation", "requests", "yaml", "markdown_it")) for name in modules)


def test_producer_sha_ignores_untracked_sdk_but_rejects_untracked_production_input(tmp_path, monkeypatch):
  clone = tmp_path / "source"
  subprocess.run(["git", "clone", "--shared", str(ROOT), str(clone)], check=True, capture_output=True)
  sha = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=clone, text=True).strip()
  monkeypatch.setenv(package_docs.SOURCE_SHA_ENV, sha)
  sdk = clone / "emsdk/upstream"
  sdk.mkdir(parents=True)
  (sdk / "compiler").write_text("tool output")
  assert package_docs.source_sha(clone) == sha
  (clone / "src/untracked.hpp").write_text("production input")
  with pytest.raises(ValueError, match="clean checkout"):
    package_docs.source_sha(clone)


def test_every_producer_and_cibuildwheel_platform_receives_the_checked_out_sha():
  wheel = yaml.safe_load((ROOT / ".github/workflows/python-wheel.yml").read_text())
  wasm = yaml.safe_load((ROOT / ".github/workflows/wasm.yml").read_text())
  carrier = "CELESTIAL_DOCS_SOURCE_SHA=${{ github.sha }}"
  assert wheel["env"][package_docs.SOURCE_SHA_ENV] == "${{ github.sha }}"
  assert wasm["env"][package_docs.SOURCE_SHA_ENV] == "${{ github.sha }}"
  for platform in ("MACOS", "WINDOWS"):
    assert carrier in wheel["env"][f"CIBW_ENVIRONMENT_{platform}"]
  assert carrier in wheel["jobs"]["manylinux"]["env"]["CIBW_ENVIRONMENT_LINUX"]
