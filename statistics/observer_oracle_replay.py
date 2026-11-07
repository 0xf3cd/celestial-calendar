# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Replay the exact retained observer inputs through external SOFA/PyMeeus or Horizons responses."""

import argparse
import csv
import ctypes
import io
import math
from pathlib import Path
import re
import sys


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
  args = parser.parse_args()
  if not any((args.sofa_library, args.pymeeus_root, args.horizons_dir)):
    parser.error("select at least one external oracle")
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
