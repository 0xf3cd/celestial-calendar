# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import hashlib

from pathlib import Path


def require(condition: bool, message: str) -> None:
  """Raise a runtime failure with the caller's source context."""
  if not condition:
    raise RuntimeError(message)


def read_pinned(path: Path, expected_sha256: str, label: str) -> bytes:
  """Read exact retained bytes, naming the source on an identity mismatch."""
  data = path.read_bytes()
  digest = hashlib.sha256(data).hexdigest()
  require(digest == expected_sha256, f"{label} hash mismatch: {digest}")
  return data


def cpp_block(text: str, declaration: str) -> str:
  """Include the first declaration's balanced braces; missing starts retain ValueError."""
  start = text.index(declaration)
  opening = text.index("{", start)
  depth = 0
  for index in range(opening, len(text)):
    if text[index] == "{":
      depth += 1
    elif text[index] == "}":
      depth -= 1
      if depth == 0:
        return text[start : index + 1]
  raise RuntimeError(f"C++ block is incomplete: {declaration}")
