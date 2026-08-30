# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

from pathlib import Path
from shutil import copy2

import pytest

from automation.internal_provenance import (
  REPO_ROOT,
  InternalProvenanceCounts,
  verify_internal_provenance,
)


TARGET_FILES = (
  Path("src/astro/sun.hpp"),
  Path("src/test/astro/earth_test.cpp"),
  Path("src/test/astro/sun_test.cpp"),
  Path("src/test/astro/moon_test.cpp"),
  Path("src/test/astro/elp2000_82b_test.cpp"),
  Path("src/test/astro/julian_day_test.cpp"),
  Path("src/test/astro/rise_set_golden_test.cpp"),
  Path("statistics/sun_equatorial_horizons_crawler.py"),
  Path("statistics/sunrise_golden_crawler.py"),
)


def materialize_inputs(destination: Path) -> None:
  for relative in TARGET_FILES:
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    copy2(REPO_ROOT / relative, target)


def replace_once(path: Path, old: str, new: str) -> None:
  text = path.read_text(encoding="utf-8")
  assert text.count(old) == 1
  path.write_text(text.replace(old, new), encoding="utf-8")


def test_internal_provenance_is_complete():
  assert verify_internal_provenance() == InternalProvenanceCounts(8, 42, 7)


@pytest.mark.parametrize(
  ("relative", "old", "new"),
  [
    (
      "src/test/astro/earth_test.cpp",
      "TEST(Earth, Vsop87dEvaluate) {\n  using namespace heliocentric_coord;\n\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by b5f6c35",
      "TEST(Earth, Vsop87dEvaluate) {\n  using namespace heliocentric_coord;\n\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by unknown",
    ),
    (
      "src/test/astro/earth_test.cpp",
      "TEST(Earth, NutationMeeus) {\n  using namespace nutation;\n\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by a99d4d7",
      "TEST(Earth, NutationMeeus) {\n  using namespace nutation;\n\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by unknown",
    ),
    (
      "src/test/astro/sun_test.cpp",
      "TEST(Sun, GeocentricPosition) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by b5f6c35",
      "TEST(Sun, GeocentricPosition) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by unknown",
    ),
    (
      "src/test/astro/sun_test.cpp",
      "TEST(Sun, CorrectedPosition) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by f4b0980",
      "TEST(Sun, CorrectedPosition) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by unknown",
    ),
    (
      "src/test/astro/moon_test.cpp",
      "TEST(Moon, CoordAndPpi) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by 066c28d",
      "TEST(Moon, CoordAndPpi) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by unknown",
    ),
    (
      "src/test/astro/moon_test.cpp",
      "TEST(Moon, Perturbation) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by 066c28d",
      "TEST(Moon, Perturbation) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by unknown",
    ),
    (
      "src/test/astro/elp2000_82b_test.cpp",
      "TEST(Elp2000, Evaluate) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by 066c28d",
      "TEST(Elp2000, Evaluate) {\n"
      "  // PyMeeus-derived regression material; the original version and sample seed are not recorded.\n"
      "  // Introduced by unknown",
    ),
    (
      "src/test/astro/julian_day_test.cpp",
      "This spot-check family was introduced by da333dd",
      "This spot-check family was introduced by unknown",
    ),
  ],
  ids=[
    "earth-vsop",
    "earth-nutation",
    "sun-geocentric",
    "sun-corrected",
    "moon-coordinates",
    "moon-perturbation",
    "elp2000",
    "julian-day",
  ],
)
def test_missing_v37_history_anchor_fails(tmp_path, relative, old, new):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / relative, old, new)

  with pytest.raises(RuntimeError, match="history anchor|introduction anchor"):
    verify_internal_provenance(tmp_path)


@pytest.mark.parametrize(
  ("old", "new", "message"),
  [
    (
      "The other seven pre-V28 rows are internal regression material",
      "All thirteen rows are internal regression material",
      "row labels differ",
    ),
    (
      "Meeus Ch.7 worked value",
      "Meeus Ch.7 worked value; internal regression material",
      "internal label count differs",
    ),
    (
      "V28 rows from http://www.stevegs.com/utils/jd_calc/",
      "V28 internal regression material from http://www.stevegs.com/utils/jd_calc/",
      "internal label count differs",
    ),
  ],
  ids=["all-rows", "meeus-row", "v28-rows"],
)
def test_julian_internal_partition_is_exact(tmp_path, old, new, message):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "src/test/astro/julian_day_test.cpp", old, new)

  with pytest.raises(RuntimeError, match=message):
    verify_internal_provenance(tmp_path)


@pytest.mark.parametrize(
  "relative",
  ["statistics/sunrise_golden_crawler.py", "src/test/astro/rise_set_golden_test.cpp"],
  ids=["crawler", "stored-test"],
)
def test_unsupported_v42_claim_fails(tmp_path, relative):
  materialize_inputs(tmp_path)
  path = tmp_path / relative
  text = path.read_text(encoding="utf-8")
  path.write_text(text + "\n// sunrise-sunset.org differs by 0.2 min.\n", encoding="utf-8")

  with pytest.raises(RuntimeError, match="unsupported V42 numeric prose"):
    verify_internal_provenance(tmp_path)


@pytest.mark.parametrize(
  "relative",
  ["statistics/sunrise_golden_crawler.py", "src/test/astro/rise_set_golden_test.cpp"],
  ids=["crawler", "stored-test"],
)
def test_noaa_acknowledgment_is_required(tmp_path, relative):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / relative, "solar-calculator output used in this comparison.", "solar comparison output.")

  with pytest.raises(RuntimeError, match="NOAA GML acknowledgment is missing"):
    verify_internal_provenance(tmp_path)


def test_r35_history_and_source_role_are_required(tmp_path):
  materialize_inputs(tmp_path)
  replace_once(
    tmp_path / "src/astro/sun.hpp",
    "conceptual and implementation\n// ideas",
    "background\n// reading",
  )

  with pytest.raises(RuntimeError, match="R35 nong source role differs"):
    verify_internal_provenance(tmp_path)


def test_v07_fixed_input_inventory_is_exact(tmp_path):
  materialize_inputs(tmp_path)
  replace_once(
    tmp_path / "statistics/sun_equatorial_horizons_crawler.py",
    '  "2432253.451627",\n',
    "",
  )

  with pytest.raises(RuntimeError, match="fixed 42-input inventory differs"):
    verify_internal_provenance(tmp_path)


def test_v07_active_goldens_are_immutable(tmp_path):
  materialize_inputs(tmp_path)
  replace_once(
    tmp_path / "src/test/astro/sun_test.cpp",
    "62.035999382,  20.945745250",
    "62.035999383,  20.945745250",
  )

  with pytest.raises(RuntimeError, match="V07 active golden values or assertions differ"):
    verify_internal_provenance(tmp_path)


@pytest.mark.parametrize(
  ("old", "new", "message"),
  [
    (
      "if api_version is None or api_version.group(1) != HORIZONS_API_VERSION:",
      "if api_version is None:",
      "unexpected API version",
    ),
    (
      'if f"{{source: {HORIZONS_DE_SOURCE}}}" not in prelude:',
      'if "{source:" not in prelude:',
      "unexpected DE source",
    ),
  ],
  ids=["api-version", "de-source"],
)
def test_v07_identity_checks_run_before_values(tmp_path, old, new, message):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "statistics/sun_equatorial_horizons_crawler.py", old, new)

  with pytest.raises(RuntimeError, match=message):
    verify_internal_provenance(tmp_path)


@pytest.mark.parametrize(
  ("old", "new"),
  [
    (
      "if api_version is None or api_version.group(1) != HORIZONS_API_VERSION:",
      'if api_version is None or api_version.group(1) not in {HORIZONS_API_VERSION, "1.3"}:',
    ),
    (
      'if f"{{source: {HORIZONS_DE_SOURCE}}}" not in prelude:',
      'if not any(f"{{source: {source}}}" in prelude for source in (HORIZONS_DE_SOURCE, "DE442")):',
    ),
  ],
  ids=["api-version-whitelist", "de-source-whitelist"],
)
def test_v07_identity_whitelists_cannot_be_broadened(tmp_path, old, new):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "statistics/sun_equatorial_horizons_crawler.py", old, new)

  with pytest.raises(RuntimeError, match="V07 crawler active code or values differ"):
    verify_internal_provenance(tmp_path)


@pytest.mark.parametrize(
  ("old", "new"),
  [
    ("https://ssd.jpl.nasa.gov/api/horizons.api", "https://example.invalid/horizons"),
    ("params=horizons_params(), timeout=60", "params=horizons_params(), timeout=30"),
  ],
  ids=["endpoint", "timeout"],
)
def test_v07_request_target_is_complete(tmp_path, old, new):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "statistics/sun_equatorial_horizons_crawler.py", old, new)

  with pytest.raises(RuntimeError, match="endpoint or request shape differs"):
    verify_internal_provenance(tmp_path)


@pytest.mark.parametrize(
  ("old", "new"),
  [
    ("{rows[jde][0]:>13}, {rows[jde][1]:>13}", "{rows[jde][1]:>13}, {rows[jde][0]:>13}"),
    ('return "\\n".join(f"    {{ {jde:>14}', 'return "\\n".join(f"  {{ {jde:>14}'),
  ],
  ids=["column-order", "padding"],
)
def test_v07_emitted_table_format_is_exact(tmp_path, old, new):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "statistics/sun_equatorial_horizons_crawler.py", old, new)

  with pytest.raises(RuntimeError, match="emitted table format differs"):
    verify_internal_provenance(tmp_path)


def test_v11_usno_version_check_cannot_be_weakened(tmp_path):
  materialize_inputs(tmp_path)
  replace_once(
    tmp_path / "statistics/sunrise_golden_crawler.py",
    'if payload.get("apiversion") != USNO_API_VERSION:',
    'if "apiversion" not in payload:',
  )

  with pytest.raises(RuntimeError, match="is not rejected"):
    verify_internal_provenance(tmp_path)


def test_v11_request_shape_is_complete(tmp_path):
  materialize_inputs(tmp_path)
  replace_once(
    tmp_path / "statistics/sunrise_golden_crawler.py",
    '"coords": f"{lat},{lon}"',
    '"coords": f"{lon},{lat}"',
  )

  with pytest.raises(RuntimeError, match="USNO request shape differs"):
    verify_internal_provenance(tmp_path)


@pytest.mark.parametrize(
  ("old", "new"),
  [
    (
      '{  3, 20,   -0.22,   -78.51,  -5, "05:57", "06:18", "12:21", "18:25", "18:45", false, false },',
      '{  3, 20,   -0.22,   -78.51,  -5, "05:57", "06:18", "12:21", "18:25", "18:46", false, false },',
    ),
    ("    ASSERT_FALSE(astronomical.polar == Polar::NIGHT) << tag;\n", ""),
  ],
  ids=["active-value", "active-assertion"],
)
def test_v42_active_goldens_are_immutable(tmp_path, old, new):
  materialize_inputs(tmp_path)
  replace_once(tmp_path / "src/test/astro/rise_set_golden_test.cpp", old, new)

  with pytest.raises(RuntimeError, match="active golden values or assertions differ"):
    verify_internal_provenance(tmp_path)


def test_v11_crawler_active_values_are_immutable(tmp_path):
  materialize_inputs(tmp_path)
  replace_once(
    tmp_path / "statistics/sunrise_golden_crawler.py",
    '  ("Quito", -0.22, -78.51, -5, "America/Guayaquil"),\n',
    "",
  )

  with pytest.raises(RuntimeError, match="crawler active code or values differ"):
    verify_internal_provenance(tmp_path)
