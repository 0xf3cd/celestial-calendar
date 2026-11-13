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

"""Compose complete far-local source proofs into outward arbitrary-site runtime packets."""

from fractions import Fraction as F

from .affine_observer_error import affine_observer_error
from .certificate_encoding import decode, encode
from .clock_cell_partition import partition_clock_interval
from .floating_operation_bounds import FloatingBound as B
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE
from .sidereal_evaluation_error import sidereal_error


def derive(parent, earth, constants):
  records, exclusions = [], []
  for index, row in enumerate(parent["results"]):
    left, right = map(F, row["analysis_interval_exact"])
    if row["status"] != "observer_envelopes_constructed" or right - left != F(1, 4):
      exclusions.append(
        {
          "parent_index": index,
          "planet": row["planet"],
          "reason": row["status"]
          if row["status"] != "observer_envelopes_constructed"
          else "not_a_complete_canonical_quarter",
        }
      )
      continue
    if (4 * (left - 2451545)).denominator != 1:
      raise ValueError("A retained local proof is not a query-independent canonical cell")
    clock = row["clock"]
    partition = partition_clock_interval(left, right, F(clock["year_error_upper"]), 2048)
    if len(partition) != 1 or partition[0].joins:
      raise ValueError("A local runtime packet cannot infer smoothness across a clock join")
    theta = sidereal_error(
      left, right, *map(F, clock["reference_ut1_interval"]), F(clock["ut1_jd_error_days"]), F(180), earth
    )
    apparent = row["apparent"]
    distance = B(
      (1 + LIBM_ALLOWANCE) * F(apparent["uniform_map"]["computed_vector_l1_upper_au"]) + SUBNORMAL_ALLOWANCE,
      F(apparent["fixed_point"]["fixed_point_distance_error_upper_au"]),
    )
    lower = F(apparent["geometry"]["radial_lower_au"])
    affine = affine_observer_error(
      B(F(90)), F(row["equatorial"]["stored_angle_direction_error"]), theta["local"].error, distance, lower, constants
    )
    derivatives = row["source_derivatives"]
    fields = (
      *map(F, derivatives["direction"][1:3]),
      *map(F, derivatives["distance"][:3]),
      lower,
      affine["residual"].error.constant,
      affine["residual"].error.height,
      affine["q"].magnitude.constant,
      affine["q"].magnitude.height,
    )
    codes = tuple(encode(value, field != 5) for field, value in enumerate(fields))
    for field, (value, code) in enumerate(zip(fields, codes, strict=True)):
      if decode(code) > value if field == 5 else decode(code) < value:
        raise ArithmeticError("A local packet rounded inward")
    for observer in row["observers"]:
      height = abs(F(observer["site_exact"][2]))
      geometry = observer["value"]["geometry"]
      # The arbitrary-site residual allowance also covers each level-zero up observation.
      if decode(codes[6]) + decode(codes[7]) * height < F(geometry["up"]["error"]):
        raise ArithmeticError("The arbitrary-site local value packet lost an inherited observation bound")
    records.append(
      {
        "parent_index": index,
        "target": {"mars": 4, "jupiter": 5, "saturn": 6}[row["planet"]],
        "planet": row["planet"],
        "interval": (left, right),
        "fields": fields,
        "codes": codes,
        "rotation": tuple(map(F, row["rotation"][1:3])),
        "clock": clock,
        "affine": affine,
      }
    )
  return records, exclusions
