# This file downloads the golden datasets for the Sun-position and jieqi tests (#94 / #68):
# - Axis 1, Sun apparent position: JPL Horizons API, Sun (10) from the geocenter (500@399),
#   DE441, TT in and out. Quantity 31 = IAU76/80 true-ecliptic-of-date apparent lon/lat —
#   the same observable as `astro::sun::geocentric_coord::apparent` (which models FK5 +
#   nutation + aberration). Geometric distance comes from a separate VECTORS query (|r| is
#   frame-free; the observer table's `delta` is light-time aberrated, ~250 km off geometric
#   for the Sun). VECTORS times are TDB; |TDB-TT| < 2 ms moves r by < 1e-9 AU — negligible.
# - Axis 2, jieqi instants from DE441: the crawler solves apparent-longitude crossings of
#   k*15 deg by batched fixed-slope (mean-motion) Newton iteration on Horizons quantity 31
#   (converges < 0.05 s). Pure TT — no delta-T model on either side of the comparison.
# - Axis 3, jieqi instants from the official HKO almanac (hko.gov.hk 24SolarTerms_{Y}.xml,
#   HKT = UTC+8, minute precision, computed by HMNAO/USNO — an independently PUBLISHED
#   product and processing pipeline, though modern almanac ephemerides derive from the same
#   JPL DE family; this is a chain cross-check, not an independent dynamical theory).
#   Available years: 2022-2028 only. Cross-validated against the axis-2 DE441 crossings
#   (TT -> UTC via the fixed 69.184 s valid over 2017-2028, no leap second scheduled), with
#   a hard gate: nothing is emitted unless the two pipelines agree (#94).
# Validation anchor: Meeus Example 25.b (JD 2448908.5 TT): book apparent lon 199.9060606 deg
# vs DE441 199.9059841 (0.275 arcsec = the book chain's own truncation scale).
# The script prints the agreement reports and emits the column-aligned C++ rows for
# src/test/astro/sun_horizons_golden_test.cpp and src/test/jieqi_golden_test.cpp.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar


import math
import re
import sys
import time

import requests

HORIZONS = "https://ssd.jpl.nasa.gov/api/horizons.api"

# Mean motion of the Sun in apparent longitude, deg/day — only used to seed/steer the
# crossing solver; the converged instants do not depend on it.
MEAN_MOTION = 360.0 / 365.2422

# --- Axis 1 epochs (same banding philosophy as moon_horizons_crawler.py) ---
# Core band 1901-2094 (32 epochs stepped 2283.25 days) + the Meeus 25.b anchor.
SUN_CORE_EPOCHS = [2415385.5 + k * 2283.25 for k in range(32)] + [2448908.5]
# ~501/999/1599/2500/3000 CE, plus the lunar-algo2 boundary years ~410 and ~5001 (#94).
SUN_EXTENDED_EPOCHS = [1870800.5, 1904000.5, 2086000.5, 2305000.5, 2634000.5, 2817000.5, 3547660.5]
# JD 2^22 straddle (~6771 CE, #76 cliff guard) and ~9420 CE.
SUN_FAR_EPOCHS = [4194303.5, 4194304.5, 5161700.5]

# --- Axis 2 sampling: (year, [solar longitudes]) ---
ALL_24 = [(285 + 15 * i) % 360 for i in range(24)]  # calendar order from XiaoHan
SEASONAL = [0, 90, 180, 270]
JIEQI_SAMPLES = (
  [(2026, ALL_24)]
  + [(y, SEASONAL) for y in (1900, 1950, 2000, 2050, 2100)]
  + [(y, SEASONAL) for y in (410, 1000, 1600, 3000, 5001)]
  + [(y, SEASONAL) for y in (6771, 6772, 9420)]
)

# --- Axis 3: HKO almanac coverage (probed 2026-07-27: 2022-2028 exist, others 404) ---
HKO_YEARS = list(range(2022, 2029))
TT_MINUS_UTC = 69.184 / 86400.0  # days; 32.184 + 37 (TAI-UTC), constant over 2017-2028.

BOOK_25B = (2448908.5, 199.9060606)


# Batch size per request: ~170-epoch TLISTs have hit 502s (URL ~3 KB); 80 stays well clear.
TLIST_CHUNK = 80


def horizons_get(params: dict) -> str:
  last_error: Exception | None = None
  for attempt in range(4):
    if attempt:
      time.sleep(5.0 * attempt)
    try:
      resp = requests.get(HORIZONS, params=params, timeout=120)
      resp.raise_for_status()
    except requests.HTTPError as exc:
      if exc.response is not None and 400 <= exc.response.status_code < 500:
        raise  # client error — retrying identical parameters cannot succeed
      last_error = exc
      print(f"horizons_get retry {attempt + 1}: {exc}", file=sys.stderr)
      continue
    except requests.RequestException as exc:  # noqa: PERF203 — retry loop
      last_error = exc
      print(f"horizons_get retry {attempt + 1}: {exc}", file=sys.stderr)
      continue
    text = resp.text
    if "{source: DE441}" not in text:
      raise RuntimeError("Horizons response is not on DE441")
    return text
  raise RuntimeError(f"Horizons request failed after retries: {last_error}")


def parse_csv_block(text: str) -> tuple[list[str], list[list[str]]]:
  lines = text.splitlines()
  soe, eoe = lines.index("$$SOE"), lines.index("$$EOE")
  header = None
  for line in reversed(lines[:soe]):
    if "," in line and not line.startswith("**"):
      header = [c.strip() for c in line.split(",")]
      break
  if header is None:
    raise RuntimeError("column header line not found")
  rows = [[c.strip() for c in line.split(",")] for line in lines[soe + 1:eoe]]
  return header, rows


def fetch_observer(command: str, jds: list[float]) -> dict[float, dict]:
  """Quantity 31 (+20 for the range-rate audit column) at the given TT JDs."""
  if len(jds) > TLIST_CHUNK:
    out = {}
    for i in range(0, len(jds), TLIST_CHUNK):
      out.update(fetch_observer(command, jds[i:i + TLIST_CHUNK]))
    return out
  text = horizons_get({
    "format": "text", "COMMAND": f"'{command}'", "OBJ_DATA": "'NO'", "MAKE_EPHEM": "'YES'",
    "EPHEM_TYPE": "'OBSERVER'", "CENTER": "'500@399'",
    "TLIST": "'" + " ".join(f"{jd:.9f}" for jd in jds) + "'",
    "TLIST_TYPE": "'JD'", "TIME_TYPE": "'TT'", "QUANTITIES": "'31,20'",
    "ANG_FORMAT": "'DEG'", "EXTRA_PREC": "'YES'", "CAL_FORMAT": "'BOTH'", "CSV_FORMAT": "'YES'",
  })
  if f"({command})" not in text:
    raise RuntimeError(f"observer response is not for body {command}")
  header, rows = parse_csv_block(text)
  jd_i = next(i for i, name in enumerate(header) if "JD" in name)
  if "JDTT" not in header[jd_i]:
    raise RuntimeError(f"JD column is not on the TT scale: {header[jd_i]!r}")
  col = {name: header.index(name) for name in ["ObsEcLon", "ObsEcLat", "deldot"]}
  parsed = {}
  for cells in rows:
    parsed[float(cells[jd_i])] = {
      "date": cells[0],
      "lon": float(cells[col["ObsEcLon"]]),
      "lat": float(cells[col["ObsEcLat"]]),
      "deldot": float(cells[col["deldot"]]),
    }
  return match_to_requested(jds, parsed, "observer")


def fetch_vectors_r(command: str, jds: list[float]) -> dict[float, float]:
  """Geometric distance |r| in AU from a VECTORS query (times interpreted as TDB)."""
  if len(jds) > TLIST_CHUNK:
    out = {}
    for i in range(0, len(jds), TLIST_CHUNK):
      out.update(fetch_vectors_r(command, jds[i:i + TLIST_CHUNK]))
    return out
  text = horizons_get({
    "format": "text", "COMMAND": f"'{command}'", "OBJ_DATA": "'NO'", "MAKE_EPHEM": "'YES'",
    "EPHEM_TYPE": "'VECTORS'", "CENTER": "'500@399'",
    "TLIST": "'" + " ".join(f"{jd:.9f}" for jd in jds) + "'",
    "TLIST_TYPE": "'JD'", "VEC_TABLE": "'1'", "OUT_UNITS": "'AU-D'", "CSV_FORMAT": "'YES'",
  })
  if f"({command})" not in text:
    raise RuntimeError(f"vectors response is not for body {command}")
  header, rows = parse_csv_block(text)
  jd_i = next(i for i, name in enumerate(header) if "JDTDB" in name)
  cols = {name: header.index(name) for name in ["X", "Y", "Z"]}
  parsed = {}
  for cells in rows:
    x, y, z = (float(cells[cols[k]]) for k in ("X", "Y", "Z"))
    parsed[float(cells[jd_i])] = math.sqrt(x * x + y * y + z * z)
  return match_to_requested(jds, parsed, "vectors")


def match_to_requested(jds: list[float], parsed: dict[float, object], what: str) -> dict:
  """Key `parsed` (echoed-JD -> payload) by the originally requested JDs.
  Horizons echoes JDs rounded to 9 decimals, which does not round-trip for arbitrary
  reals, so exact float keys fail; nearest-match within 2e-6 d (~0.17 s) is unambiguous
  because no two requested epochs are ever that close."""
  out = {}
  for pjd, payload in parsed.items():
    nearest = min(jds, key=lambda j: abs(j - pjd))
    if abs(nearest - pjd) > 2e-6:
      raise RuntimeError(f"{what}: echoed epoch {pjd} matches no requested epoch")
    out[nearest] = payload
  missing = [jd for jd in jds if jd not in out]
  if missing:
    raise RuntimeError(f"{what}: epochs missing from response: {missing}")
  return out


def wrapped_deg(d: float) -> float:
  """Wrap a longitude difference to (-180, 180]."""
  return (d + 180.0) % 360.0 - 180.0


def approx_jieqi_jd(year: int, lon: float) -> float:
  """Seed the crossing of apparent longitude `lon` that falls inside calendar year `year`.
  Year boundaries here are proleptic-Gregorian civil midnights taken as TT JDs; the C++
  window (`get_start_jde`) is UT1-based — a delta-T-scale difference (minutes at most),
  irrelevant for jieqi that sit days from any year boundary. The lon >= 285 block
  (XiaoHan..JingZhe) belongs to the solar cycle begun the PREVIOUS March, landing in
  Jan-Mar of `year`."""
  equinox = 2451623.8 + (year - 2000) * 365.2422
  jd = equinox + (lon % 360.0) / MEAN_MOTION
  if jd >= jd_from_civil(year + 1, 1, 1, 0, 0):
    jd -= 365.2422
  if jd < jd_from_civil(year, 1, 1, 0, 0):
    jd += 365.2422
  return jd


def solve_crossings(targets: list[tuple[int, float]]) -> dict[tuple[int, float], dict]:
  """Refine DE441 apparent-longitude crossings by batched fixed-slope Newton iteration
  (slope = MEAN_MOTION; the converged instants do not depend on the slope)."""
  est = {t: approx_jieqi_jd(*t) for t in targets}
  for it in range(6):
    jds = sorted(set(est.values()))
    obs = fetch_observer("10", jds)
    worst = 0.0
    for t, jd in est.items():
      diff = wrapped_deg(t[1] - obs[jd]["lon"])
      worst = max(worst, abs(diff))
      est[t] = jd + diff / MEAN_MOTION
    print(f"crossing iteration {it}: worst |lon diff| = {worst:.6f} deg", file=sys.stderr)
    if worst * 86400.0 / MEAN_MOTION < 0.05:  # < 0.05 s across the board
      break
  else:
    raise RuntimeError("crossing solver did not converge in 6 iterations")
  final = fetch_observer("10", sorted(set(est.values())))
  for (year, lon), jd in est.items():
    # Re-verify the CONVERGED instants against the final fetch (the loop's 0.05 s check
    # measured the residual before the last linear correction).
    resid_s = abs(wrapped_deg(lon - final[jd]["lon"])) * 86400.0 / MEAN_MOTION
    if resid_s > 0.05:
      raise RuntimeError(f"crossing ({year}, {lon}) final residual {resid_s:.3f} s > 0.05 s")
    # Containment gate: every crossing must land inside its calendar year, else the seed
    # picked the wrong solar cycle (this exact bug produced 1-year offsets on lon >= 285
    # in the first run — caught by the HKO cross-validation).
    if not jd_from_civil(year, 1, 1, 0, 0) <= jd < jd_from_civil(year + 1, 1, 1, 0, 0):
      raise RuntimeError(f"crossing ({year}, {lon}) landed outside its calendar year: JD {jd}")
  return {t: {"jde": jd, "date": final[jd]["date"]} for t, jd in est.items()}


def fetch_hko(year: int) -> list[tuple[int, int, int, int]]:
  """(month, day, hour, minute) in HKT for the 24 entries of `year`, calendar order."""
  url = f"https://www.hko.gov.hk/en/gts/astronomy/data/files/24SolarTerms_{year}.xml"
  resp = requests.get(url, timeout=60)
  resp.raise_for_status()
  raw = re.findall(r"<M>(\d+)</M><D>(\d+)</D><hm>(\d+):(\d+)</hm>", resp.text)
  if len(raw) != 24:
    raise RuntimeError(f"HKO {year}: expected 24 entries, got {len(raw)}")
  entries = [(int(m), int(d), int(h), int(mi)) for (m, d, h, mi) in raw]
  # Order hygiene: entry 0 must be January's XiaoHan and the list must be in calendar
  # order — the C++ rows pair entries with longitudes purely by index.
  if entries[0][0] != 1:
    raise RuntimeError(f"HKO {year}: first entry is not in January: {entries[0]}")
  month_days = [(m, d) for (m, d, _h, _mi) in entries]
  if month_days != sorted(month_days):
    raise RuntimeError(f"HKO {year}: entries are not in calendar order")
  return entries


def jd_from_civil(year: int, month: int, day: int, hour: int, minute: int) -> float:
  """Proleptic-Gregorian civil date-time -> JD (Fliegel-Van Flandern), for report math only."""
  a = (14 - month) // 12
  y = year + 4800 - a
  m = month + 12 * a - 3
  jdn = day + (153 * m + 2) // 5 + 365 * y + y // 4 - y // 100 + y // 400 - 32045
  return jdn - 0.5 + (hour + minute / 60.0) / 24.0


def emit_sun_rows(obs: dict[float, dict], radii: dict[float, float],
                  name: str, epochs: list[float]) -> None:
  print(f"\n// --- sun rows: {name} ---")
  for jd in sorted(epochs):
    h = obs[jd]
    print(f"  {{ {jd:11.2f}, {h['lon']:12.7f}, {h['lat']:11.7f}, {radii[jd]:13.10f} }},"
          f"  // {h['date']} TT, rdot {h['deldot']:+.4f} km/s")


def main() -> None:
  # ---- Axis 1: Sun apparent lon/lat + geometric r ----
  sun_jds = sorted(SUN_CORE_EPOCHS + SUN_EXTENDED_EPOCHS + SUN_FAR_EPOCHS)
  obs = fetch_observer("10", sun_jds)
  radii = fetch_vectors_r("10", sun_jds)

  anchor = obs[BOOK_25B[0]]
  print(f"Meeus 25.b book vs Horizons/DE441: dlon={(BOOK_25B[1] - anchor['lon']) * 3600:+.3f}″ "
        f"(book chain truncation scale)")

  # ---- Axis 2: DE441 jieqi crossings ----
  targets = [(y, float(lon)) for (y, lons) in JIEQI_SAMPLES for lon in lons]
  crossings = solve_crossings(targets)

  # ---- Axis 3: HKO almanac + cross-validation against axis 2 ----
  hko = {y: fetch_hko(y) for y in HKO_YEARS}
  hko_targets = [(y, float(lon)) for y in HKO_YEARS for lon in ALL_24]
  hko_crossings = solve_crossings(hko_targets)
  worst_min = 0.0
  for y in HKO_YEARS:
    for i, lon in enumerate(ALL_24):
      m, d, hh, mi = hko[y][i]
      hko_jd_utc = jd_from_civil(y, m, d, hh, mi) - 8.0 / 24.0
      de_jd_utc = hko_crossings[(y, float(lon))]["jde"] - TT_MINUS_UTC
      diff_min = abs(hko_jd_utc - de_jd_utc) * 1440.0
      worst_min = max(worst_min, diff_min)
      if diff_min > 1.0:
        print(f"CHECK HKO {y} lon={lon}: |HKO - DE441| = {diff_min:.2f} min")
  print(f"HKO(2022-2028, 168 values) vs DE441 crossings: worst {worst_min:.2f} min "
        f"(expect <= ~0.5 min rounding + chain difference)")
  # Hard acceptance gate (#94): nothing is emitted unless the two independent pipelines
  # agree. Threshold 0.6 min = HKO's ±0.5 min rounding bound plus a small chain allowance
  # (measured worst 0.51); anything larger means real drift, not rounding. The first run's
  # one-year seed bug was only visible in this report — a report nobody re-reads on re-runs.
  if worst_min > 0.6:
    raise RuntimeError(f"HKO vs DE441 cross-validation gate failed: worst {worst_min:.2f} min")

  # ---- Emit C++ rows ----
  emit_sun_rows(obs, radii, "CORE (1900-2100 + 25.b anchor)", SUN_CORE_EPOCHS)
  emit_sun_rows(obs, radii, "EXTENDED (~410/501/999/1599/2500/3000/5001 CE)", SUN_EXTENDED_EPOCHS)
  emit_sun_rows(obs, radii, "FAR (JD 2^22 straddle ~6771 CE; ~9420 CE)", SUN_FAR_EPOCHS)

  print("\n// --- jieqi DE441 crossing rows (year, target lon deg, JDE TT) ---")
  for (y, lon) in targets:
    c = crossings[(y, lon)]
    print(f"  {{ {y:5d}, {lon:5.1f}, {c['jde']:18.9f} }},  // {c['date']} TT")

  print("\n// --- jieqi HKO rows (year, entry idx 0=XiaoHan(285deg), month, day, hour, minute; HKT) ---")
  for y in HKO_YEARS:
    for i, (m, d, hh, mi) in enumerate(hko[y]):
      print(f"  {{ {y}, {i:2d}, {m:2d}, {d:2d}, {hh:2d}, {mi:2d} }},")


if __name__ == "__main__":
  main()
