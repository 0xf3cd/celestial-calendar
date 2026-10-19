# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Collect directed PyMeeus event references without calling production.

Supply the pinned, unmodified source tree. The one-day reference meshes
are count witnesses for these cases, not a whole-model isolation proof.
No upstream source is copied or emitted.
"""

import argparse
import hashlib
import json
import math
import sys

from collections.abc import Callable
from pathlib import Path


SOURCE_COMMIT = "c8c5d719ace57d00fa7f4ae93ffa82ef1a79cf92"
SOURCE_SHA256 = "af697956dfbb21908af6f5a0a07c6d6f57644c8681033f1f7d6654df0c0b0c67"


def bisect(function: Callable[[float], float], left: float, right: float) -> float:
  """Refine a strict crossing bracket to 2e-8 day or adjacent binary64 times."""
  fl, fr = function(left), function(right)
  if (fl < 0) == (fr < 0):
    raise RuntimeError("Reference bracket does not straddle zero")
  for _ in range(64):
    middle = (left + right) / 2
    if middle == left or middle == right:
      break
    fm = function(middle)
    if (fm < 0) == (fl < 0):
      left, fl = middle, fm
    else:
      right = middle
    if right - left < 2e-8:
      break
  return (left + right) / 2


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--reference-root", type=Path, required=True)
  args = parser.parse_args()
  source = args.reference_root.resolve()
  modules = sorted((source / "pymeeus").glob("*.py"))
  digest = hashlib.sha256()
  for path in modules:
    digest.update(path.name.encode())
    digest.update(b"\0")
    digest.update(path.read_bytes())
  if len(modules) != 20 or digest.hexdigest() != SOURCE_SHA256:
    raise RuntimeError("The supplied PyMeeus tree does not match the pinned source")
  sys.path.insert(0, str(source))
  from pymeeus.Coordinates import equatorial2ecliptical, true_obliquity
  from pymeeus.Epoch import Epoch
  from pymeeus.Mercury import Mercury
  from pymeeus.Moon import Moon
  from pymeeus.Sun import Sun

  anchor = Mercury.geocentric_position(Epoch(1992, 12, 20.0))
  ra_hours, dec_deg = float(anchor[0]) / 15, float(anchor[1])
  wrong_ra = abs(ra_hours - (16 + 33 / 60 + 59.3 / 3600)) >= 0.2 / 3600
  wrong_dec = abs(dec_deg + (20 + 53 / 60 + 31.6 / 3600)) >= 0.2 / 3600
  if wrong_ra or wrong_dec:
    raise RuntimeError("Meeus Example 33.a did not reproduce its printed equatorial coordinates")
  calls = 0

  def longitude(body: str, time: float) -> float:
    nonlocal calls
    calls += 1
    if calls > 5000:
      raise RuntimeError("Reference evaluation budget exhausted")
    epoch = Epoch(time)
    if body == "SUN":
      return float(Sun.apparent_geocentric_position(epoch)[0])
    if body == "MOON":
      return float(Moon.apparent_ecliptical_pos(epoch)[0])
    if body == "MERCURY":
      ra, dec, _ = Mercury.geocentric_position(epoch)
      return float(equatorial2ecliptical(ra, dec, true_obliquity(Epoch(time)))[0])
    raise ValueError(body)

  def crossings(body_a: str, body_b: str | None, target: float, start: float, end: float) -> dict:
    def angle(time: float) -> float:
      return longitude(body_a, time) - (0 if body_b is None else longitude(body_b, time))

    result = []
    left, a = start, math.remainder(angle(start) - target, 360)
    for index in range(1, math.ceil(end - start) + 1):
      right = min(end, start + index)
      b = a + math.remainder(angle(right) - target - a, 360)
      low, high = sorted((a, b))
      for branch in range(math.ceil(low / 360), math.floor(high / 360) + 1):
        level = 360 * branch

        def residual(time: float, level: float = level) -> float:
          return math.remainder(angle(time) - target - level, 360)

        result.append({"jde_tt": bisect(residual, left, right), "direction": "increasing" if b > a else "decreasing"})
      left, a = right, b
    return {"a": body_a, "b": body_b, "target": target, "start": start, "end": end, "events": result}

  def rate(time: float) -> float:
    h = 1 / 16
    coefficients = (5 / 6, -5 / 21, 5 / 84, -5 / 504, 1 / 1260)
    return math.fsum(
      c * math.remainder(longitude("MERCURY", time + i * h) - longitude("MERCURY", time - i * h), 360) / h
      for i, c in enumerate(coefficients, 1)
    )

  rows = [
    crossings("SUN", None, 0.0, 2460745.5, 2460755.5),
    crossings("MERCURY", None, 0.0, 2460735.5, 2460785.5),
    crossings("MOON", "SUN", 60.0, 2460676.5, 2460707.5),
    crossings("MOON", "SUN", -60.0, 2460676.5, 2460707.5),
    crossings("MERCURY", "SUN", 0.0, 2460755.5, 2460763.5),
  ]
  stationary = [bisect(rate, 2460748.5, 2460751.5), bisect(rate, 2460771.5, 2460774.5)]
  print(
    json.dumps(
      {
        "source_commit": SOURCE_COMMIT,
        "source_python_sha256": digest.hexdigest(),
        "source_module_count": len(modules),
        "anchor": {"jde_tt": float(Epoch(1992, 12, 20.0)), "ra_hours": ra_hours, "dec_deg": dec_deg},
        "rate": "11-point centered, h=1/16 TT day; not production seven-point h=1/8",
        "census_limit": "Independent physical reference meshes, not a proof of whole-provider root isolation",
        "frame_caveat": "Mercury uses retarded-epoch apparent reduction; inverse obliquity is at observation TT",
        "crossings": rows,
        "mercury_station_jde_tt": stationary,
        "position_evaluations": calls,
      },
      indent=2,
    )
  )


if __name__ == "__main__":
  main()
