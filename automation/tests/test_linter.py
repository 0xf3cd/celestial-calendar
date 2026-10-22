# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import pytest
import yaml

from automation import linter
from automation.utils import ProcReturn


@pytest.mark.parametrize(
  "source",
  [
    "inline auto f(const A& a, const B& b) -> Result {}",
    "inline auto f(\n  const A& a,\n  const B& b\n) -> Result {}",
    "template <typename F>\nauto wrap(F&& f) noexcept -> decltype(f()) {}",
    "auto f() const & noexcept(noexcept(g())) -> Result;",
    "auto Type::f() const volatile && -> Result;",
    "auto operator()(const A& a) -> Result;",
    "auto operator<=>(const A& a) const -> Result;",
    "auto f() /* explanation */ -> Result;",
    "// auto f()\n//   -> Result {}\n",
    "/* auto f()\n  -> Result {} */",
    'const auto text = "auto f()\\n  -> Result {}";',
    'const auto text = R"tag(auto f()\n  -> Result {})tag";',
    'const auto text = u8R"(auto f()\n  -> Result {})";',
    "const auto character = '(';\nauto f() -> Result;",
    "auto f(const int32_t n = 1'000) -> Result;",
    r'auto f(const char* text = "(\")") -> Result;',
    r"auto f(const wchar_t character = L'\"') -> Result;",
    "const auto f = []()\n  -> Result {};",
    "using Result = decltype(object\n  ->member());",
    "auto value = object\n  ->member();",
  ],
)
def test_cpp_signature_layout_accepts_allowed_forms_and_unrelated_arrows(source):
  assert linter.trailing_return_breaks(source) == []


@pytest.mark.parametrize(
  ("source", "lines"),
  [
    ("inline auto f(const A& a, const B& b)\n  -> Result {}", [2]),
    ("inline auto f(\n  const A& a,\n  const B& b\n)\n  -> Result {}", [5]),
    ("auto f() noexcept\n  -> Result;", [2]),
    ("auto f() const & noexcept(noexcept(g()))\n  -> Result;", [2]),
    ("auto Type::f() const volatile &&\n  -> Result;", [2]),
    ("auto operator()(const A& a)\n  -> Result;", [2]),
    ("auto operator<=>(const A& a) const\n  -> Result;", [2]),
    ("auto f() // explanation\n  -> Result;", [2]),
    ("auto f() /* explanation\n */ -> Result;", [2]),
    ("auto f()\n  -> Result;\nauto g()\n  -> Other;", [2, 4]),
    ('const auto text = R"tag(auto fake()\n -> Fake;)tag";\nauto real()\n -> Result;', [4]),
  ],
)
def test_cpp_signature_layout_rejects_detached_trailing_returns(source, lines):
  assert linter.trailing_return_breaks(source) == lines


def test_cpp_signature_layout_gate_reports_source_and_skips_frozen_provenance(monkeypatch, tmp_path, capsys):
  source = tmp_path / "src"
  source.mkdir()
  (source / "example.hpp").write_text("auto f()\n  -> Result;", encoding="utf-8")
  frozen = source / "test/provenance/example.hpp"
  frozen.parent.mkdir(parents=True)
  frozen.write_text("auto retained()\n  -> Result;", encoding="utf-8")
  monkeypatch.setattr(linter.paths, "proj_root", lambda: tmp_path)

  assert linter.check_cpp_signature_layout() == 1
  output = capsys.readouterr().out
  assert "src/example.hpp:2:" in output
  assert "retained" not in output and "provenance" not in output

  (source / "example.hpp").write_text("auto f() -> Result;", encoding="utf-8")
  assert linter.check_cpp_signature_layout() == 0


def test_clang_tidy_entrypoint_rejects_layout_before_running_tools(monkeypatch, tmp_path):
  source = tmp_path / "src"
  source.mkdir()
  (source / "example.hpp").write_text("auto f()\n  -> Result;", encoding="utf-8")
  monkeypatch.setattr(linter.paths, "proj_root", lambda: tmp_path)

  def unexpected_tool(*_args, **_kwargs):
    pytest.fail("layout failure must stop before invoking clang-tidy")

  monkeypatch.setattr(linter, "check_tool", unexpected_tool)
  monkeypatch.setattr(linter, "run_cmd", unexpected_tool)
  assert linter.run_clang_tidy() == 1


@pytest.mark.parametrize(
  ("retcodes", "expected_commands", "expected_result"),
  [
    ([0, 0], 2, 0),
    ([1], 1, 1),
    ([2], 1, 2),
    ([0, 1], 2, 1),
    ([0, 2], 2, 2),
  ],
)
def test_run_ruff_checks_lint_and_format(monkeypatch, tmp_path, retcodes, expected_commands, expected_result):
  results = iter(retcodes)
  commands = []

  monkeypatch.setattr(linter, "check_tool", lambda _tool: True)
  monkeypatch.setattr(linter.paths, "proj_root", lambda: tmp_path)
  monkeypatch.setattr(
    linter,
    "run_cmd",
    lambda command: commands.append(command) or ProcReturn(next(results), "", ""),
  )

  assert linter.run_ruff() == expected_result
  assert (
    commands
    == [
      ["ruff", "check", str(tmp_path)],
      ["ruff", "format", "--check", str(tmp_path)],
    ][:expected_commands]
  )


def test_core_tests_pin_ruff_and_run_repo_entrypoint():
  workflow_path = linter.paths.proj_root() / ".github" / "workflows" / "core_tests.yml"
  workflow = yaml.safe_load(workflow_path.read_text(encoding="utf-8"))
  steps = workflow["jobs"]["linters-and-static-analysis"]["steps"]
  install = next(step for step in steps if step.get("name") == "Install Python Dependencies")
  style = next(step for step in steps if step.get("name") == "Style Check")

  assert "python3 -m pip install ruff==0.16.1" in install["run"].splitlines()
  assert "./checks.py --ruff --clang-tidy --abi-layout" in [line.strip() for line in style["run"].splitlines()]
