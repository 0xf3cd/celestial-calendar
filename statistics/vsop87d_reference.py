# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Reproduce the seven Earth VSOP87D rows from an unpacked PyMeeus 0.5.12 source tree."""

import argparse
import hashlib
import sys
from datetime import datetime, timezone
from pathlib import Path
from unittest.mock import patch


SOURCE_SHA256 = "570018274f36727d44c7360a0cfb8ce7fb9e1bbf3db428fb9baecb7582109ea2"
EPOCHS = (2445701.1, 2451545.0, 2454359.1, 2454774.36215, 2460505.25, 2462597.96105, 2464080.5)


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--reference-root", type=Path, required=True)
  args = parser.parse_args()
  source = args.reference_root.resolve()
  modules = sorted((source / "pymeeus").glob("*.py"))
  digest = hashlib.sha256()
  for module in modules:
    digest.update(module.name.encode() + b"\0" + module.read_bytes())
  if len(modules) != 20 or digest.hexdigest() != SOURCE_SHA256:
    raise RuntimeError("The supplied PyMeeus tree does not match the pinned 0.5.12 source")

  sys.path.insert(0, str(source))
  import pymeeus.Coordinates as coordinates
  from pymeeus.Earth import VSOP87_L, VSOP87_B, VSOP87_R
  from pymeeus.Epoch import Epoch

  angle = coordinates.Angle
  rows = []
  radians = []
  for jde_tt in EPOCHS:
    radians.clear()

    def record_angle(value, *args, **kwargs):
      """Observe raw radians before Angle normalizes the unwrapped VSOP87D longitude."""
      if kwargs.get("radians") is True:
        radians.append(value)
      return angle(value, *args, **kwargs)

    with patch.object(coordinates, "Angle", record_angle):
      _, _, radius_au = coordinates.vsop_pos(Epoch(jde_tt), VSOP87_L, VSOP87_B, VSOP87_R)
    if len(radians) != 2:
      raise RuntimeError("Expected exactly longitude and latitude Angle constructions")
    rows.append([repr(value) for value in (jde_tt, *radians, radius_au)])

  widths = [max(len(row[index]) for row in rows) for index in range(4)]
  print(f"// PyMeeus 0.5.12; collected {datetime.now(timezone.utc).date()}; no random sampling.")
  print("// JDE(TT), unwrapped L (rad), B (rad), R (AU); no FK5 correction.")
  for row in rows:
    jde, longitude, latitude, radius = (value.rjust(width) for value, width in zip(row, widths, strict=True))
    print(f"  {{ {jde}, {{ {longitude}, {latitude}, {radius} }} }},")


if __name__ == "__main__":
  main()
