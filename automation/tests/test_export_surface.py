# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import sys

import pytest

from automation import export_surface
from automation.export_surface import find_real_so

requires_symlinks = pytest.mark.skipif(
  sys.platform == "win32", reason="ELF version-link tests do not require Windows symlink privileges"
)


@pytest.mark.parametrize("indirect", [False, True])
@requires_symlinks
def test_current_link_selects_the_built_version_not_the_oldest(tmp_path, indirect):
  old = tmp_path / "libcelestial_calendar.so.0.9.0"
  current = tmp_path / "libcelestial_calendar.so.1.0.0"
  old.write_bytes(b"old")
  current.write_bytes(b"current")
  if indirect:
    (tmp_path / "libcelestial_calendar.so.1").symlink_to(current.name)
  (tmp_path / "libcelestial_calendar.so").symlink_to("libcelestial_calendar.so.1" if indirect else current.name)
  assert find_real_so(tmp_path) == current


@pytest.mark.parametrize("fault", ["missing", "broken", "outside", "unversioned", "directory"])
@requires_symlinks
def test_no_fallback_to_an_old_version_when_the_current_link_is_invalid(tmp_path, fault):
  folder = tmp_path / "shared_lib"
  folder.mkdir()
  (folder / "libcelestial_calendar.so.0.9.0").write_bytes(b"old")
  link = folder / "libcelestial_calendar.so"
  if fault == "broken":
    link.symlink_to("libcelestial_calendar.so.1.0.0")
  elif fault == "outside":
    outside = tmp_path / "libcelestial_calendar.so.1.0.0"
    outside.write_bytes(b"unrelated")
    link.symlink_to(outside)
  elif fault == "unversioned":
    link.write_bytes(b"unversioned")
  elif fault == "directory":
    folder /= "missing"
  assert find_real_so(folder) is None


@pytest.fixture
def gate_header(tmp_path, monkeypatch):
  header = tmp_path / "src/shared_lib/celestial.h"
  header.parent.mkdir(parents=True)
  header.write_text("CELESTIAL_API bool celestial_probe(void);\n", encoding="utf-8")
  monkeypatch.setattr(export_surface.paths, "proj_root", lambda: tmp_path)
  return header


@pytest.mark.parametrize("platform", ["darwin", "win32", "freebsd"])
@pytest.mark.parametrize("marked", [True, False])
def test_nonlinux_checks_static_markers_without_elf_access(gate_header, monkeypatch, capsys, platform, marked):
  monkeypatch.setattr(sys, "platform", platform)
  if not marked:
    gate_header.write_text("bool celestial_probe(void);\n", encoding="utf-8")

  def forbidden(*args):
    pytest.fail("ELF discovery or tools ran on a non-Linux platform")

  monkeypatch.setattr(export_surface.paths, "build_dir", forbidden)
  for name in ("find_real_so", "nm_defined", "demangle", "read_soname"):
    monkeypatch.setattr(export_surface, name, forbidden)
  assert export_surface.check_export_surface() == (0 if marked else 1)
  output = capsys.readouterr().out
  assert ("checks not applicable" if marked else "without CELESTIAL_API") in output


@pytest.mark.parametrize("platform", ["linux", "darwin", "win32"])
@pytest.mark.parametrize("self_test", ["self_test_parser", "self_test_predicate"])
def test_self_test_failures_precede_platform_checks(monkeypatch, capsys, platform, self_test):
  monkeypatch.setattr(sys, "platform", platform)
  monkeypatch.setattr(export_surface, self_test, lambda: ["injected self-test failure"])

  def forbidden():
    pytest.fail("read the header after a gate self-test failure")

  monkeypatch.setattr(export_surface.paths, "proj_root", forbidden)
  assert export_surface.check_export_surface() == 1
  assert "self-test failed" in capsys.readouterr().out


@pytest.fixture
def linux_surface(gate_header, monkeypatch):
  monkeypatch.setattr(sys, "platform", "linux")
  library = gate_header.parent / "libcelestial_calendar.so.1.2.3"
  monkeypatch.setattr(export_surface, "find_real_so", lambda folder: library)
  symbols = {"celestial_probe": "T", "_ZStprobe": "W"}
  monkeypatch.setattr(export_surface, "nm_defined", lambda so: symbols)
  monkeypatch.setattr(
    export_surface,
    "demangle",
    lambda names: {name: "std::probe()" if name == "_ZStprobe" else "astro::probe()" for name in names},
  )
  monkeypatch.setattr(export_surface, "read_soname", lambda so: "libcelestial_calendar.so.1")
  return symbols


@pytest.mark.parametrize("version,soname", [("1.2.3", "1"), ("0.9.1", "0.9")])
def test_linux_checks_dynamic_exports_and_soname(linux_surface, gate_header, monkeypatch, capsys, version, soname):
  library = gate_header.parent / f"libcelestial_calendar.so.{version}"
  monkeypatch.setattr(export_surface, "find_real_so", lambda folder: library)
  monkeypatch.setattr(export_surface, "read_soname", lambda so: f"libcelestial_calendar.so.{soname}")
  assert export_surface.check_export_surface() == 0
  output = capsys.readouterr().out
  assert "1 entries, 1 std vague-linkage survivors" in output and f"SONAME libcelestial_calendar.so.{soname}" in output


@pytest.mark.parametrize("fault", ["missing-build", "missing-export", "extra-export", "soname"])
def test_linux_dynamic_failures_remain_failures(linux_surface, monkeypatch, capsys, fault):
  if fault == "missing-build":
    monkeypatch.setattr(export_surface, "find_real_so", lambda folder: None)
  elif fault == "missing-export":
    linux_surface.pop("celestial_probe")
  elif fault == "extra-export":
    linux_surface["survivor"] = "T"
  else:
    monkeypatch.setattr(export_surface, "read_soname", lambda so: "wrong-soname")
  assert export_surface.check_export_surface() == 1
  message = {
    "missing-build": "Built library not found",
    "missing-export": "entry point not exported",
    "extra-export": "neither an entry point nor a std",
    "soname": "SONAME is",
  }[fault]
  assert message in capsys.readouterr().out


@pytest.mark.parametrize("tool", ["nm_defined", "demangle", "read_soname"])
@pytest.mark.parametrize("error_type", [RuntimeError, OSError, ValueError])
def test_linux_tool_failures_keep_static_findings(linux_surface, gate_header, monkeypatch, capsys, tool, error_type):
  gate_header.write_text("bool celestial_probe(void);\n", encoding="utf-8")

  def fail(*args):
    raise error_type("injected ELF tool failure")

  monkeypatch.setattr(export_surface, tool, fail)
  assert export_surface.check_export_surface() == 1
  output = capsys.readouterr().out
  assert "without CELESTIAL_API" in output and "injected ELF tool failure" in output
