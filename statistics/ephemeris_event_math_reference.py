# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Generate libm premise witnesses with the optional, validation-only mpmath 1.3.0 oracle.

The generated C++ rows need no Python dependency at build or test time.
These finite witnesses can refute the error premise, not prove it globally.
"""

import argparse
import hashlib
import json
import math
import random
import re

from pathlib import Path

import mpmath as mp

from mpmath.libmp.backend import BACKEND


SEED = 20261019
FUNCTIONS = ("SIN", "COS", "TAN", "ATAN2", "ASIN", "HYPOT2", "HYPOT3", "POW")


def inputs() -> dict[str, list[tuple[float, float, float]]]:
  """Use fixed edge cases and seeded samples, all within the stated function domains."""
  rng = random.Random(SEED)
  angles = [0.0, -0.0, 0.1, -0.1, math.pi, -math.pi, math.pi / 2, -math.pi / 2, 2.0**40, -(2.0**40)]
  for turns in (1, 17, 65536, 2**36):
    center = float(turns * math.tau)
    angles.extend((math.nextafter(center, 0.0), center, math.nextafter(center, math.inf)))
  angles.extend(rng.uniform(-1.0, 1.0) * 2 ** rng.randrange(-40, 40) for _ in range(42))
  small = [0.0, -0.0, -1.0, 1.0, math.nextafter(1.0, 0.0), math.nextafter(-1.0, 0.0), 2.0**-64, -(2.0**-64)]
  small.extend(rng.uniform(-1.0, 1.0) for _ in range(56))
  result = {
    "SIN": [(x, 0.0, 0.0) for x in angles],
    "COS": [(x, 0.0, 0.0) for x in angles],
    "TAN": [(x, 0.0, 0.0) for x in small],
    "ASIN": [(x, 0.0, 0.0) for x in small],
  }
  pairs = [(0.0, -1.0, 0.0), (-0.0, -1.0, 0.0), (1.0, 0.0, 0.0), (-1.0, 0.0, 0.0)]
  triples = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (3.0, 4.0, 12.0), (2.0**20, 2.0**-40, -1.0)]
  for _ in range(60):
    coordinates = tuple(rng.uniform(-1.0, 1.0) * 2 ** rng.randrange(-40, 20) for _ in range(3))
    pairs.append((coordinates[0], coordinates[1], 0.0))
    triples.append(coordinates)
  result["ATAN2"] = pairs
  result["HYPOT2"] = pairs
  result["HYPOT3"] = triples
  powers = [(0.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 2.0, 0.0), (-0.0, 2.0, 0.0)]
  for index in range(60):
    value = rng.uniform(1.0, 2.0) * 2 ** rng.randrange(-64, 6)
    powers.append((value if index % 2 else -value, float(index % 3), 0.0))
  result["POW"] = powers
  assert all(len(result[name]) == 64 for name in FUNCTIONS)
  return result


def evaluate(name: str, args: tuple[float, float, float], digits: int) -> float:
  """Convert the actual binary64 inputs exactly, including atan2's signed-zero branch."""
  with mp.workdps(digits):
    x, y, z = (mp.mpf(v) for v in args)
    if name == "ATAN2" and args[0] == 0.0 and args[1] < 0.0:
      return float(-mp.pi if math.copysign(1.0, args[0]) < 0 else mp.pi)
    if name in ("SIN", "COS", "TAN", "ASIN"):
      return float(getattr(mp, name.lower())(x))
    if name == "ATAN2":
      return float(mp.atan2(x, y))
    if name == "HYPOT2":
      return float(mp.sqrt(x * x + y * y))
    if name == "HYPOT3":
      return float(mp.sqrt(x * x + y * y + z * z))
    if name == "POW":
      return float(x ** int(args[1]))
    raise ValueError(name)


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--output", type=Path, required=True)
  parser.add_argument("--pins-only", action="store_true", help="Write metadata for an already generated header")
  parser.add_argument(
    "--format-only", action="store_true", help="Add member designators without reevaluating references"
  )
  args = parser.parse_args()
  if mp.__version__ != "1.3.0" or BACKEND != "python":
    raise RuntimeError("Reference collection requires mpmath 1.3.0's Python backend")
  if args.pins_only:
    write_pins(args.output)
    return
  if args.format_only:
    header = args.output / "libm_reference.hpp"
    original = header.read_text()
    pattern = re.compile(r"^  \{ (\s*Function::\w+), \{ (.+) \}, (.+) \},$", re.M)
    formatted, count = pattern.subn(r"  { .function = \1, .inputs = { \2 }, .expected = \3 },", original)
    if count != 512:
      raise RuntimeError("Expected exactly 512 undesignated rows")
    header.write_text(formatted)
    write_pins(args.output)
    return
  rows = []
  for name, cases in inputs().items():
    for values in cases:
      low = evaluate(name, values, 80)
      high = evaluate(name, values, 112)
      if low.hex() != high.hex():
        raise RuntimeError(f"Reference precision disagreement for {name} {values}")
      rows.append((f"Function::{name}", *(value.hex() for value in values), high.hex()))
  widths = [max(len(row[column]) for row in rows) for column in range(5)]
  lines = [
    "#pragma once",
    "",
    "#include <array>",
    "#include <cstdint>",
    "",
    "namespace astro::ephemeris::test::libm_reference {",
    "",
    "enum class Function : uint8_t { " + ", ".join(FUNCTIONS) + " };",
    "",
    "struct Row {",
    "  Function function;",
    "  std::array<double, 3> inputs;",
    "  double expected;",
    "};",
    "",
    "// Function, three actual binary64 inputs, rounded high-precision reference.",
    f"inline constexpr std::array<Row, {len(rows)}> ROWS {{{{",
  ]
  for row in rows:
    padded = [value.rjust(widths[column]) for column, value in enumerate(row)]
    lines.append(
      f"  {{ .function = {padded[0]}, .inputs = {{ {padded[1]}, {padded[2]}, {padded[3]} }},"
      f" .expected = {padded[4]} }},"
    )
  lines += ["}};", "", "} // namespace astro::ephemeris::test::libm_reference", ""]
  args.output.mkdir(parents=True, exist_ok=True)
  header = args.output / "libm_reference.hpp"
  if header.exists():
    raise RuntimeError("Refusing to overwrite an existing reference dataset")
  header.write_text("\n".join(lines))
  write_pins(args.output)


def write_pins(output: Path) -> None:
  """Record source identities without replaying completed high-precision evaluations."""
  header = output / "libm_reference.hpp"
  if "std::array<Row, 512>" not in header.read_text():
    raise RuntimeError("Expected the complete 512-row dataset")
  package = Path(mp.__file__).parent
  source_files = ("ctx_mp_python.py", "libmp/libmpf.py", "libmp/libelefun.py", "functions/functions.py")
  pins = {name: hashlib.sha256((package / name).read_bytes()).hexdigest() for name in source_files}
  (output / "LIBM_REFERENCE_PINS.json").write_text(
    json.dumps(
      {
        "oracle": "mpmath 1.3.0",
        "backend": BACKEND,
        "seed": SEED,
        "collection_date": "2026-10-19",
        "precision_decimal_digits": [80, 112],
        "rounded_results_identical": True,
        "rows": 512,
        "source_module_sha256": pins,
        "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "header_sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
        "astronomical_evaluations": 0,
        "limitation": "Finite witnesses can reject the libm premises; they do not establish a uniform platform bound",
      },
      indent=2,
    )
    + "\n"
  )


if __name__ == "__main__":
  main()
