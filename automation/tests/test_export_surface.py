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

from automation.export_surface import find_real_so

pytestmark = pytest.mark.skipif(
  sys.platform == "win32", reason="ELF version-link tests do not require Windows symlink privileges"
)


@pytest.mark.parametrize("indirect", [False, True])
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
