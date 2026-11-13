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

"""Restricted C++ expression readers and exact-rational derivation serialization."""

import ast
import hashlib
import re
from dataclasses import asdict, is_dataclass
from fractions import Fraction as F


MIRRORED_OPERATIONS = {
  "vsop87d/defines.hpp": "fe0f35a74433dc931829975832264d23d03a338ee887361966688c5b5719f839",
  "julian_day.hpp": "c260c4d7fc6fe551f84756b57fd7c7d3ac739de0e9838b4345c5a68feaf13c6e",
  "sun.hpp": "2980b980d04c512049f667efb565d481f2dad419d1d56e8069728bb14dd2c7ec",
  "moon.hpp": "026062f45b3361c8f348e76f2c818b61c4e638602d5b54e1b064cc5a136327b1",
}


def verify_mirrored_operations(sources: dict[str, str]) -> None:
  """Require a reviewed source shape for provider operations modeled rather than parsed."""
  for name, digest in MIRRORED_OPERATIONS.items():
    if name not in sources or hashlib.sha256(sources[name].encode()).hexdigest() != digest:
      raise ValueError("Mirrored operation source changed: " + name + "; review its operation bound")


def serial(value):
  if isinstance(value, F):
    return str(value)
  if is_dataclass(value):
    return serial(asdict(value))
  if isinstance(value, dict):
    return {key: serial(item) for key, item in value.items()}
  if isinstance(value, (tuple, list)):
    return [serial(item) for item in value]
  return value


def stored_constant(source, name, environment):
  expression = re.search(r"inline constexpr double " + name + r"\s*=\s*(.*?);", source, re.DOTALL)
  if expression is None:
    raise ValueError("A source constant has no exact initializer: " + name)

  def evaluate(node):
    if isinstance(node, ast.Constant) and type(node.value) in (int, float):
      return float(node.value)
    if isinstance(node, ast.Name) and node.id in environment:
      return environment[node.id]
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
      return -evaluate(node.operand)
    if isinstance(node, ast.BinOp):
      a, b = evaluate(node.left), evaluate(node.right)
      if isinstance(node.op, ast.Add):
        return a + b
      if isinstance(node.op, ast.Sub):
        return a - b
      if isinstance(node.op, ast.Mult):
        return a * b
      if isinstance(node.op, ast.Div):
        return a / b
    raise ValueError("Unsupported stored-constant operation: " + ast.dump(node))

  return evaluate(ast.parse(expression[1].strip().replace("\n", " "), mode="eval").body)


def correction_expressions(planet, coordinates):
  annual = planet.split("inline auto aberration(", 1)[1].split("return {", 1)[0]
  fk5 = coordinates.split("inline auto fk5_correction(", 1)[1].split("return {", 1)[0]
  patterns = {
    "eccentricity": (annual, r"const double e = (.*?);"),
    "perihelion_degrees": (annual, r"const toolbox::AngleDeg π \{ (.*?) \};"),
    "fk5_drift_degrees": (fk5, r"λ_dash = vsop_λ - toolbox::AngleDeg \{ (.*?) \};"),
    "annual_longitude_arcsec": (annual, r"const double Δλ_arcsec = (.*?);"),
    "annual_latitude_arcsec": (annual, r"const double Δβ_arcsec = (.*?);"),
    "fk5_longitude_arcsec": (fk5, r"const double delta_λ_arcsec = (.*?);"),
    "fk5_latitude_arcsec": (fk5, r"const double delta_β_arcsec = (.*?);"),
  }
  return {name: re.search(pattern, source, re.DOTALL)[1] for name, (source, pattern) in patterns.items()}
