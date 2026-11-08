# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Replay frozen independent calibration, or exact inputs through optional external oracles."""

import argparse
import csv
import ctypes
import hashlib
import io
import json
import math
from pathlib import Path
import re
import sys


RECEIPTS = Path(__file__).resolve().parents[1] / "src/test/provenance/observer"
AU_KM = 149597870.7


def reduce(geo, site, gast):
  """Replay the pre-observer Cartesian/ENU preflight, independent of production reduction.

  Meeus Chapters 11 and 40, horizontal projection (13.5)/(13.6), with NGA WGS84
  defining parameters. Inputs: RA/Dec/GAST and site lon/lat in degrees, range AU, height km.
  Returns RA/Dec, range, north-zero/east-positive azimuth and geometric altitude.
  """
  _, lon, lat, height = site
  ra, dec, distance = geo
  ra, dec, phi, theta = map(math.radians, (ra, dec, lat, gast + lon))
  flattening = 1 / 298.257223563
  eccentricity_squared = flattening * (2 - flattening)
  normal = 6378.137 / math.sqrt(1 - eccentricity_squared * math.sin(phi) ** 2)
  equatorial_radius = (normal + height) * math.cos(phi) / AU_KM
  polar = ((1 - eccentricity_squared) * normal + height) * math.sin(phi) / AU_KM
  x = distance * math.cos(dec) * math.cos(ra) - equatorial_radius * math.cos(theta)
  y = distance * math.cos(dec) * math.sin(ra) - equatorial_radius * math.sin(theta)
  z = distance * math.sin(dec) - polar
  east = -math.sin(theta) * x + math.cos(theta) * y
  north = -math.sin(phi) * (math.cos(theta) * x + math.sin(theta) * y) + math.cos(phi) * z
  up = math.cos(phi) * (math.cos(theta) * x + math.sin(theta) * y) + math.sin(phi) * z
  return (
    math.degrees(math.atan2(y, x)) % 360,
    math.degrees(math.atan2(z, math.hypot(x, y))),
    math.hypot(x, y, z),
    math.degrees(math.atan2(east, north)) % 360,
    math.degrees(math.atan2(up, math.hypot(east, north))),
  )


def replay_calibration(path: Path) -> list[dict]:
  """Recompute both frozen reductions from their original geocentric and GAST inputs.

  Native geocentric/GAST values came from base 4d9efd5, before observer.hpp existed.
  This replay does not recompute those providers or fit tolerances to production output.
  """
  data = json.loads(path.read_text())
  assert data["source_sha"] == "4d9efd52ed8ef83946208b1e2d6abe91deaeac01", "calibration base identity"
  result = []
  for row in data["cases"]:
    assert row["jd_ut1"] == row["jde_tt"] - (row["tt_minus_utc_seconds"] - row["dut1_seconds"]) / 86400
    replayed = dict(row)
    for key, source in (("horizons_reduced", "horizons_geocentric"), ("native_reduced", "library_geocentric")):
      actual = list(reduce(row[source], row["site"], row["gast_deg"]))
      assert actual == row[key], f"independent calibration {key}: target {row['target_code']} TT {row['jde_tt']}"
      replayed[key] = actual
    result.append(replayed)
  return result


def rows(text: str, name: str) -> list[list[float]]:
  block = re.search(rf"std::array<std::array<double, \d+>, \d+> {name} \{{\{{(.*?)\n\}}\}};", text, re.S)
  if block is None:
    raise ValueError(f"missing fixture {name}")
  return [[float(cell) for cell in line.split(",") if cell.strip()] for line in re.findall(r"\{([^{}]+)\}", block[1])]


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--fixture", type=Path, default=Path("src/test/astro/observer_reference_data.hpp"))
  parser.add_argument("--sofa-library", type=Path)
  parser.add_argument("--pymeeus-root", type=Path)
  parser.add_argument("--horizons-dir", type=Path)
  parser.add_argument("--calibration", type=Path, help="offline numeric receipt; defaults to the retained calibration")
  args = parser.parse_args()
  if args.calibration or not any((args.sofa_library, args.pymeeus_root, args.horizons_dir)):
    count = len(replay_calibration(args.calibration or RECEIPTS / "calibration-results.json"))
    print(f"PASS independent offline calibration: {count} rows, both frozen reductions identical")
  if not any((args.sofa_library, args.pymeeus_root, args.horizons_dir)):
    return
  text = args.fixture.read_text()
  if args.sofa_library:
    library = ctypes.CDLL(str(args.sofa_library.resolve()))
    oracle = library.iauGd2gce
    oracle.argtypes = [ctypes.c_double] * 5 + [ctypes.POINTER(ctypes.c_double)]
    oracle.restype = ctypes.c_int
    maximum = 0.0
    for lon, lat, height, *expected in rows(text, "SOFA"):
      result = (ctypes.c_double * 3)()
      assert oracle(6378137.0, 1 / 298.257223563, math.radians(lon), math.radians(lat), height, result) == 0
      maximum = max(maximum, *(abs(a - b) for a, b in zip(result, expected, strict=True)))
    assert maximum <= 5e-9, maximum
    print(f"PASS SOFA 72 exact-input vectors, max metres {maximum:.17g}")
  if args.pymeeus_root:
    pins = json.loads((RECEIPTS / "SOURCE_PINS.json").read_text())["pymeeus"]
    source = args.pymeeus_root / pins["source_file"]
    assert hashlib.sha256(source.read_bytes()).hexdigest() == pins["source_sha256"], "PyMeeus source identity"
    sys.path.insert(0, str(args.pymeeus_root.resolve()))
    from pymeeus.Angle import Angle
    from pymeeus.Earth import Earth

    maximum = 0.0
    for ra, dec, _distance, latitude, height, hour, oracle_distance, expected_ra, expected_dec in rows(text, "PYMEEUS"):
      actual_ra, actual_dec = Earth.parallax_correction(
        Angle(ra), Angle(dec), Angle(latitude), oracle_distance, Angle(hour), height
      )
      maximum = max(
        maximum, abs(math.remainder(float(actual_ra) - expected_ra, 360)), abs(float(actual_dec) - expected_dec)
      )
    assert maximum <= 1e-12, maximum
    print(f"PASS PyMeeus 128 controlled-distance reductions, max degrees {maximum:.17g}")
  if args.horizons_dir:
    pins = json.loads((RECEIPTS / "SOURCE_PINS.json").read_text())["horizons"]
    for response in pins["responses"]:
      path = args.horizons_dir / (response["id"] + ".txt")
      assert hashlib.sha256(path.read_bytes()).hexdigest() == response["response_sha256"], "Horizons response identity"
    names = ("sun", "moon", "mercury", "venus", "mars", "jupiter", "saturn", "uranus", "neptune", "pluto")
    count = 0
    for row in rows(text, "HORIZONS"):
      code, lon, _lat, _height, ut1, tt, _gast, *values = row
      suffix = "north" if lon < 0 else "south"
      raw = (args.horizons_dir / f"{names[int(code)]}-{suffix}.txt").read_text()
      before, after = raw.split("$$SOE")
      assert "Atmos refraction: NO (AIRLESS)" in before and "149597870.700" in raw
      header = next(line for line in before.splitlines() if "Date_________JDTT" in line)
      fields = [cell.strip() for cell in next(csv.reader([header]))]
      candidates = [
        dict(zip(fields, (cell.strip() for cell in cells), strict=True))
        for cells in csv.reader(io.StringIO(after.split("$$EOE")[0].strip()))
      ]
      source = next(candidate for candidate in candidates if float(candidate["Date_________JDTT"]) == tt)
      actual = [
        float(source[key])
        for key in ("R.A.__(a-app)", "DEC___(a-app)", "delta", "Azimuth_(a-app)", "Elevation_(a-app)")
      ]
      assert actual == values[3:], (actual, values[3:])
      tt_minus_utc = 64.184 if tt == 2451545.0 else 69.184
      assert ut1 == tt - (tt_minus_utc - float(source["UT1-UTC"])) / 86400
      geocenter = (args.horizons_dir / f"{names[int(code)]}-geocenter.txt").read_text()
      geo_before, geo_after = geocenter.split("$$SOE")
      geo_header = next(line for line in geo_before.splitlines() if "Date_________JDTT" in line)
      geo_fields = [cell.strip() for cell in next(csv.reader([geo_header]))]
      geo_rows = [
        dict(zip(geo_fields, (cell.strip() for cell in cells), strict=True))
        for cells in csv.reader(io.StringIO(geo_after.split("$$EOE")[0].strip()))
      ]
      geo = next(candidate for candidate in geo_rows if float(candidate["Date_________JDTT"]) == tt)
      assert [float(geo[key]) for key in ("R.A.__(a-app)", "DEC___(a-app)", "delta")] == values[:3]
      count += 1
    print(f"PASS Horizons {count} exact geocentric/topocentric inputs, Q49 UT1 and airless output rows")


if __name__ == "__main__":
  main()
