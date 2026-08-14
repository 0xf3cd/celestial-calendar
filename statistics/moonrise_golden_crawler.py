# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

# Golden-dataset crawler for moonrise/moonset/lunar-transit tests (#62):
# - Source: USNO "Complete Sun and Moon Data for One Day" API (aa.usno.navy.mil/api/rstt/oneday),
#   queried per site at tz=0, so each response lists the events of one UT day — matching
#   `astro::rise_set::moon::calculate`'s UT-day window semantics 1:1, including empty cells
#   (UT dates without a moonrise are routine: lunar transits are ~24.84 h apart; the August
#   span below catches skipped events at 3 of 7 sites — Singapore/Beijing rise, Quito set).
# - The Tromso extra rows are lunar polar day/night dates (2026 is inside the major-standstill
#   season, so the Moon is circumpolar / never-rising at 69.65°N for several days each month —
#   the candidate dates were located with the library's own engine, then pinned against USNO
#   here), plus one DOUBLE-RISE day (2026-05-14): USNO lists two rises in the same cell, and
#   the library's one-event-per-cell contract reports the later one — the dict below keeps
#   last-wins on purpose and says so loudly when it happens.
# The script emits column-aligned C++ dataset rows to paste into
# src/test/astro/rise_set_moon_golden_test.cpp.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar


import sys

import requests


# (name, lat, lon) — the same city-centroid coordinates as sunrise_golden_crawler.py.
SITES = [
  ("Quito",     -0.22,  -78.51),
  ("Singapore",  1.35,  103.82),
  ("London",    51.50,   -0.13),
  ("Beijing",   39.90,  116.41),
  ("NewYork",   40.71,  -74.01),
  ("Sydney",   -33.87,  151.21),
  ("Tromso",    69.65,   18.96),
]

# Consecutive-day span per site, plus Tromso lunar-polar dates (DAY: circumpolar; NIGHT:
# never rises) and the double-rise day.
SPAN = [(2026, 8, d) for d in range(13, 18)]
TROMSO_EXTRA = [(2026, 8, 8), (2026, 8, 9), (2026, 8, 20), (2026, 8, 21), (2026, 5, 14)]

PHEN_KEYS = ["Rise", "Upper Transit", "Set"]


def fetch_usno_moon(lat: float, lon: float, y: int, m: int, d: int) -> dict:
  url = "https://aa.usno.navy.mil/api/rstt/oneday"
  params = {"date": f"{y:04d}-{m:02d}-{d:02d}", "coords": f"{lat},{lon}", "tz": "0"}
  resp = requests.get(url, params=params, headers={"User-Agent": "celestial-calendar-golden/0.1"}, timeout=30)
  resp.raise_for_status()
  payload = resp.json()
  moondata = payload["properties"]["data"]["moondata"]
  times: dict[str, list[str]] = {}
  for entry in moondata:
    times.setdefault(entry["phen"], []).append(entry["time"])
  for phen, vals in times.items():
    if len(vals) > 1:
      print(f"NOTE double event {y}-{m:02d}-{d:02d} ({lat},{lon}) {phen}: {vals} "
            f"— keeping the later one per the library's one-event-per-cell contract", file=sys.stderr)
  return {phen: vals[-1] for phen, vals in times.items()}


def main() -> None:
  print("// --- C++ rows (USNO rstt/oneday, tz=0; "" = event absent that UT day) ---")
  for (name, lat, lon) in SITES:
    dates = SPAN + (TROMSO_EXTRA if name == "Tromso" else [])
    for (y, m, d) in dates:
      rec = fetch_usno_moon(lat, lon, y, m, d)
      print(f"USNO {name} {y}-{m:02d}-{d:02d}: {rec}", file=sys.stderr)
      cells = ", ".join(f'"{rec.get(k) or "":5s}"' for k in PHEN_KEYS)
      print(f"  {{ {m:2d}, {d:2d}, {lat:7.2f}, {lon:8.2f}, {cells} }},  // {name}")


if __name__ == "__main__":
  main()
