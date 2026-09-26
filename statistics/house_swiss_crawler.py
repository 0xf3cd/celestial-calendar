#!/usr/bin/env python3
#
# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import argparse
import ctypes
import sys

from dataclasses import dataclass
from pathlib import Path
from typing import Final


SWISS_VERSION: Final[str] = "2.10.03"  # Runtime version reported by the v2.10.3bfinal source release.
SYSTEM_NAMES: Final[dict[str, str]] = {"E": "EQUAL", "W": "WHOLE_SIGN", "P": "PLACIDUS"}


@dataclass(frozen=True)
class FormulaCase:
  name: str
  armc_deg: float
  latitude_deg: float
  obliquity_deg: float
  systems: str = "EWP"


@dataclass(frozen=True)
class DateCase:
  name: str
  jd_ut1: float
  latitude_deg: float
  longitude_deg: float
  systems: str = "EWP"


@dataclass(frozen=True)
class HouseResult:
  ascendant_deg: float
  midheaven_deg: float
  armc_deg: float
  cusps_deg: tuple[float, ...]


FORMULA_CASES: Final[tuple[FormulaCase, ...]] = (
  FormulaCase("meeus-14", 75.0, 51.0, 23.44),
  FormulaCase("southern", 123.0, -33.8688, 23.4392911),
  FormulaCase("armc-wrap", 359.9, 0.0, 23.4392911),
  FormulaCase("near-polar-north", 276.94, 66.50, 23.44, "P"),
  FormulaCase("near-polar-south", 96.94, -66.50, 23.44, "P"),
  FormulaCase("high-latitude-north", 183.0, 89.0, 23.44, "EW"),
  FormulaCase("high-latitude-south", 3.0, -89.0, 23.44, "EW"),
  FormulaCase("whole-zero-boundary", 270.0, 60.0, 23.44, "W"),
)

DATE_CASES: Final[tuple[DateCase, ...]] = (
  DateCase("j2000-greenwich", 2451545.0, 51.5, 0.0),
  # Date, time, and Washington coordinates from Meeus Example 13.b.
  DateCase("meeus-13b-washington", 2446896.30625, 38.9213888889, -77.0655555556, "P"),
  DateCase("j2000-sydney", 2451545.0, -33.8688, 151.2093, "P"),
  DateCase("east-date-line", 2461041.5, 35.0, 179.9, "P"),
  DateCase("west-date-line", 2461041.5, 35.0, -179.9, "P"),
)


class SwissEphemeris:
  def __init__(self, library_path: Path) -> None:
    if not library_path.is_file():
      raise FileNotFoundError(f"Swiss Ephemeris shared library not found: {library_path}")
    self._library = ctypes.CDLL(str(library_path.resolve()))
    double_pointer = ctypes.POINTER(ctypes.c_double)

    self._library.swe_version.argtypes = [ctypes.c_char_p]
    self._library.swe_version.restype = ctypes.c_char_p
    self._library.swe_deltat.argtypes = [ctypes.c_double]
    self._library.swe_deltat.restype = ctypes.c_double
    self._library.swe_houses_armc_ex2.argtypes = [
      ctypes.c_double,
      ctypes.c_double,
      ctypes.c_double,
      ctypes.c_int,
      double_pointer,
      double_pointer,
      double_pointer,
      double_pointer,
      ctypes.c_char_p,
    ]
    self._library.swe_houses_armc_ex2.restype = ctypes.c_int
    self._library.swe_houses_ex2.argtypes = [
      ctypes.c_double,
      ctypes.c_int32,
      ctypes.c_double,
      ctypes.c_double,
      ctypes.c_int,
      double_pointer,
      double_pointer,
      double_pointer,
      double_pointer,
      ctypes.c_char_p,
    ]
    self._library.swe_houses_ex2.restype = ctypes.c_int

    version_buffer = ctypes.create_string_buffer(256)
    self.version = self._library.swe_version(version_buffer).decode("ascii")
    if self.version != SWISS_VERSION:
      raise RuntimeError(f"unexpected Swiss Ephemeris version: expected {SWISS_VERSION}, got {self.version}")

  def delta_t_days(self, jd_ut1: float) -> float:
    return float(self._library.swe_deltat(jd_ut1))

  @staticmethod
  def _arrays() -> tuple[ctypes.Array, ctypes.Array, ctypes.Array]:
    return (ctypes.c_double * 13)(), (ctypes.c_double * 10)(), ctypes.create_string_buffer(256)

  @staticmethod
  def _result(cusps: ctypes.Array, ascmc: ctypes.Array) -> HouseResult:
    return HouseResult(
      ascendant_deg=ascmc[0],
      midheaven_deg=ascmc[1],
      armc_deg=ascmc[2],
      cusps_deg=tuple(cusps[index] for index in range(1, 13)),
    )

  def houses_armc(
    self,
    armc_deg: float,
    latitude_deg: float,
    obliquity_deg: float,
    system: str,
  ) -> HouseResult:
    cusps, ascmc, error = self._arrays()
    result = self._library.swe_houses_armc_ex2(
      armc_deg,
      latitude_deg,
      obliquity_deg,
      ord(system),
      cusps,
      ascmc,
      None,
      None,
      error,
    )
    if result < 0:
      raise RuntimeError(
        f"Swiss Ephemeris rejected {system=} {armc_deg=} {latitude_deg=} {obliquity_deg=}: "
        f"{error.value.decode('utf-8')}"
      )
    return self._result(cusps, ascmc)

  def houses_date(self, case: DateCase, system: str) -> HouseResult:
    cusps, ascmc, error = self._arrays()
    result = self._library.swe_houses_ex2(
      case.jd_ut1,
      0,
      case.latitude_deg,
      case.longitude_deg,
      ord(system),
      cusps,
      ascmc,
      None,
      None,
      error,
    )
    if result < 0:
      raise RuntimeError(f"Swiss Ephemeris rejected {case.name} {system=}: {error.value.decode('utf-8')}")
    return self._result(cusps, ascmc)

  def assert_placidus_fallback_is_rejected(self) -> None:
    cusps, ascmc, error = self._arrays()
    result = self._library.swe_houses_armc_ex2(
      276.94,
      66.57,
      23.44,
      ord("P"),
      cusps,
      ascmc,
      None,
      None,
      error,
    )
    message = error.value.decode("utf-8")
    if result >= 0 or "switched to Porphyry" not in message:
      raise RuntimeError(f"expected the pinned Swiss polar fallback, got return={result}, error={message!r}")
    print(f"confirmed rejected Swiss fallback: return={result}, error={message!r}", file=sys.stderr)


def format_cusps(cusps_deg: tuple[float, ...]) -> str:
  return "{ " + ", ".join(f"{cusp:.12f}" for cusp in cusps_deg) + " }"


def emit_formula_rows(swiss: SwissEphemeris) -> None:
  print("// Swiss Ephemeris v2.10.3bfinal, swe_houses_armc_ex2 formula-level rows")
  for case in FORMULA_CASES:
    for system in case.systems:
      result = swiss.houses_armc(case.armc_deg, case.latitude_deg, case.obliquity_deg, system)
      print(
        f'  {{ "{case.name}", System::{SYSTEM_NAMES[system]}, {case.armc_deg:.10f}, '
        f"{case.latitude_deg:.10f}, {case.obliquity_deg:.10f}, {result.ascendant_deg:.12f}, "
        f"{result.midheaven_deg:.12f}, {format_cusps(result.cusps_deg)} }},"
      )


def emit_date_rows(swiss: SwissEphemeris) -> None:
  print("\n// Swiss Ephemeris v2.10.3bfinal, swe_houses_ex2 date-level rows")
  for case in DATE_CASES:
    jde_tt = case.jd_ut1 + swiss.delta_t_days(case.jd_ut1)
    for system in case.systems:
      result = swiss.houses_date(case, system)
      print(
        f'  {{ "{case.name}", System::{SYSTEM_NAMES[system]}, {case.jd_ut1:.10f}, {jde_tt:.10f}, '
        f"{case.latitude_deg:.10f}, {case.longitude_deg:.10f}, {result.ascendant_deg:.12f}, "
        f"{result.midheaven_deg:.12f}, {format_cusps(result.cusps_deg)} }},"
      )


def main() -> None:
  parser = argparse.ArgumentParser(description="Emit house-system golden rows from pinned Swiss Ephemeris")
  parser.add_argument("library", type=Path, help="path to a v2.10.3bfinal Swiss Ephemeris shared library")
  args = parser.parse_args()

  swiss = SwissEphemeris(args.library)
  print(f"Swiss Ephemeris runtime version {swiss.version}", file=sys.stderr)
  swiss.assert_placidus_fallback_is_rejected()
  emit_formula_rows(swiss)
  emit_date_rows(swiss)


if __name__ == "__main__":
  main()
