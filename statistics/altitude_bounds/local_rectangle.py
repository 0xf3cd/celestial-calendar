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

"""Compose source-centre enclosures into local apparent, clock and observer certificates."""

from fractions import Fraction as F

from .apparent_correction_error import degree_input_error
from .conditional_source_derivatives import vsop_source_derivatives
from .equatorial_evaluation_error import equatorial_error, rotated_axial_lower
from .floating_operation_bounds import FloatingBound as B
from .local_apparent_value import local_apparent_errors, local_rounded_geometry
from .nutation_majorants import nutation_bounds, nutation_rows
from .observer_objective_derivatives import observer_objective_derivatives
from .ordinary_clock_tiers import branch_rotation
from .poisson_majorants import pi_interval
from .rational_intervals import Interval as I
from .source_reading import serial
from .uniform_clock_error import uniform_clock_error
from .uniform_observer_error import uniform_observer_error


def compose_rectangle(raw, row, precession, earth, delta, expressions, constants, sites):
  name = row["planet"]
  centre, half = F(row["centre_jde_tt"]), F(row["half_width_days"])
  point = tuple(I(F(a), F(b)) for a, b in row["centre_geocentric_intervals_exact"])
  interval = (centre - half, centre + half)
  local = local_rounded_geometry(raw[name], raw["earth"], centre, half, point, precession)
  entry = {"planet": name, "analysis_interval_exact": serial(interval), "local_map": serial(local)}
  if local["geometry"]["axial_lower_au"] <= 0:
    return dict(entry, status="axial_unresolved")
  physical = local_apparent_errors(raw[name], raw["earth"], centre, half, point, precession, earth, expressions)
  if physical["local_map"] != local:
    raise ArithmeticError("The apparent value proof changed its complete invariant geometry")
  errors, magnitudes = physical["correction_errors"], physical["correction_magnitudes"]
  latitude = (
    sum(magnitudes[key] + errors[key] for key in ("annual_latitude_degrees", "fk5_latitude_degrees"))
    * pi_interval()[1]
    / 180
  )
  obliquity = (
    (physical["true_obliquity_degree_magnitude"] + physical["true_obliquity_degree_error"]) * pi_interval()[1] / 180
  )
  axis = rotated_axial_lower(physical["reference_cosine_lower"], latitude, obliquity)
  equatorial = equatorial_error(
    B(F(360), degree_input_error(F(360), errors["final_longitude_radians"])),
    B(
      physical["latitude_degree_magnitude"],
      degree_input_error(physical["latitude_degree_magnitude"], errors["final_latitude_radians"]),
    ),
    B(physical["true_obliquity_degree_magnitude"], physical["true_obliquity_degree_error"]),
    axis,
  )
  clock = uniform_clock_error(*interval, delta)
  if len(clock["model_bounds"]) != 1 or clock["model_bounds"][0]["mismatch_seconds"]:
    raise ValueError("A local source rectangle requires an explicit model collar")
  rotation = branch_rotation(clock["model_bounds"][0]["branch"], *interval, earth)
  radius = max(abs(value - 2451545) for value in interval) / 36525
  iau = nutation_bounds(nutation_rows(earth), radius, 3)
  derivatives = vsop_source_derivatives(raw[name], raw["earth"], local["geometry"], radius, iau)
  apparent = {"planet": name, "analysis_interval_exact": interval, "proof": physical}
  projected = {"planet": name, "analysis_interval_exact": interval, "proof": equatorial}
  observers = []
  for site in sites:
    value = uniform_observer_error(apparent, projected, clock, site, earth, constants)
    smooth = observer_objective_derivatives(
      derivatives["direction"],
      derivatives["distance"],
      rotation,
      local["geometry"]["radial_lower_au"],
      value["geometry"]["reference_q_lower"],
      site,
      constants,
      F(0),
    )
    observers.append({"site_exact": serial(site), "value": serial(value), "smooth_level_zero": serial(smooth)})
  return dict(
    entry,
    status="observer_envelopes_constructed",
    apparent=serial(physical),
    equatorial=serial(equatorial),
    clock=serial(clock),
    rotation=serial(rotation),
    source_derivatives=serial(derivatives),
    observers=observers,
  )
