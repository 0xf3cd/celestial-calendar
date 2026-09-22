# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import sys
from pathlib import Path

import pytest
import yaml

import project
from automation import build
from automation.utils import ProcReturn


ROOT = Path(__file__).parents[2]


def generated_docs(root):
  html = root / "build/api-docs/html"
  pages = {
    "index.html": "<html>API Reference 0.7.0</html>",
    "celestial_8h.html": "query_jieqi_moment last_error",
    "structastro_1_1toolbox_1_1SphericalCoordinate.html": "\u03bb \u03b2",
    "structcalendar_1_1lunar_1_1converter_1_1Converter.html": "is_valid_gregorian \u516c\u5386 \u9634\u5386",
    "doxygen.css": "body {}",
    "navtree.js": "const tree = [];",
    "search/search.js": "const search = [];",
    "LICENSE": (root / "LICENSE").read_text(encoding="utf-8"),
    "THIRD_PARTY_NOTICES.txt": (root / "THIRD_PARTY_NOTICES.txt").read_text(encoding="utf-8"),
  }
  for name, text in pages.items():
    path = html / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
  return html


@pytest.fixture
def doc_root(tmp_path, monkeypatch):
  (tmp_path / "docs").mkdir()
  (tmp_path / "docs/Doxyfile").write_text("OUTPUT_DIRECTORY = build/api-docs\n", encoding="utf-8")
  (tmp_path / "LICENSE").write_text("project license\n", encoding="utf-8")
  (tmp_path / "THIRD_PARTY_NOTICES.txt").write_text("retained terms\n", encoding="utf-8")
  monkeypatch.setattr(build.paths, "proj_root", lambda: tmp_path)
  monkeypatch.setattr(build, "check_tool", lambda tool, report: tool.name == "doxygen" and report)
  return tmp_path


@pytest.mark.parametrize("arguments,expected", [(["--docs"], True), (["--all"], False), (["--all", "--docs"], True)])
def test_documentation_is_explicitly_opt_in(monkeypatch, arguments, expected):
  monkeypatch.setattr(sys, "argv", ["project.py", *arguments])
  args = project.parse_args()
  assert args.docs is expected
  tasks = project.create_tasks(args)
  assert ("Generate API HTML" in [task.name for task in tasks]) is expected
  if arguments == ["--docs"]:
    versions = []
    monkeypatch.setattr(project, "build_docs", lambda version: versions.append(version) or 0)
    assert len(tasks) == 1 and tasks[0].func() == 0
    assert versions == [project.BUILD_VERSION]


@pytest.mark.parametrize("failure", [None, "missing-tool", "missing-config", "generator-error", "stale-output"])
def test_docs_generation_has_no_compiler_and_rejects_stale_output(doc_root, monkeypatch, failure):
  old = generated_docs(doc_root)
  (doc_root / "build/unrelated.txt").write_text("keep", encoding="utf-8")
  calls = []

  def run(command, **kwargs):
    calls.append(command)
    assert command == ["doxygen", str(doc_root / "docs/Doxyfile")]
    assert kwargs["cwd"] == doc_root and kwargs["env"]["BUILD_VERSION"] == "0.7.0"
    assert not (old / "index.html").exists()
    if failure == "generator-error":
      return ProcReturn(2, "", "generator failed")
    if failure != "stale-output":
      generated_docs(doc_root)
    return ProcReturn(0, "", "")

  monkeypatch.setattr(build, "run_cmd", run)
  if failure == "missing-tool":
    monkeypatch.setattr(build, "check_tool", lambda *args, **kwargs: False)
  if failure == "missing-config":
    (doc_root / "docs/Doxyfile").unlink()
  result = build.build_docs("0.7.0")
  assert result == (0 if failure is None else 2 if failure == "generator-error" else 1)
  assert len(calls) == (0 if failure in ("missing-tool", "missing-config") else 1)
  assert (doc_root / "build/unrelated.txt").read_text(encoding="utf-8") == "keep"


@pytest.mark.parametrize(
  "name,text",
  [
    ("index.html", ""),
    ("index.html", "API Reference 0.6.0"),
    ("celestial_8h.html", "missing C exports"),
    ("structastro_1_1toolbox_1_1SphericalCoordinate.html", "lambda beta"),
    ("structcalendar_1_1lunar_1_1converter_1_1Converter.html", "is_valid_gregorian"),
    ("doxygen.css", ""),
    ("navtree.js", None),
    ("search/search.js", ""),
    ("LICENSE", "changed license"),
    ("THIRD_PARTY_NOTICES.txt", None),
    ("header_8hpp_source.html", "source listing"),
  ],
)
def test_docs_validate_generated_pages_assets_and_attribution(doc_root, monkeypatch, name, text):
  def run(*_args, **_kwargs):
    html = generated_docs(doc_root)
    if text is None:
      (html / name).unlink()
    else:
      (html / name).write_text(text, encoding="utf-8")
    return ProcReturn(0, "", "")

  monkeypatch.setattr(build, "run_cmd", run)
  assert build.build_docs("0.7.0") == 1


def test_docs_ci_pins_doxygen_and_uploads_only_html():
  workflow = yaml.safe_load((ROOT / ".github/workflows/core_tests.yml").read_text(encoding="utf-8"))
  assert workflow["permissions"] == {"contents": "read"}
  job = workflow["jobs"]["api-docs"]
  assert job["if"] == "github.event_name == 'push' || github.event.pull_request.head.repo.fork"
  assert job["runs-on"] == "ubuntu-26.04"
  assert job["env"] == {"DOXYGEN_PACKAGE_VERSION": "1.15.0+ds1-1ubuntu3"}
  checkout, python, install, generate, upload = job["steps"]
  assert checkout == {"uses": "actions/checkout@v7", "with": {"persist-credentials": False}}
  assert python == {"uses": "actions/setup-python@v7", "with": {"python-version": "3.12"}}
  assert install["run"].splitlines() == [
    "sudo apt-get update",
    'sudo apt-get install --no-install-recommends -y "doxygen=$DOXYGEN_PACKAGE_VERSION"',
    'test "$(doxygen --version)" = "${DOXYGEN_PACKAGE_VERSION%%+*}"',
  ]
  assert generate == {"name": "Generate API HTML", "run": "python3 project.py --docs"}
  assert upload == {
    "uses": "actions/upload-artifact@v7",
    "with": {
      "name": "celestial-api-html",
      "path": "build/api-docs/html/",
      "if-no-files-found": "error",
      "retention-days": 30,
    },
  }


def test_doxygen_configuration_has_no_source_listing_or_release_output():
  config = {}
  for line in (ROOT / "docs/Doxyfile").read_text(encoding="utf-8").splitlines():
    if line.strip() and not line.startswith("#"):
      key, value = line.split("=", 1)
      config[key.strip()] = value.strip()
  assert config["INPUT"] == "docs/API.md src/astro src/calendar src/util src/shared_lib/celestial.h"
  assert config["OUTPUT_DIRECTORY"] == "build/api-docs"
  assert config["OUTPUT_LANGUAGE"] == "English"
  assert config["EXCLUDE_PATTERNS"] == "*/vsop87d/*_coeff.hpp */vsop87d_check_data.hpp"
  assert config["MAX_INITIALIZER_LINES"] == "0"
  assert config["USE_MDFILE_AS_MAINPAGE"] == "docs/API.md"
  assert config["HTML_EXTRA_FILES"] == "LICENSE THIRD_PARTY_NOTICES.txt"
  assert config["SOURCE_BROWSER"] == config["VERBATIM_HEADERS"] == config["HAVE_DOT"] == "NO"
  assert config["GENERATE_LATEX"] == config["WARN_IF_UNDOCUMENTED"] == config["WARN_IF_INCOMPLETE_DOC"] == "NO"
  assert config["WARN_AS_ERROR"] == "YES"
