#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Whole-tier Sun, Moon and Pluto value/direction records from source-operation bounds."""

from fractions import Fraction as F
from pathlib import Path

from .conditional_source_derivatives import lunar_source_derivatives, solar_source_derivatives
from .equatorial_evaluation_error import equatorial_error, rotated_axial_lower
from .floating_operation_bounds import FloatingBound as B
from .global_vsop_geometry import source_radius_lower
from .lunar_evaluation_error import lunar_evaluation_error
from .nutation_majorants import nutation_bounds, nutation_rows
from .pluto_apparent_error import pluto_apparent_error
from .pluto_equatorial_majorants import pluto_equatorial_bounds
from .pluto_smooth_majorants import pluto_distance_bounds
from .poisson_majorants import array_rows, pi_interval, pluto_bounds
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE
from .solar_evaluation_error import solar_evaluation_error


def other_body_tier(
  name: str, root: Path, left: F, right: F, earth_record: dict, earth_tables: dict, expressions: dict, constants: tuple
) -> dict:
  """Bind positive stored TT endpoints within the body's provider domain."""
  if not 0 < left < right or any(F(float(value)) != value for value in (left, right)):
    raise ValueError("A whole-body tier needs positive ordered stored TT endpoints")
  if name not in ("sun", "moon", "pluto"):
    raise ValueError("This source composition is for Sun, Moon or Pluto")
  if name == "pluto" and not F(2409543.5) <= left < right < F(2488069.5):
    raise ValueError("The Pluto tier leaves the actual apparent-provider domain")
  earth = (root / "src/astro/earth.hpp").read_text()
  radius = max(abs(left - 2451545), abs(right - 2451545)) / 36525
  iau = nutation_bounds(nutation_rows(earth), radius, 3)
  if name == "sun":
    source = solar_evaluation_error(earth_record, left, right, earth, expressions["fk5_drift_degrees"])
    derivative = solar_source_derivatives(
      earth_record, radius, iau, array_rows(earth, "MEEUS_DAILY_VARIATION_TERMS", "DailyVariationTerm", 21, 4)
    )
    distance_lower = source_radius_lower(earth_tables, F(earth_record["tau_radius_exact"]))
    epsilon = source["true_obliquity_degrees"]
    correction = source["fk5_latitude_degrees"]
    axis = rotated_axial_lower(
      source["geometric_cosine_lower"],
      (correction.magnitude + correction.error) * pi_interval()[1] / 180,
      (epsilon.magnitude + epsilon.error) * pi_interval()[1] / 180,
    )
    equatorial = equatorial_error(source["final_longitude_degrees"], source["final_latitude_degrees"], epsilon, axis)
    distance = source["distance_au"]
  elif name == "moon":
    elp = (root / "src/astro/elp2000_82b.hpp").read_text()
    source = lunar_evaluation_error(left, right, elp, earth)
    derivative = lunar_source_derivatives(
      array_rows(elp, "LR", "LRCoefficients", 60, 6),
      array_rows(elp, "B", "BCoefficients", 60, 5),
      radius,
      iau,
      constants[2],
    )
    distance_lower = source["reference_distance_lower_au"]
    epsilon = source["true_obliquity_degrees"]
    axis = rotated_axial_lower(
      source["reference_latitude_cosine_lower"], F(0), (epsilon.magnitude + epsilon.error) * pi_interval()[1] / 180
    )
    equatorial = equatorial_error(source["final_longitude_degrees"], source["final_latitude_degrees"], epsilon, axis)
    distance = source["distance_au"]
  else:
    pluto = (root / "src/astro/pluto.hpp").read_text()
    precession = (root / "src/astro/earth/precession.hpp").read_text()
    source = pluto_apparent_error(left, right, earth_record, pluto, precession, earth, expressions)
    emission_radius = max(abs(left - 1 - 2451545), abs(right - 2451545)) / 36525
    raw = pluto_bounds(array_rows(pluto, "TERMS", "Term", 43, 9), emission_radius, 3)
    earth_vector = tuple(F(row["exact"]) for row in earth_record["cartesian_norm_derivatives_au"])
    smooth = pluto_distance_bounds(raw, earth_vector, radius, source["provider"], source["geometry"])
    apparent = pluto_equatorial_bounds(smooth, source, earth_record, radius, earth)
    derivative = {"direction": apparent["apparent"]["equatorial_direction_norm"], "distance": smooth["distance_au"]}
    source["raw_derivatives"] = raw
    source["smooth_derivatives"] = smooth
    source["apparent_derivatives"] = apparent
    distance_lower = source["geometry"]["radial_lower_au"]
    equatorial = source["equatorial"]
    distance = B(
      (1 + LIBM_ALLOWANCE) * source["uniform_map"]["computed_vector_l1_upper_au"] + SUBNORMAL_ALLOWANCE,
      source["fixed_point"]["fixed_point_distance_error_upper_au"],
    )
  return {
    "observation_interval": (left, right),
    "geometry": {"radial_lower_au": distance_lower},
    "apparent": {"equatorial_direction_norm": derivative["direction"]},
    "distance_derivatives_au": derivative["distance"],
    "equatorial": equatorial,
    "distance_value": distance,
    "source_value_proof": source,
  }
