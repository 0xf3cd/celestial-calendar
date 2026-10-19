# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Check source identities and emit the exact event-bound records as C++.

This checks transcription and directed binary64 serialization, not the
coefficient-majorant derivation or a deployment's elementary functions.
"""

import argparse
import hashlib
import json
import math

from fractions import Fraction
from pathlib import Path


TARGETS = (
  "SUN",
  "MOON",
  "MERCURY",
  "VENUS",
  "MARS",
  "JUPITER",
  "SATURN",
  "URANUS",
  "NEPTUNE",
  "PLUTO",
  "MEAN_ASCENDING",
  "MEAN_DESCENDING",
  "TRUE_ASCENDING",
  "TRUE_DESCENDING",
)
RECORD_PATH = "src/test/provenance/ephemeris-events/BOUND_RECORDS.json"
HEADER_PATH = "src/astro/ephemeris_event_bounds.hpp"


def directed(record: dict, lower: bool = False) -> str:
  """Require the literal to enclose its exact rational in the intended signed direction."""
  value = float.fromhex(record["hex"])
  if not math.isfinite(value):
    raise ValueError("Non-finite event bound")
  exact = Fraction(record["exact"])
  wrong_direction = Fraction(value) > exact if lower else Fraction(value) < exact
  if wrong_direction:
    raise ValueError("Event bound points in the wrong rounding direction")
  return record["hex"]


def emit(record: dict) -> str:
  """Render all fourteen records in their fixed Target order without changing mathematical values."""
  if (
    record["schema"] != 1
    or tuple(record["target_order"]) != TARGETS
    or record["epsilon_hex"] != "0x1.0000000000000p-48"
  ):
    raise ValueError("Unsupported event-bound schema, target order or elementary-function premise")
  lines = [
    "/*",
    " * CelestialCalendar:",
    " *   A C++23-style library that performs astronomical calculations and date conversions between",
    " *   Gregorian and Chinese Lunar calendars.",
    " *",
    " * Copyright (C) 2026 Ningqi Wang (0xf3cd)",
    " * Email: nq.maigre@gmail.com",
    " * Repo : https://github.com/0xf3cd/celestial-calendar",
    " *",
    " * SPDX-License-Identifier: MIT",
    " */",
    "",
    "#pragma once",
    "",
    "#include <array>",
    "#include <optional>",
    "",
    '#include "ephemeris.hpp"',
    '#include "event_search.hpp"',
    "",
    "namespace astro::ephemeris::detail {",
    "",
    "/** @brief Source-model derivative and evaluation allowances under the event-search floating-point contract. */",
    "struct EventBounds {",
    "  astro::detail::event_search::TrajectoryBounds trajectory;",
    "  double position_error_deg;",
    "  double rate_error_deg_per_day;",
    "  std::optional<double> minimum_rate;",
    "  std::optional<double> maximum_rate;",
    "};",
    "",
    "// Indexed by Target; all floating literals enclose the exact offline bound in its required direction.",
  ]
  for name in ("EVENT_BOUNDS_WIDE", "EVENT_BOUNDS_CENTURY"):
    table = record["tables"][name]
    if set(table) != set(TARGETS):
      raise ValueError("Incomplete event-bound table")
    lines.append(f"inline constexpr std::array<EventBounds, {len(TARGETS)}> {name} {{{{")
    for target in TARGETS:
      item = table[target]

      def value(field: str, item: dict = item) -> str:
        if Fraction(item[field]["exact"]) < 0:
          raise ValueError("Negative magnitude/error bound")
        return directed(item[field])

      def optional(field: str, item: dict = item) -> str:
        if item[field] is None:
          return "std::nullopt"
        return "std::optional<double> { " + directed(item[field], lower=field == "signed_rate_lower") + " }"

      lines += [
        f"  // {target}",
        "  {",
        "    .trajectory = {",
        "      .longitude = { .first_derivative = "
        + value("position_first_derivative")
        + ", .second_derivative = "
        + value("position_second_derivative")
        + " },",
        "      .rate = { .first_derivative = "
        + value("rate_first_derivative")
        + ", .second_derivative = "
        + value("rate_second_derivative")
        + " },",
        "      .third_derivative = " + value("position_third_derivative") + ",",
        "      .rate_remainder = " + value("rate_to_position_derivative_remainder") + ",",
        "    },",
        "    .position_error_deg = " + value("position_error") + ",",
        "    .rate_error_deg_per_day = " + value("rate_error") + ",",
        "    .minimum_rate = " + optional("signed_rate_lower") + ",",
        "    .maximum_rate = " + optional("signed_rate_upper") + ",",
        "  },",
      ]
    lines += ["}};", ""]
  lines += ["} // namespace astro::ephemeris::detail", ""]
  return "\n".join(lines)


def check_sources(record: dict, actual: dict[str, str], solar_body: str) -> None:
  """Invalidate bound evidence when a pinned numerical source changes."""
  if record["source_sha256"] != actual or record["solar_geocentric_namespace_body_sha256"] != solar_body:
    raise ValueError("Event-bound source identity changed; recheck the affected numerical evidence")


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--check", action="store_true", help="Check the source pins and generated header")
  args = parser.parse_args()
  root = Path(__file__).resolve().parents[1]
  record = json.loads((root / RECORD_PATH).read_text(encoding="utf-8"))
  rendered = emit(record)
  if args.check:
    actual = {
      name: hashlib.sha256((root / name).read_text(encoding="utf-8").encode("utf-8")).hexdigest()
      for name in record["source_sha256"]
    }
    solar = (
      (root / "src/astro/sun.hpp").read_text(encoding="utf-8").split("namespace astro::sun::geocentric_coord {", 1)[1]
    )
    solar = solar.split("} // namespace astro::sun::geocentric_coord", 1)[0]
    check_sources(record, actual, hashlib.sha256(solar.encode()).hexdigest())
    if rendered != (root / HEADER_PATH).read_text(encoding="utf-8"):
      raise ValueError("C++ event bounds do not match the exact records")
    print("Event-bound source pins, target inventory and directed C++ literals match")
  else:
    print(rendered, end="")


if __name__ == "__main__":
  main()
