#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import re


def canonical_cpp(text: str) -> str:
  without_comments = re.sub(r"//[^\n]*|/\*.*?\*/", " ", text, flags=re.DOTALL)
  return " ".join(without_comments.split())
