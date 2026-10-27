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
    "auto operator co_await() -> Result;",
    "auto operator,(const A& a) -> Result;",
    "auto operator new(std::size_t n) -> void*;",
    "auto operator new[](std::size_t n) -> void*;",
    "auto operator new [](std::size_t n) -> void*;",
    "auto operator delete(void* p) noexcept -> void;",
    "auto operator delete[](void* p) noexcept -> void;",
    "auto operator delete [](void* p) noexcept -> void;",
    'constexpr auto operator""_deg(long double x) -> Angle;',
    'constexpr auto operator "" _deg(long double x) -> Angle;',
    "#define PROBE_DECL auto probe_fn() -> int\nPROBE_DECL { return 0; }",
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
    ("auto operator co_await()\n  -> Result;", [2]),
    ("auto operator,(const A& a)\n  -> Result;", [2]),
    ("auto operator new(std::size_t n)\n  -> void*;", [2]),
    ("auto operator new[](std::size_t n)\n  -> void*;", [2]),
    ("auto operator new [](std::size_t n)\n  -> void*;", [2]),
    ("auto operator delete(void* p) noexcept\n  -> void;", [2]),
    ("auto operator delete[](void* p) noexcept\n  -> void;", [2]),
    ("auto operator delete [](void* p) noexcept\n  -> void;", [2]),
    ('constexpr auto operator""_deg(long double x)\n  -> Angle;', [2]),
    ('constexpr auto operator "" _deg(long double x)\n  -> Angle;', [2]),
    ("#define PROBE_DECL auto probe_fn() \\\n  -> int\nPROBE_DECL { return 0; }", [2]),
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
  (source / "directory.hpp").mkdir()
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


def test_cpp_signature_layout_checks_documentation_examples_but_not_build_outputs(monkeypatch, tmp_path, capsys):
  example = tmp_path / "docs/examples/chart_snapshot/fixture.cpp"
  example.parent.mkdir(parents=True)
  example.write_text("auto fixture()\n  -> Result;", encoding="utf-8")
  generated = tmp_path / "build/generated.hpp"
  generated.parent.mkdir()
  generated.write_text("auto generated()\n  -> Result;", encoding="utf-8")
  monkeypatch.setattr(linter.paths, "proj_root", lambda: tmp_path)

  assert linter.check_cpp_signature_layout() == 1
  output = capsys.readouterr().out
  assert "docs/examples/chart_snapshot/fixture.cpp:2:" in output
  assert "generated.hpp" not in output

  example.write_text("auto fixture() -> Result;", encoding="utf-8")
  assert linter.check_cpp_signature_layout() == 0


def test_clang_tidy_entrypoint_runs_independently_of_layout(monkeypatch, tmp_path):
  (tmp_path / "compile_commands.json").write_text("[]", encoding="utf-8")
  monkeypatch.setattr(linter.paths, "build_dir", lambda: tmp_path)
  monkeypatch.setattr(linter.paths, "proj_root", lambda: tmp_path)
  monkeypatch.setattr(linter, "check_cpp_signature_layout", lambda: pytest.fail("tidy must not call layout"))
  monkeypatch.setattr(linter, "check_tool", lambda *_args, **_kwargs: True)
  calls = []
  monkeypatch.setattr(linter, "run_cmd", lambda *args, **_kwargs: calls.append(args) or ProcReturn(7, "finding", ""))
  assert linter.run_clang_tidy() == 7
  assert len(calls) == 1
  assert (tmp_path / "clang-tidy-stdout.log").read_text() == "finding"


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
  layout = next(step for step in steps if step.get("name") == "Check C++ signature layout")
  assert layout["run"] == "python3 ./checks.py --signature-layout"
  assert style["if"] == "${{ !cancelled() }}"
  cmake = next(step for step in steps if step.get("name") == "Install CMake")
  assert cmake["if"] == style["if"]
  provenance = next(step for step in steps if step.get("name") == "Verify retained provenance")
  assert provenance["run"] == "python3 ./checks.py --provenance"
