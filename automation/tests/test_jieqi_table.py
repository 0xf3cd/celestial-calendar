# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import math

import pytest

from toolbox import jieqi_table


@pytest.mark.parametrize(
  ("fields", "jd", "display"),
  [
    ((2000, 1, 1, 0.5), 2451545.0, "2000-01-01T12:00:00.000"),
    ((2024, 6, 1, 0.5), 2460463.0, "2024-06-01T12:00:00.000"),
    ((401, 1, 1, 0.0), 1867522.5, "0401-01-01T00:00:00.000"),
  ],
)
def test_ut1_julian_day_and_unzoned_display(fields, jd, display):
  assert jieqi_table.ut1_fields(*fields) == (jd, display)


@pytest.mark.parametrize("year", [1961, 1972, 2016, 2050])
def test_each_era_stays_ut1_without_a_utc_claim(year):
  jd, display = jieqi_table.ut1_fields(year, 12, 31, 0.5)
  assert math.isfinite(jd)
  assert display == f"{year}-12-31T12:00:00.000"
  assert not display.endswith("Z")
  assert "No UTC conversion" in jieqi_table.TIMESCALE_NOTE


@pytest.mark.parametrize("fraction", [-1e-12, 1.0, 1e30, math.nan, math.inf])
def test_invalid_ut1_fields_fail(fraction):
  with pytest.raises(ValueError, match="fraction"):
    jieqi_table.ut1_fields(2024, 1, 1, fraction)


def test_display_truncates_without_midnight_rollover():
  _, display = jieqi_table.ut1_fields(2024, 1, 1, math.nextafter(1.0, 0.0))
  assert display == "2024-01-01T23:59:59.999"


def test_provenance_failure_precedes_native_loading(monkeypatch, tmp_path):
  artifact = tmp_path / "native-artifact"
  artifact.write_bytes(b"unverified")
  monkeypatch.setattr(jieqi_table, "find_shared_lib", lambda: artifact)

  def reject(*args):
    raise RuntimeError("missing provenance")

  def must_not_load(*args):
    raise AssertionError("native loading occurred before provenance verification")

  monkeypatch.setattr(jieqi_table, "verify_library", reject)
  monkeypatch.setattr(jieqi_table, "load_lib", must_not_load)
  with pytest.raises(RuntimeError, match="provenance"):
    jieqi_table.generate(2024, 2024)


@pytest.mark.parametrize("years", [(400, 2024), (2025, 2024), (2024, 10000)])
def test_invalid_year_window_precedes_loading(years):
  with pytest.raises(ValueError, match="year range"):
    jieqi_table.generate(*years)
