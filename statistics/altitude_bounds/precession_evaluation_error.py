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

"""Error propagation through the implemented ecliptic-precession Cartesian route."""

import ast
import re
from fractions import Fraction as F
from math import ceil

from .floating_operation_bounds import FloatingBound as B
from .heliocentric_evaluation_error import DEG_PER_RAD_FP, TWO_PI_FP, angle_roundtrip_error
from .poisson_majorants import pi_interval
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, UNIT_ROUNDOFF, rounded
from .spherical_reconstruction_error import reconstruction_error
from .spherical_coordinate_majorants import dyadic_upper


def source_polynomial_bound(expression: str, variables: dict) -> B:
  """Evaluate a source arithmetic AST in the error algebra, retaining its operation tree."""
  tree = ast.parse(" ".join(expression.split()), mode="eval").body

  def evaluate(node):
    if isinstance(node, ast.Constant) and type(node.value) in (int, float):
      return B.literal(node.value)
    if isinstance(node, ast.Name) and node.id in variables:
      return variables[node.id]
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
      return evaluate(node.operand)
    if isinstance(node, ast.BinOp):
      left, right = evaluate(node.left), evaluate(node.right)
      if isinstance(node.op, (ast.Add, ast.Sub)):
        return left + right
      if isinstance(node.op, ast.Mult):
        return left * right
      if (
        isinstance(node.op, ast.Div) and isinstance(node.right, ast.Constant) and type(node.right.value) in (int, float)
      ):
        return left / node.right.value
    raise ValueError("Unsupported precession polynomial syntax")

  return evaluate(tree)


def degree_normalized_storage_error(radian_magnitude: F) -> F:
  """Bound rad_to_deg, degree normalization and rad retrieval against the original circular phase."""
  storage_error = UNIT_ROUNDOFF * radian_magnitude * DEG_PER_RAD_FP + SUBNORMAL_ALLOWANCE
  degree_magnitude = radian_magnitude * DEG_PER_RAD_FP + storage_error
  turns = ceil(degree_magnitude / 360 + F(1, 2)) + 2
  pi_lower, pi_upper = pi_interval()
  period = 360 / DEG_PER_RAD_FP
  period_error = max(abs(period - 2 * pi_lower), abs(period - 2 * pi_upper))
  wrap_error = UNIT_ROUNDOFF * 360 + SUBNORMAL_ALLOWANCE
  retrieval_error = UNIT_ROUNDOFF * period + SUBNORMAL_ALLOWANCE
  return (storage_error + wrap_error) / DEG_PER_RAD_FP + turns * period_error + retrieval_error


def precession_error_from_centuries(
  record: dict, raw: dict, t0: B, t: B, source: str, proved_output_axial_lower: F = F(0)
) -> dict:
  """Compose pointwise or uniform century-operation enclosures with the same source expression tree."""
  if not 0 <= proved_output_axial_lower <= 1:
    raise ValueError("A supplied precession axial lower bound must be in[0,1]")
  variables = {"t0": t0, "t": t}
  expressions = {
    name: re.search(r"const double " + name + r"\s*=\s*(.*?);", source, re.DOTALL)[1]
    for name in ("η_arcsec", "Π_var_arcsec", "p_arcsec")
  }
  eta = (source_polynomial_bound(expressions["η_arcsec"], variables) / 3600).degrees_to_radians()
  pi = (
    B.literal(174.876384) + source_polynomial_bound(expressions["Π_var_arcsec"], variables) / 3600
  ).degrees_to_radians()
  p = (source_polynomial_bound(expressions["p_arcsec"], variables) / 3600).degrees_to_radians()
  lambda_input = B(TWO_PI_FP + angle_roundtrip_error(TWO_PI_FP), raw["longitude_circular_error_rad"])
  raw_beta_magnitude = F(record["evaluation_at_exact_binary64_tau"]["B"]["computed_magnitude"]["exact"])
  beta_input = B(raw_beta_magnitude + angle_roundtrip_error(raw_beta_magnitude), raw["latitude_error_rad"])
  phase = pi - lambda_input
  phase_wave, beta_wave, eta_wave = (value.sine_or_cosine() for value in (phase, beta_input, eta))
  a = eta_wave * beta_wave * phase_wave - eta_wave * beta_wave
  b = beta_wave * phase_wave
  c = eta_wave * beta_wave + eta_wave * beta_wave * phase_wave
  # Only the final phase additions are charged here; inverse-angle input transport is coupled below.
  phase_sum = B(p.magnitude) + B(pi.magnitude) - B(pi_interval()[1] + LIBM_ALLOWANCE)
  phase_rounding = phase_sum.error
  angular_error = p.error + pi.error + phase_rounding + 2 * LIBM_ALLOWANCE
  angular_error += degree_normalized_storage_error(phase_sum.magnitude)
  angular_error += angle_roundtrip_error(pi_interval()[1] / 2 + LIBM_ALLOWANCE)
  beta_reference_upper = F(record["coordinate_derivatives"]["B"][0]["exact"])
  global_axial_lower = max(F(0), 1 - beta_reference_upper**2 / 2 - eta.magnitude - eta.error)
  axial_lower = max(global_axial_lower, proved_output_axial_lower)
  direction_error = dyadic_upper((reconstruction_error(a.error + b.error, c.error, angular_error, axial_lower),))[0]
  radial_magnitude = F(record["evaluation_at_exact_binary64_tau"]["R"]["computed_magnitude"]["exact"])
  first_magnitude, first_rounding = rounded(radial_magnitude * (1 + LIBM_ALLOWANCE), F(0))
  _, second_rounding = rounded(first_magnitude * (1 + LIBM_ALLOWANCE), first_rounding * (1 + LIBM_ALLOWANCE))
  xy_error = radial_magnitude * (2 * LIBM_ALLOWANCE + LIBM_ALLOWANCE**2) + second_rounding
  z_error = radial_magnitude * LIBM_ALLOWANCE + first_rounding
  vector_error = dyadic_upper((raw["radius_error_au"] + radial_magnitude * direction_error + 2 * xy_error + z_error,))[
    0
  ]
  return {
    "direction_error": direction_error,
    "cartesian_norm_error_au": vector_error,
    "output_axial_lower": axial_lower,
    "intermediate_xy_error": a.error + b.error,
    "intermediate_z_error": c.error,
    "phase_angular_error": angular_error,
  }
