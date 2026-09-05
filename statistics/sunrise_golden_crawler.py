# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

# This file is used to download the golden dataset for sunrise/sunset tests (#44):
# - Primary source: USNO "Complete Sun and Moon Data for One Day" API (aa.usno.navy.mil/api/rstt/oneday),
#   queried per site at its fixed standard-time offset (no DST), so each response lists the
#   events of one local civil day — matching `astro::rise_set::sun::calculate`'s local-day semantics.
# - Cross-check source: NOAA solcalc yearly tables (gml.noaa.gov/grad/solcalc/table.php),
#   which render sunrise/sunset/solar-noon in the site's IANA zone (DST applied per date);
#   zoneinfo converts them to the site's fixed standard offset before comparison.
#   CelestialCalendar acknowledges NOAA's Global Monitoring Laboratory (GML) for the
#   solar-calculator output used in this comparison.
# - Nautical/astronomical twilight golden values come from Skyfield (JPL DE421), validated here
#   against USNO on every overlapping quantity (rise/set/civil twilight) before its -12/-18
#   values are accepted.
# The script prints the agreement reports and emits the column-aligned C++ dataset rows to
# paste into src/test/astro/rise_set_golden_test.cpp.
# Retained material boundary (V11): the external output blocks addressed by this crawler remain under
# their source terms and outside the project MIT grant; this crawler is project-authored.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar


import re
import sys
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo

import requests


# (name, lat, lon, standard-time UTC offset in hours, IANA zone for the NOAA cross-check).
# Coordinates are city centroids rounded to 0.01° — they only need to be identical across
# sources and the C++ dataset, not to match any authority's database.
SITES = [
  ("Quito", -0.22, -78.51, -5, "America/Guayaquil"),
  ("Singapore", 1.35, 103.82, 8, "Asia/Singapore"),
  ("London", 51.50, -0.13, 0, "Europe/London"),
  ("Beijing", 39.90, 116.41, 8, "Asia/Shanghai"),
  ("NewYork", 40.71, -74.01, -5, "America/New_York"),
  ("Sydney", -33.87, 151.21, 10, "Australia/Sydney"),
  ("Tromso", 69.65, 18.96, 1, "Europe/Oslo"),
]

# 2026 equinoxes / solstices (sample dates; the exact instants are irrelevant here).
DATES = [(2026, 3, 20), (2026, 6, 21), (2026, 9, 23), (2026, 12, 21)]

PHEN_KEYS = ["Begin Civil Twilight", "Rise", "Upper Transit", "Set", "End Civil Twilight"]
USNO_API_VERSION = "4.0.1"


def parse_usno(payload: dict) -> dict:
  if payload.get("apiversion") != USNO_API_VERSION:
    raise RuntimeError(f"unexpected USNO API version: {payload.get('apiversion')}")
  sundata = payload["properties"]["data"]["sundata"]
  return {entry["phen"]: entry["time"] for entry in sundata}


def fetch_usno(lat: float, lon: float, y: int, m: int, d: int, tz: int) -> dict:
  url = "https://aa.usno.navy.mil/api/rstt/oneday"
  params = {"date": f"{y:04d}-{m:02d}-{d:02d}", "coords": f"{lat},{lon}", "tz": str(tz)}
  resp = requests.get(url, params=params, headers={"User-Agent": "celestial-calendar-golden/0.1"}, timeout=30)
  resp.raise_for_status()
  return parse_usno(resp.json())


def fetch_noaa_tables(lat: float, lon: float, year: int) -> list[list[list[str]]]:
  url = "https://gml.noaa.gov/grad/solcalc/table.php"
  resp = requests.get(url, params={"lat": lat, "lon": lon, "year": year}, timeout=30)
  resp.raise_for_status()
  tables = re.findall(r"<table class='table table-bordered'>(.*?)</table>", resp.text, re.S)
  assert len(tables) == 3, "expected sunrise / sunset / solar-noon tables"
  parsed = []
  for tbl in tables:
    rows = re.findall(r"<tr[^>]*>(.*?)</tr>", tbl, re.S)
    parsed.append(
      [[re.sub(r"<[^>]+>", "", c).strip() for c in re.findall(r"<t[dh][^>]*>(.*?)</t[dh]>", r, re.S)] for r in rows]
    )
  return parsed


def noaa_local_to_std(cell: str, y: int, m: int, d: int, zone: str, tz_std: int) -> float | None:
  """NOAA cell ("HH:MM" or "HH:MM:SS", local wall clock w/ DST) -> minutes-of-day in standard time."""
  if not cell or not re.fullmatch(r"\d{2}:\d{2}(:\d{2})?", cell):
    return None
  parts = [int(p) for p in cell.split(":")]
  h, mi, s = parts[0], parts[1], parts[2] if len(parts) == 3 else 0
  local = datetime(y, m, d, h, mi, s, tzinfo=ZoneInfo(zone))
  std = local.astimezone(timezone(timedelta(hours=tz_std)))
  return std.hour * 60 + std.minute + std.second / 60.0


def hm_to_min(t: str | None) -> float | None:
  if t is None:
    return None
  parts = [int(p) for p in t.split(":")]
  return parts[0] * 60 + parts[1] + (parts[2] / 60.0 if len(parts) > 2 else 0.0)


# (site name, year, month, day) rows for the nautical/astronomical twilight golden set.
TWILIGHT_CASES = [("London", 2026, 6, 21), ("London", 2026, 12, 21), ("Tromso", 2026, 12, 21)]


def skyfield_boundaries(lat: float, lon: float, tz: int, y: int, m: int, d: int) -> dict:
  """All dark_twilight_day boundary crossings within the local standard day, keyed by
  (from-state, to-state) using state indices 0..4 = night/astro/nautical/civil/day."""
  import tempfile
  from skyfield import api, almanac

  load = api.Loader(tempfile.gettempdir() + "/skyfield-cache")
  eph = load("de421.bsp")
  ts = load.timescale()
  f = almanac.dark_twilight_day(eph, api.wgs84.latlon(lat, lon))
  t0 = ts.utc(y, m, d, -tz, 0, 0)
  t1 = ts.utc(y, m, d, 24 - tz, 0, 0)
  times, events = almanac.find_discrete(t0, t1, f)
  out, prev = {}, f(t0).item()
  for t, e in zip(times, events, strict=True):
    utc = t.utc_datetime()
    minutes = (utc.hour * 60 + utc.minute + utc.second / 60.0 + tz * 60) % 1440
    out[(prev, e.item())] = minutes
    prev = e.item()
  return out


def min_to_hms(minutes: float | None) -> str:
  if minutes is None:
    return ""
  total = round(minutes * 60)
  return f"{total // 3600:02d}:{(total // 60) % 60:02d}:{total % 60:02d}"


def skyfield_report(usno: dict) -> None:
  import skyfield

  print(f"skyfield version {skyfield.__version__}", file=sys.stderr)
  worst = 0.0
  for name, lat, lon, tz, _zone in SITES:
    for y, m, d in DATES:
      bounds = skyfield_boundaries(lat, lon, tz, y, m, d)
      pairs = [
        ("Begin Civil Twilight", (2, 3)),
        ("Rise", (3, 4)),
        ("Set", (4, 3)),
        ("End Civil Twilight", (3, 2)),
      ]
      for phen, key in pairs:
        u, s = hm_to_min(usno[(name, m, d)].get(phen)), bounds.get(key)
        if u is None or s is None:
          if (u is None) != (s is None):
            print(f"MISMATCH presence {name} {m:02d}-{d:02d} {phen}: usno={u} skyfield={s}")
          continue
        delta = abs(u - s)
        delta = min(delta, 1440 - delta)
        worst = max(worst, delta)
        flag = "  <-- CHECK" if delta > 2.0 else ""
        print(f"{name:10s} {m:02d}-{d:02d} {phen:19s} usno={u:7.1f} skyfield={s:7.2f} d={delta:4.2f}min{flag}")
  print(f"\nworst |USNO - Skyfield| = {worst:.2f} min")

  print("\n// --- C++ twilight rows (nautical / astronomical, HH:MM:SS local standard) ---")
  site_by_name = {s[0]: s for s in SITES}
  for name, y, m, d in TWILIGHT_CASES:
    _, lat, lon, tz, _zone = site_by_name[name]
    b = skyfield_boundaries(lat, lon, tz, y, m, d)
    cells = ", ".join(
      f'"{min_to_hms(b.get(key)):8s}"'
      for key in [(1, 2), (2, 1), (0, 1), (1, 0)]  # naut begin/end, astro begin/end
    )
    print(f"  {{ {m:2d}, {d:2d}, {lat:7.2f}, {lon:8.2f}, {tz:3d}, {cells} }},  // {name}")


def main() -> None:
  usno: dict[tuple, dict] = {}
  for name, lat, lon, tz, _zone in SITES:
    for y, m, d in DATES:
      usno[(name, m, d)] = fetch_usno(lat, lon, y, m, d, tz)
      print(f"USNO {name} {y}-{m:02d}-{d:02d}: {usno[(name, m, d)]}", file=sys.stderr)

  # Cross-check against NOAA (sunrise / sunset / solar noon).
  worst = 0.0
  for name, lat, lon, tz, zone in SITES:
    tables = fetch_noaa_tables(lat, lon, DATES[0][0])
    for y, m, d in DATES:
      noaa_cells = []
      for rows in tables:  # sunrise, sunset, solar noon
        cell = next((r[m] for r in rows if r and r[0] == str(d)), "")
        noaa_cells.append(noaa_local_to_std(cell, y, m, d, zone, tz))
      pairs = [("Rise", noaa_cells[0]), ("Set", noaa_cells[1]), ("Upper Transit", noaa_cells[2])]
      for phen, noaa_min in pairs:
        usno_min = hm_to_min(usno[(name, m, d)].get(phen))
        if usno_min is None or noaa_min is None:
          if (usno_min is None) != (noaa_min is None):
            print(f"MISMATCH presence {name} {m:02d}-{d:02d} {phen}: usno={usno_min} noaa={noaa_min}")
          continue
        delta = abs(usno_min - noaa_min)
        delta = min(delta, 1440 - delta)
        worst = max(worst, delta)
        flag = "  <-- CHECK" if delta > 2.0 else ""
        print(
          f"{name:10s} {m:02d}-{d:02d} {phen:14s} usno={usno_min:7.1f} noaa={noaa_min:7.1f} d={delta:4.1f}min{flag}"
        )
  print(f"\nworst |USNO - NOAA| = {worst:.2f} min")

  # Emit C++ dataset rows (local standard time "HH:MM"; "" = event absent that day).
  print("\n// --- C++ rows ---")
  for name, lat, lon, tz, _zone in SITES:
    for _y, m, d in DATES:
      rec = usno[(name, m, d)]
      cells = ", ".join(f'"{rec.get(k) or "":5s}"' for k in PHEN_KEYS)
      above = "Object continuously above the Horizon" in rec
      below = "Object continuously below the Horizon" in rec
      flags = f"{str(above).lower():5s}, {str(below).lower():5s}"
      print(f"  {{ {m:2d}, {d:2d}, {lat:7.2f}, {lon:8.2f}, {tz:3d}, {cells}, {flags} }},  // {name}")

  skyfield_report(usno)


if __name__ == "__main__":
  main()
