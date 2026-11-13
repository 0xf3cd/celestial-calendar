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

"""Fixed ALGO5 branch arithmetic, including integer powers and the positive rational fit denominator."""

import ast
import re
from fractions import Fraction as F

from .apparent_correction_error import positive_quotient
from .source_arithmetic import arithmetic
from .clock_branch_majorants import branch_records
from .floating_operation_bounds import FloatingBound as B
from .rational_intervals import Interval as I
from .raw_evaluation_error import LIBM_ALLOWANCE, SUBNORMAL_ALLOWANCE, rounded


def clock_source_bodies(source: str) -> tuple:
  """Select the thirteen retained branches from the source, including pre2005 delegation."""
  clean = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.DOTALL)
  historical = clean.split("namespace algo2 {", 1)[1].split("#pragma endregion", 1)[0]
  historical_bodies = re.findall(r"if\s*\((.*?)\)\s*\{(.*?)\}", historical, re.DOTALL)
  current = clean.split("namespace algo5 {", 1)[1].split("#pragma endregion", 1)[0]
  fit = re.search(r"if\s*\(year <= LAST_OBSERVATION_YEAR\)\s*\{(.*?)\}", current, re.DOTALL)[1]
  future = re.search(r"\{\s*const double t = \(year - 1825\.0\) / 100\.0;(.*?)\}", current, re.DOTALL)[1]
  bodies = []
  for index, row in enumerate(branch_records()):
    if index < 11:
      midpoint = (row["start_year"] + row["end_year"]) / 2
      matching = [body for condition, body in historical_bodies if arithmetic(condition, {"year": midpoint}, True)]
      if len(matching) != 1:
        raise ValueError("The historical fixed-clock branch is not uniquely selected")
      bodies.append(matching[0])
    else:
      bodies.append(fit if index == 11 else "const double t = (year - 1825.0) / 100.0;" + future)
  return tuple(bodies)


def integer_power_error(argument: B, power: int) -> B:
  """Use |pow_fl(x,n)-x^n|<=2^-48*(1+|x^n|)+eta for the source's integer n in[2,7]."""
  if power not in range(2, 8):
    raise ValueError("Only the fixed source integer powers2 through7 have this retained premise")
  magnitude = argument.magnitude**power
  elementary_error = LIBM_ALLOWANCE * (1 + magnitude) + SUBNORMAL_ALLOWANCE
  transported = power * (argument.magnitude + argument.error) ** (power - 1) * argument.error
  return B(magnitude + elementary_error, transported + elementary_error)


def clock_model_branch_error(index: int, reference_left: F, reference_right: F, year_error: F, source: str) -> B:
  """Bound one branch's expression; callers separately account for a different reference branch."""
  if index not in range(13) or reference_left > reference_right or year_error < 0:
    raise ValueError("Require a retained branch, ordered reference-year interval and nonnegative input allowance")
  body = clock_source_bodies(source)[index]
  references = {"year": I.enclose(reference_left, reference_right)}
  values = {"year": B(max(abs(reference_left), abs(reference_right)) + year_error, year_error)}

  def evaluate(node):
    if isinstance(node, ast.Constant) and type(node.value) in (int, float):
      return B.literal(node.value)
    if isinstance(node, ast.Name) and node.id in values:
      return values[node.id]
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
      return evaluate(node.operand)
    if isinstance(node, ast.BinOp):
      # Bound cancellation in year-origin before evaluating the fit's large powers.
      if (
        isinstance(node.op, ast.Sub)
        and isinstance(node.left, ast.Name)
        and node.left.id == "year"
        and isinstance(node.right, ast.Constant)
      ):
        origin = F(node.right.value)
        magnitude = max(abs(reference_left - origin), abs(reference_right - origin)) + year_error
        return B(*rounded(magnitude, year_error))
      left, right = evaluate(node.left), evaluate(node.right)
      if isinstance(node.op, (ast.Add, ast.Sub)):
        return left + right
      if isinstance(node.op, ast.Mult):
        return left * right
      if isinstance(node.op, ast.Div):
        if isinstance(node.right, ast.Constant):
          return left / node.right.value
        if isinstance(node.right, ast.Name) and node.right.id in references:
          return positive_quotient(left, right, references[node.right.id].lo)
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
      if (
        node.func.id == "pow"
        and len(node.args) == 2
        and isinstance(node.args[1], ast.Constant)
        and type(node.args[1].value) is int
      ):
        return integer_power_error(evaluate(node.args[0]), node.args[1].value)
      if node.func.id == "cos" and len(node.args) == 1:
        return evaluate(node.args[0]).sine_or_cosine()
    raise ValueError("Unsupported fixed-clock source operation")

  def expression(text):
    return evaluate(ast.parse(" ".join(text.replace("std::", "").split()), mode="eval").body)

  for name, text in re.findall(r"const double (\w+)\s*=\s*(.*?);", body, re.DOTALL):
    values[name] = expression(text)
    references[name] = arithmetic(text, references, True)
  returned = re.findall(r"return (.*?);", body, re.DOTALL)
  if len(returned) != 1:
    raise ValueError("The retained fixed-clock source branch has no unique return expression")
  return expression(returned[0])
