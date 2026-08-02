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

# This file downloads the golden dataset for the Moon geocentric-position tests (#94 / #65):
# - Primary source: JPL Horizons API (ssd.jpl.nasa.gov/api/horizons.api), Moon (301) seen from
#   the geocenter (500@399), ephemeris DE441. Times are given and returned in TT (TIME_TYPE=TT),
#   so no Horizons-side delta-T model enters the chain. Quantity 31 = ObsEcLon/ObsEcLat, the
#   IAU76/80 true-ecliptic-of-date apparent position (light-time + light deflection + stellar
#   aberration; planetary aberration for the Moon is ~0.7 arcsec, far below the model envelope);
#   quantity 20 = apparent range (AU) and range-rate (km/s).
# - Cross-check 1: Skyfield + JPL DE421, apparent ecliptic-of-date via frame_latlon — validates
#   the Horizons query semantics on every core-band epoch inside DE421's span (1899-2053).
#   Extended/far-band rows reuse the exact same, thus-validated, query pipeline.
# - Cross-check 2: pyerfa's eraMoon98 — SOFA's independent transcription of the same truncated
#   ELP2000-82B from Meeus ch.47 that src/astro/elp2000_82b.hpp implements. Compared on distance
#   only (frame-free): |moon98 - Horizons| measures the model-truncation envelope per band, so
#   C++ residuals can be attributed (transcription error vs inherent truncation error).
# Validation anchor: Meeus "Astronomical Algorithms" 2nd ed. Example 47.a (JD 2448724.5 TT);
# the book's apparent lon/lat/distance and eraMoon98 are compared against DE441 at that epoch.
# The script prints the agreement reports and emits the column-aligned C++ dataset rows to
# paste into src/test/astro/moon_horizons_golden_test.cpp.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar


import sys

import requests


# Horizons' own units-conversion constant (echoed in every response header).
AU_KM = 149597870.700

# Core band 1900-2100: 32 epochs stepped by 2283.25 days (~6.25 y). The step is deliberately
# incommensurate with both the anomalistic (27.5545 d) and synodic (29.5306 d) months, so the
# samples scan all lunar phases and radial velocities — including non-extremum epochs where
# |range-rate| is large enough for the longitude column to expose time-scale errors (#68).
# JD 2448724.5 is the Meeus Example 47.a anchor.
CORE_EPOCHS = [2415385.5 + k * 2283.25 for k in range(32)] + [2448724.5]

# ~501 / ~999 / ~1599 / ~2500 / ~3000 CE: truncation degradation away from the fitted era.
EXTENDED_EPOCHS = [1904000.5, 2086000.5, 2305000.5, 2634000.5, 2817000.5]

# JD 2^22 = 4194304 (~6771 CE) straddle — double-precision cliff regression guard (#76 context —
# the Newton cliff itself is a sun/jieqi concern, but the guard is cheap) — plus ~9420 CE.
FAR_EPOCHS = [4194303.5, 4194304.5, 5161700.5]

# Meeus Example 47.a apparent position (book p.342-343), for the anchor-epoch report.
BOOK_47A = (2448724.5, 133.167265, -3.229126, 368409.7)

# DE421 kernel coverage (1899-07-29 .. 2053-10-09), with one day of margin on each side.
DE421_SPAN = (2414993.5, 2471183.5)


def fetch_horizons(jds: list[float]) -> dict[float, dict]:
  url = "https://ssd.jpl.nasa.gov/api/horizons.api"
  params = {
    "format": "text",
    "COMMAND": "'301'",
    "OBJ_DATA": "'NO'",
    "MAKE_EPHEM": "'YES'",
    "EPHEM_TYPE": "'OBSERVER'",
    "CENTER": "'500@399'",
    "TLIST": "'" + " ".join(f"{jd:.6f}" for jd in jds) + "'",
    "TLIST_TYPE": "'JD'",
    "TIME_TYPE": "'TT'",
    "QUANTITIES": "'31,20'",
    "ANG_FORMAT": "'DEG'",
    "EXTRA_PREC": "'YES'",
    "CAL_FORMAT": "'BOTH'",
    "CSV_FORMAT": "'YES'",
  }
  resp = requests.get(url, params=params, timeout=60)
  resp.raise_for_status()
  text = resp.text

  # Audit prints + hard gates (plain raises — `assert` dies under -O): a re-run must fail
  # loudly if Horizons ever swaps the ephemeris or time scale, because the provenance pins
  # DE441/TT (#94) and a sub-arcsec ephemeris drift would pass the C++ tolerances silently.
  for line in text.splitlines():
    if line.startswith("API VERSION") or "{source:" in line:
      print(line.strip(), file=sys.stderr)
  if "Moon (301)" not in text or "{source: DE441}" not in text:
    raise RuntimeError("Horizons response is not Moon (301) on DE441")

  lines = text.splitlines()
  soe, eoe = lines.index("$$SOE"), lines.index("$$EOE")
  header = None
  for line in reversed(lines[:soe]):
    if "ObsEcLon" in line:
      header = [c.strip() for c in line.split(",")]
      break
  if header is None:
    raise RuntimeError("column header line not found")
  jd_i = next(i for i, name in enumerate(header) if "JD" in name)
  if "JDTT" not in header[jd_i]:
    raise RuntimeError(f"JD column is not on the TT scale: {header[jd_i]!r}")
  col = {name: header.index(name) for name in ["ObsEcLon", "ObsEcLat", "delta", "deldot"]}

  out = {}
  for line in lines[soe + 1:eoe]:
    cells = [c.strip() for c in line.split(",")]
    jd = float(cells[jd_i])
    out[jd] = {
      "date": cells[0],
      "lon": float(cells[col["ObsEcLon"]]),
      "lat": float(cells[col["ObsEcLat"]]),
      "r_km": float(cells[col["delta"]]) * AU_KM,
      "deldot": float(cells[col["deldot"]]),
    }
  missing = [jd for jd in jds if jd not in out]
  if missing:
    raise RuntimeError(f"epochs missing from Horizons response: {missing}")
  return out


def skyfield_report(rows: dict[float, dict]) -> None:
  import tempfile

  import skyfield
  from skyfield import api
  from skyfield.framelib import ecliptic_frame

  print(f"skyfield version {skyfield.__version__}", file=sys.stderr)
  load = api.Loader(tempfile.gettempdir() + "/skyfield-cache")
  eph = load("de421.bsp")
  ts = load.timescale()

  worst_ang, worst_r = 0.0, 0.0
  in_span = [jd for jd in sorted(rows) if DE421_SPAN[0] < jd < DE421_SPAN[1]]
  for jd in in_span:
    h = rows[jd]
    app = eph["earth"].at(ts.tt_jd(jd)).observe(eph["moon"]).apparent()
    lat, lon, dist = app.frame_latlon(ecliptic_frame)
    wrapped = abs(lon.degrees - h["lon"]) % 360.0
    d_lon = min(wrapped, 360.0 - wrapped) * 3600.0
    d_lat = abs(lat.degrees - h["lat"]) * 3600.0
    d_r = abs(dist.km - h["r_km"]) * 1000.0
    worst_ang, worst_r = max(worst_ang, d_lon, d_lat), max(worst_r, d_r)
    flag = "  <-- CHECK" if max(d_lon, d_lat) > 0.5 or d_r > 50.0 else ""
    print(f"{h['date']}  dlon={d_lon:6.3f}″  dlat={d_lat:6.3f}″  dr={d_r:6.1f} m{flag}")
  print(f"\nskyfield/DE421 cross-check on {len(in_span)} core epochs: "
        f"worst angle diff {worst_ang:.3f} arcsec, worst distance diff {worst_r:.1f} m")
  # Semantic-regression gate: the two sources agree to 0.23″ / 1.0 m today; anything past
  # 1″ / 10 m on a re-run means the query semantics (frame, time scale, correction level)
  # changed on one side, not that the ephemerides drifted.
  if worst_ang > 1.0 or worst_r > 10.0:
    raise RuntimeError("skyfield cross-check exceeded the semantic-regression gate (1 arcsec / 10 m)")


def erfa_report(rows: dict[float, dict]) -> None:
  from statistics import median

  import erfa
  import numpy as np

  print(f"pyerfa version {erfa.__version__}", file=sys.stderr)
  bands = [("CORE", CORE_EPOCHS), ("EXTENDED", EXTENDED_EPOCHS), ("FAR", FAR_EPOCHS)]
  for name, epochs in bands:
    diffs = []
    for jd in sorted(epochs):
      pv = erfa.moon98(2400000.5, jd - 2400000.5)
      e_km = float(np.linalg.norm(pv[0])) * AU_KM
      diffs.append(abs(e_km - rows[jd]["r_km"]))
      if jd == BOOK_47A[0]:
        print(f"  47.a anchor: moon98 r={e_km:.3f} km, book {BOOK_47A[3]} km "
              f"(transcription check, expect ~m-level)")
    print(f"|moon98 - Horizons| distance envelope, {name:8s}: "
          f"median {median(diffs):8.2f} km, worst {max(diffs):8.2f} km")


def emit_rows(rows: dict[float, dict], name: str, epochs: list[float]) -> None:
  print(f"\n// --- C++ rows: {name} ---")
  for jd in sorted(epochs):
    h = rows[jd]
    print(f"  {{ {jd:11.2f}, {h['lon']:12.7f}, {h['lat']:11.7f}, {h['r_km']:11.3f} }},"
          f"  // {h['date']} TT, rdot {h['deldot']:+.3f} km/s")


def main() -> None:
  jds = sorted(CORE_EPOCHS + EXTENDED_EPOCHS + FAR_EPOCHS)
  rows = fetch_horizons(jds)

  anchor = rows[BOOK_47A[0]]
  d_lon = (BOOK_47A[1] - anchor["lon"]) * 3600.0
  d_lat = (BOOK_47A[2] - anchor["lat"]) * 3600.0
  print(f'Meeus 47.a book vs Horizons/DE441: dlon={d_lon:+.2f}" dlat={d_lat:+.2f}" '
        f"dr={BOOK_47A[3] - anchor['r_km']:+.2f} km (model envelope at the anchor epoch)")

  fast = sum(1 for h in rows.values() if abs(h["deldot"]) >= 0.03)
  print(f"epochs with |range-rate| >= 0.03 km/s: {fast}/{len(rows)} "
        f"(non-extremum coverage, #68)")

  skyfield_report(rows)
  erfa_report(rows)

  emit_rows(rows, "CORE (1900-2100 + 47.a anchor)", CORE_EPOCHS)
  emit_rows(rows, "EXTENDED (~501/999/1599/2500/3000 CE)", EXTENDED_EPOCHS)
  emit_rows(rows, "FAR (JD 2^22 straddle ~6771 CE; ~9420 CE)", FAR_EPOCHS)


if __name__ == "__main__":
  main()
