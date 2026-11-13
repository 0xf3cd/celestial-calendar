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

"""Restricted arithmetic and comparison evaluation for source clock expressions."""

import ast
from fractions import Fraction as F


def arithmetic(expression, values, rational=False):
  tree = ast.parse(" ".join(expression.replace("std::", "").split()), mode="eval").body

  def convert(value):
    fraction = F(value)
    if rational:
      return fraction
    import mpmath as mp

    return mp.mpf(fraction.numerator) / fraction.denominator

  def evaluate(node):
    if isinstance(node, ast.Constant) and type(node.value) in (int, float):
      return convert(node.value)
    if isinstance(node, ast.Name) and node.id in values:
      return values[node.id]
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
      return evaluate(node.operand) * (-1 if isinstance(node.op, ast.USub) else 1)
    if isinstance(node, ast.BinOp):
      left, right = evaluate(node.left), evaluate(node.right)
      if isinstance(node.op, ast.Add):
        return left + right
      if isinstance(node.op, ast.Sub):
        return left - right
      if isinstance(node.op, ast.Mult):
        return left * right
      if isinstance(node.op, ast.Div):
        return left / right
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
      arguments = [evaluate(arg) for arg in node.args]
      if node.func.id == "pow" and len(arguments) == 2 and arguments[1] == int(arguments[1]):
        return arguments[0] ** int(arguments[1])
      if node.func.id == "cos" and len(arguments) == 1 and not rational:
        import mpmath as mp

        return mp.cos(arguments[0])
    if isinstance(node, ast.BoolOp) and isinstance(node.op, ast.And):
      return all(evaluate(arg) for arg in node.values)
    if isinstance(node, ast.Compare) and len(node.ops) == 1:
      left, right = evaluate(node.left), evaluate(node.comparators[0])
      op = node.ops[0]
      if isinstance(op, ast.Lt):
        return left < right
      if isinstance(op, ast.LtE):
        return left <= right
      if isinstance(op, ast.Gt):
        return left > right
      if isinstance(op, ast.GtE):
        return left >= right
    raise ValueError("Unsupported clock-source expression")

  return evaluate(tree)
