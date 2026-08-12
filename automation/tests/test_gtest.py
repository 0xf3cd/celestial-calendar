# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

from automation.gtest import parse_ctest_listing


# Sample rows verbatim from a real `ctest -N` listing (build/test, collected 2026-08-12):
# "  Test   #N:" single-digit rows carry three spaces, "  Test  #70" carries two -- the
# padding aligns on the widest id, and the parser keys keep it all.
CANONICAL = """\
Test project $REPO/build/test
  Test   #1: CoordTransform.EclipticToEquatorialMeeus13a
  Test   #2: CoordTransform.EclipticToEquatorialPoles
  Test  #70: Moon.CoordAndPpi
Total Tests: 3
"""


def test_canonical_listing():
  # Keys keep ctest's padding ("Test   #1" vs "Test  #70"): find_gtests' numeric match
  # relies on rpartition("#"), not on a cleaned-up key.
  assert parse_ctest_listing(CANONICAL) == {
    "Test   #1": "CoordTransform.EclipticToEquatorialMeeus13a",
    "Test   #2": "CoordTransform.EclipticToEquatorialPoles",
    "Test  #70": "Moon.CoordAndPpi",
  }


def test_non_entry_lines_are_ignored():
  # "Test" without "#" (header), "Tests" footer, "#" without "Test" -- none of them
  # name a test.
  assert parse_ctest_listing("Test project /x\nTotal Tests: 3\n#1: notatest\n") == {}


def test_empty_listing():
  assert parse_ctest_listing("") == {}
