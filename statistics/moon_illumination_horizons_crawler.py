#!/usr/bin/env python3
#
# Crawl the JPL Horizons golden dataset for the Moon's illuminated fraction
# (src/test/astro/moon_phase_test.cpp, Illumination.HorizonsGoldenDataset).
#
#########################################################################################
#
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

import sys
import time
import random
import requests

from typing import Final

# Dataset shape: 120 seeded epochs uniform in [1900, 2100] (JDE 2415020.5 .. 2488068.5)
# plus the Meeus Example 48.a anchor. Quantity 10 is the disk's illuminated fraction in
# percent, geocenter observer, TT scale. Horizons sorts TLIST internally (2026-08-10:
# pairing rows to epochs by input order manufactures phantom gaps), so the epochs go out
# sorted and the row count is asserted. TLIST travels in the query string, so it goes in
# chunks: 121 JDs in one GET is a ~2.6 KB URL and the API answers 502.
SEED: Final[int] = 42
RANDOM_POINTS: Final[int] = 120
JDE_RANGE: Final[tuple[float, float]] = (2415020.5, 2488068.5)
EXAMPLE_48A_JDE: Final[float] = 2448724.5  # 1992-04-12 0h TT
CHUNK: Final[int] = 40


def fetch_illumination(jdes: list[float]) -> list[float]:
  """Horizons observer quantity 10 (Illu%) for the Moon, as a fraction in [0, 1]."""
  out: list[float] = []
  for start in range(0, len(jdes), CHUNK):
    chunk = jdes[start:start + CHUNK]
    params = {
      "format": "text",
      "COMMAND": "'301'",
      "OBJ_DATA": "'NO'",
      "MAKE_EPHEM": "'YES'",
      "EPHEM_TYPE": "'OBSERVER'",
      "CENTER": "'500@399'",
      "TLIST": "'" + " ".join(f"{jd:.6f}" for jd in chunk) + "'",
      "TLIST_TYPE": "'JD'",
      "TIME_TYPE": "'TT'",
      "QUANTITIES": "'10'",
      "CSV_FORMAT": "'YES'",
    }
    resp = requests.get("https://ssd.jpl.nasa.gov/api/horizons.api", params=params, timeout=60)
    resp.raise_for_status()
    text = resp.text

    # Hard gate, same shape as moon_horizons_crawler.py: a re-run must fail loudly if
    # Horizons ever swaps the ephemeris, because the provenance pins DE441/TT (#94).
    if "Moon (301)" not in text or "{source: DE441}" not in text:
      raise RuntimeError("Horizons response is not Moon (301) on DE441")

    lines = text.splitlines()
    soe, eoe = lines.index("$$SOE"), lines.index("$$EOE")
    rows = lines[soe + 1:eoe]
    if len(rows) != len(chunk):
      raise RuntimeError(f"expected {len(chunk)} rows, got {len(rows)}")
    out.extend(float(line.split(",")[-2]) / 100.0 for line in rows)
    time.sleep(0.15)  # polite pacing, same as the USNO crawlers

  return out


def main() -> None:
  random.seed(SEED)
  jdes = sorted(round(random.uniform(*JDE_RANGE), 6) for _ in range(RANDOM_POINTS))
  jdes = sorted(jdes + [EXAMPLE_48A_JDE])

  for jd, illum in zip(jdes, fetch_illumination(jdes), strict=True):
    print(f"    {{ {jd:.6f}, {illum:.7f} }},")


if __name__ == "__main__":
  sys.exit(main())
