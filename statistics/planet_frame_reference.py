# CelestialCalendar:
#   Astronomical reference-data generation for the CelestialCalendar C++ project.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Generate geometric observation-date frame witnesses without production calls.

Run with --reference-root pointing to unmodified AA+ v2.55 and --check-file to
IMCCE's vsop87.chk. JSON on stdout includes source identities, complete-series
counts, printed substitution checks, both Cartesian constructions and old-frame
controls. No coefficients or external implementation source are emitted.

Meeus, Astronomical Algorithms, second edition, (21.5)/(21.7), pp.136-137,
and (33.1)-(33.4). The matrix and fixed-J2000 reconstruction share this model
and VSOP87D theory; they do not establish the exact dynamical precession law
used to generate VSOP87D, or a physical accuracy bound. Numerical JD is used
as VSOP dynamical time approximately equal to TT; no TT/TDB model is applied.
"""

import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import re

NAMES = ("EARTH", "MERCURY", "VENUS", "MARS", "JUPITER", "SATURN", "URANUS", "NEPTUNE")
CODES = ("EAR", "MER", "VEN", "MAR", "JUP", "SAT", "URA", "NEP")
AS2R = math.pi / (180 * 3600)
LIGHT_TIME_DAYS_PER_AU = 0.0057755183  # Meeus (33.3).
# Earliest 2025 station rows in the existing Horizons dataset, used only to select inputs.
STATIONS = (
  2460749.782868,
  2460736.525850,
  2460730.584059,
  2460710.902419,
  2460869.670848,
  2460706.182786,
  2460861.399610,
)


def mv(matrix, vector):
  return tuple(math.fsum(a * b for a, b in zip(row, vector, strict=True)) for row in matrix)


def transpose(matrix):
  return tuple(zip(*matrix, strict=True))


def mm(lhs, rhs):
  return tuple(tuple(math.fsum(a * b for a, b in zip(row, col, strict=True)) for col in transpose(rhs)) for row in lhs)


def rx(angle):
  c, s = math.cos(angle), math.sin(angle)
  return ((1, 0, 0), (0, c, -s), (0, s, c))


def rz(angle):
  c, s = math.cos(angle), math.sin(angle)
  return ((c, -s, 0), (s, c, 0), (0, 0, 1))


def ecliptic_matrix(initial, final):
  # Active column-vector form of printed (21.7), not the production spherical transform.
  T = (initial - 2451545.0) / 36525.0
  t = (final - initial) / 36525.0
  eta = ((47.0029 - 0.06603 * T + 0.000598 * T**2) * t + (-0.03302 + 0.000598 * T) * t**2 + 0.000060 * t**3) * AS2R
  Pi = (
    math.radians(174.876384) + (3289.4789 * T + 0.60622 * T**2 - (869.8089 + 0.50491 * T) * t + 0.03536 * t**2) * AS2R
  )
  p = ((5029.0966 + 2.22226 * T - 0.000042 * T**2) * t + (1.11113 - 0.000042 * T) * t**2 - 0.000006 * t**3) * AS2R
  return mm(rz(Pi + p), mm(rx(-eta), rz(-Pi)))


def rectangular(coordinate):
  lon, lat, radius = coordinate
  return (radius * math.cos(lat) * math.cos(lon), radius * math.cos(lat) * math.sin(lon), radius * math.sin(lat))


def spherical(vector):
  x, y, z = vector
  return (math.degrees(math.atan2(y, x)) % 360, math.degrees(math.atan2(z, math.hypot(x, y))), math.hypot(x, y, z))


def difference(lhs, rhs):
  return tuple(a - b for a, b in zip(lhs, rhs, strict=True))


def parse_tables(root, body):
  path = root / f"AAVSOP87D_{CODES[body]}.cpp"
  text = path.read_text(encoding="utf-8")
  arrays = {}
  parsed = []
  pattern = re.compile(
    rf"constexpr std::array<VSOP87Coefficient, (\d+)> (g_VSOP87D_([LBR])([0-5])_{NAMES[body]})"
    r"\s*\{\s*\{(.*?)\}\s*\};",
    re.S,
  )
  number = r"[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?"
  term = re.compile(rf"\{{\s*({number})\s*,\s*({number})\s*,\s*({number})\s*\}}\s*,?")
  for match in pattern.finditer(text):
    count, name, axis, power, contents = match.groups()
    terms = term.findall(contents)
    if len(terms) != int(count) or term.sub("", contents).strip():
      raise ValueError(f"Incomplete coefficient array: {path.name} {name}")
    key = (axis, int(power))
    if key in arrays:
      raise ValueError(f"Duplicate array: {name}")
    arrays[key] = tuple(tuple(map(float, triple)) for triple in terms)
    parsed.append(name)
  declared = re.findall(r"constexpr std::array<VSOP87Coefficient, \d+> (\w+)", text)
  if sorted(declared) != sorted(parsed):
    raise ValueError(f"Missing coefficient arrays: {path.name}")
  aggregate = re.compile(
    rf"constexpr std::array<VSOP87Coefficient2, (\d+)> g_VSOP87D_([LBR])_{NAMES[body]}"
    r"\s*\{\s*\{(.*?)\}\s*\};",
    re.S,
  )
  seen = set()
  for match in aggregate.finditer(text):
    count, axis, contents = match.groups()
    refs = re.findall(r"\{\s*(\w+)\.data\(\),\s*\1\.size\(\)\s*\}", contents)
    powers = sorted(p for a, p in arrays if a == axis)
    expected = [f"g_VSOP87D_{axis}{p}_{NAMES[body]}" for p in powers]
    if powers != list(range(int(count))) or refs != expected:
      raise ValueError(f"Incomplete aggregate: {path.name} {axis}")
    seen.add(axis)
  if seen != set("LBR"):
    raise ValueError(f"Missing coordinates: {path.name}")
  return arrays, {
    "file": path.name,
    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    "counts": {a: [len(arrays[a, p]) for p in sorted(p for axis, p in arrays if axis == a)] for a in "LBR"},
  }


def evaluate(tables, jd):
  t = (jd - 2451545.0) / 365250.0
  result = []
  for axis in "LBR":
    value = math.fsum(
      t**p * math.fsum(a * math.cos(b + c * t) for a, b, c in tables[axis, p])
      for p in sorted(p for a, p in tables if a == axis)
    )
    result.append(value % math.tau if axis == "L" else value)
  return tuple(result)


def light_time(target_at, observer, observation):
  emission = observation
  for count in range(1, 11):
    vector = difference(target_at(emission), observer)
    next_emission = observation - LIGHT_TIME_DAYS_PER_AU * math.hypot(*vector)
    if abs(next_emission - emission) <= math.ulp(observation):
      return {
        "vector_au": vector,
        "emission_jde_tt": emission,
        "iterations": count,
        "fixed_point_residual_days": next_emission - emission,
      }
    emission = next_emission
  raise RuntimeError(f"Independent light-time iteration failed at {observation}")


def directed_inputs():
  for body in range(1, 8):
    for year, role in ((1885, "early"), (2000, "J2000"), (2099, "late")):
      jd = 2451545.0 + (datetime.date(year, 1, 1) - datetime.date(2000, 1, 1)).days
      yield body, jd, role
    for offset in (-0.1, 0.1):
      yield body, STATIONS[body - 1] + offset, "station-before" if offset < 0 else "station-after"
  # Source-selected wrap neighbourhoods; the reference is geometric, not apparent.
  for jd in (2460737.0, 2460738.0, 2460764.0, 2460765.0):
    yield 1, jd, "Mercury-wrap"
  for jd in (2460764.0, 2460765.0):
    yield 7, jd, "Neptune-wrap"


def generate(root, check_file):
  documentation = (root / "AA+.htm").read_bytes()
  if "<title>AA+ v2.55</title>" not in documentation.decode("iso-8859-1"):
    raise ValueError("Reference root does not identify AA+ v2.55")
  tables, sources = zip(*(parse_tables(root, body) for body in range(8)), strict=True)
  check = check_file.read_text(encoding="ascii")
  anchors = []
  for match in re.finditer(r"VSOP87D\s+(\w+)\s+JD2451545\.0[^\n]*\n([^\n]*)", check):
    body = NAMES.index(match[1])
    expected = tuple(float(v) for v in re.findall(r"[lbr]\s+([-+]?\d*\.\d+)\s+(?:rad|au)", match[2]))
    actual = evaluate(tables[body], 2451545.0)
    errors = difference(actual, expected)
    if len(expected) != 3 or max(map(abs, errors)) > 5e-11:
      raise ValueError(f"Primary printed D substitution failed: {match[1]}")
    anchors.append({"body": match[1], "expected_rad_rad_au": expected, "actual": actual, "errors": errors})
  if len(anchors) != 8:
    raise ValueError("Incomplete primary D checks")
  example = spherical(
    mv(ecliptic_matrix(2451545.0, 1643074.5), rectangular((math.radians(149.48194), math.radians(1.76549), 1)))
  )
  if abs(example[0] - 118.704) > 0.0005 or abs(example[1] - 1.615) > 0.0005:
    raise ValueError("Meeus Example 21.c failed")
  rows = []
  for body, observation, role in directed_inputs():
    earth = rectangular(evaluate(tables[0], observation))
    R_observation = ecliptic_matrix(2451545.0, observation)

    def target(emission, body=body):
      return rectangular(evaluate(tables[body], emission))

    direct = light_time(
      lambda e, observation=observation: mv(ecliptic_matrix(e, observation), target(e)), earth, observation
    )
    fixed = light_time(
      lambda e: mv(transpose(ecliptic_matrix(2451545.0, e)), target(e)),
      mv(transpose(R_observation), earth),
      observation,
    )
    fixed["vector_au"] = mv(R_observation, fixed["vector_au"])
    old = light_time(target, earth, observation)
    rows.append(
      {
        "body": NAMES[body],
        "observation_jde_tt": observation,
        "role": role,
        "direct": direct,
        "fixed": fixed,
        "mixed": old,
        "geometric_deg_deg_au": spherical(direct["vector_au"]),
        "direct_minus_fixed_au": difference(direct["vector_au"], fixed["vector_au"]),
      }
    )
  return {
    "source_version": "AA+ v2.55 (2024-04-29), complete unfiltered VSOP87D",
    "documentation_sha256": hashlib.sha256(documentation).hexdigest(),
    "check_sha256": hashlib.sha256(check_file.read_bytes()).hexdigest(),
    "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    "sources": sources,
    "primary_J2000_D_anchors": anchors,
    "Meeus_21c": example,
    "rows": rows,
  }


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--reference-root", type=Path, required=True)
  parser.add_argument("--check-file", type=Path, required=True)
  args = parser.parse_args()
  print(json.dumps(generate(args.reference_root, args.check_file), indent=2))


if __name__ == "__main__":
  main()
