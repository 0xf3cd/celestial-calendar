# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Verify that mypy consumes the clean-installed package's inline annotations."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

import celestial_calendar


def run_mypy(source: Path) -> subprocess.CompletedProcess[str]:
  """Run the pinned type checker without reusing cached import metadata."""
  return subprocess.run(
    [sys.executable, "-m", "mypy", "--strict", "--no-incremental", "--show-error-codes", str(source)],
    cwd=source.parent,
    text=True,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    check=False,
  )


def main() -> None:
  """Exercise typed, mistyped, and missing-marker consumers."""
  package = Path(celestial_calendar.__file__).resolve().parent
  marker = package / "py.typed"
  hidden_marker = package / "py.typed.hidden"
  assert marker.read_bytes() == b""
  assert not hidden_marker.exists()

  with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    positive = root / "positive.py"
    positive.write_text(
      "from collections.abc import Callable\n"
      "from datetime import date, datetime\n"
      "from typing import assert_type\n"
      "from celestial_calendar import (\n"
      "  BodyState, ChartHouses, ChartSnapshot, ChartTarget, ChartTimes, CivilDateTime, CivilScale,\n"
      "  DeltaTModel, GeoLocation, HouseSystem, chart_snapshot,\n"
      "  GregorianDate, Jieqi, JieqiMoment, LunarAlgorithm, LunarDate, LunarYearInfo, LunarYearRange,\n"
      "  gregorian_to_lunar, jieqi_moment, lunar_to_gregorian, lunar_year_info,\n"
      "  sun_longitude_crossings, supported_lunar_year_range,\n"
      ")\n"
      "from celestial_calendar.chart import GeoLocation as ChartGeoLocation\n"
      "class DateSubclass(date):\n"
      "  pass\n"
      "class GregorianDateSubclass(GregorianDate):\n"
      "  pass\n"
      "moment: JieqiMoment = jieqi_moment(2026, Jieqi.LICHUN)\n"
      "crossings: tuple[float, ...] = sun_longitude_crossings(2024, 0.0)\n"
      "factory: Callable[[date], GregorianDate] = GregorianDate.from_date\n"
      "project: GregorianDate = factory(date(2024, 2, 10))\n"
      "assert_type(GregorianDate.from_date(DateSubclass(2024, 2, 10)), GregorianDate)\n"
      "assert_type(GregorianDateSubclass.from_date(date(2024, 2, 10)), GregorianDate)\n"
      "assert_type(GregorianDateSubclass(2024, 2, 10).from_date(date(2024, 2, 10)), GregorianDate)\n"
      "ordinary: date = project.to_date()\n"
      "assert_type(project.to_date(), date)\n"
      "lunar: LunarDate = gregorian_to_lunar(LunarAlgorithm.ALGO2, ordinary)\n"
      "project_lunar: LunarDate = gregorian_to_lunar(LunarAlgorithm.ALGO2, project)\n"
      "subclass_lunar: LunarDate = gregorian_to_lunar(LunarAlgorithm.ALGO2, DateSubclass(2024, 2, 10))\n"
      "restored: GregorianDate = lunar_to_gregorian(LunarAlgorithm.ALGO2, lunar)\n"
      "assert_type(restored.to_date(), date)\n"
      "bounds: LunarYearRange = supported_lunar_year_range(LunarAlgorithm.ALGO2)\n"
      "info: LunarYearInfo = lunar_year_info(LunarAlgorithm.ALGO2, 2024)\n"
      "assert_type(info.first_day.to_date(), date)\n"
      # datetime is a date subtype to mypy; smoke.py owns its runtime rejection.
      "typed_datetime: GregorianDate = GregorianDate.from_date(datetime(2024, 2, 10))\n"
      "typed_datetime_lunar: LunarDate = gregorian_to_lunar(LunarAlgorithm.ALGO2, datetime(2024, 2, 10))\n"
      "civil = CivilDateTime(2026, 1, 1, 0.5)\n"
      "location: GeoLocation = ChartGeoLocation(51.5, 0.0)\n"
      "chart: ChartSnapshot = chart_snapshot(civil, CivilScale.UTC, location, HouseSystem.PLACIDUS)\n"
      "assert_type(\n"
      "  chart_snapshot(civil, CivilScale.UT1, location, HouseSystem.EQUAL, DeltaTModel.ALGO5), ChartSnapshot,\n"
      ")\n"
      "assert_type(chart.times, ChartTimes)\n"
      "assert_type(chart.bodies, tuple[BodyState, ...])\n"
      "assert_type(chart.houses, ChartHouses)\n"
      "assert_type(chart.houses.cusps_deg, tuple[float, ...])\n"
      "assert_type(chart.bodies[0].target, ChartTarget)\n"
      "assert_type(chart.bodies[0].distance_au, float | None)\n"
      "assert_type(chart.bodies[0].longitude_rate_deg_per_tt_day, float)\n",
      encoding="utf-8",
    )
    result = run_mypy(positive)
    assert result.returncode == 0, result.stdout

    negative = root / "negative.py"
    negative.write_text(
      "from datetime import date, datetime\n"
      "from celestial_calendar import (\n"
      "  BodyState, CivilDateTime, CivilScale, GeoLocation, HouseSystem, chart_snapshot,\n"
      "  GregorianDate, Jieqi, LunarAlgorithm, gregorian_to_lunar, jieqi_moment,\n"
      "  lunar_to_gregorian, sun_longitude_crossings,\n"
      ")\n"
      "from celestial_calendar import solar_longitude_roots\n"
      "class GregorianDateSubclass(GregorianDate):\n"
      "  pass\n"
      "moment: str = jieqi_moment(2026, Jieqi.LICHUN)\n"
      "GregorianDate.from_date('2024-02-10')\n"
      "GregorianDate.from_date(GregorianDate(2024, 2, 10))\n"
      "gregorian_to_lunar(LunarAlgorithm.ALGO2, '2024-02-10')\n"
      "lunar_to_gregorian(LunarAlgorithm.ALGO2, date(2024, 2, 10))\n"
      "sun_longitude_crossings(2024, '0.0')\n"
      "bad_factory: date = GregorianDate.from_date(date(2024, 2, 10))\n"
      "bad_result: GregorianDate = GregorianDate(2024, 2, 10).to_date()\n"
      "bad_subclass: GregorianDateSubclass = GregorianDateSubclass.from_date(date(2024, 2, 10))\n"
      "bad_datetime: datetime = GregorianDate(2024, 2, 10).to_date()\n"
      "bad_crossings: list[float] = sun_longitude_crossings(2024, 0.0)\n"
      "civil = CivilDateTime(2026, 1, 1, 0.5)\n"
      "location = GeoLocation(51.5, 0.0)\n"
      "chart = chart_snapshot(civil, CivilScale.UTC, location, HouseSystem.PLACIDUS)\n"
      "bad_chart: str = chart\n"
      "chart_snapshot(datetime(2026, 1, 1), CivilScale.UTC, location, HouseSystem.EQUAL)\n"
      "chart_snapshot(civil, 0, location, HouseSystem.EQUAL)\n"
      "chart_snapshot(civil, CivilScale.UTC, (51.5, 0.0), HouseSystem.EQUAL)\n"
      "chart_snapshot(civil, CivilScale.UTC, location, 0)\n"
      "chart_snapshot(civil, CivilScale.UTC, location, HouseSystem.EQUAL, 'algo5')\n"
      "bad_bodies: list[BodyState] = chart.bodies\n"
      "bad_cusps: list[float] = chart.houses.cusps_deg\n"
      "chart.times.jd_ut1 = 0.0\n"
      "chart.bodies += ()\n",
      encoding="utf-8",
    )
    result = run_mypy(negative)
    assert result.returncode == 1, result.stdout
    assert "[assignment]" in result.stdout
    expected_errors = (
      'has no attribute "solar_longitude_roots"',
      'expression has type "JieqiMoment", variable has type "str"',
      'Argument 1 to "from_date" of "GregorianDate" has incompatible type "str"',
      'Argument 1 to "from_date" of "GregorianDate" has incompatible type "GregorianDate"',
      'Argument 2 to "gregorian_to_lunar" has incompatible type "str"',
      'Argument 2 to "lunar_to_gregorian" has incompatible type "date"',
      'Argument 2 to "sun_longitude_crossings" has incompatible type "str"',
      'expression has type "GregorianDate", variable has type "date"',
      'expression has type "date", variable has type "GregorianDate"',
      'expression has type "GregorianDate", variable has type "GregorianDateSubclass"',
      'expression has type "date", variable has type "datetime"',
      'expression has type "tuple[float, ...]", variable has type "list[float]"',
      'expression has type "ChartSnapshot", variable has type "str"',
      'Argument 1 to "chart_snapshot" has incompatible type "datetime"',
      'Argument 2 to "chart_snapshot" has incompatible type "int"',
      'Argument 3 to "chart_snapshot" has incompatible type "tuple[float, float]"',
      'Argument 4 to "chart_snapshot" has incompatible type "int"',
      'Argument 5 to "chart_snapshot" has incompatible type "str"',
      'expression has type "tuple[BodyState, ...]", variable has type "list[BodyState]"',
      'Property "jd_ut1" defined in "ChartTimes" is read-only',
      'Property "bodies" defined in "ChartSnapshot" is read-only',
    )
    for diagnostic in expected_errors:
      assert diagnostic in result.stdout, result.stdout
    # The tuple[float, ...] -> list[float] witness now occurs for both crossings and cusps.
    assert result.stdout.count(": error:") == len(expected_errors) + 1, result.stdout
    assert "[attr-defined]" in result.stdout and "[arg-type]" in result.stdout

    marker.replace(hidden_marker)
    try:
      result = run_mypy(positive)
    finally:
      hidden_marker.replace(marker)
    assert result.returncode == 1, result.stdout
    assert 'Skipping analyzing "celestial_calendar"' in result.stdout
    assert "[import-untyped]" in result.stdout


if __name__ == "__main__":
  main()
