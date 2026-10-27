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

import pytest

from automation.source_identity import cpp_block, read_pinned, require


def test_cpp_block_first_start_includes_balanced_end():
  assert cpp_block("before decl { nested {} } after decl {other}", "decl") == "decl { nested {} }"


@pytest.mark.parametrize("text", ["no declaration", "decl without opening"])
def test_cpp_block_missing_start_or_opening_retains_value_error(text):
  with pytest.raises(ValueError):
    cpp_block(text, "decl")


def test_cpp_block_incomplete_end_is_contextual():
  with pytest.raises(RuntimeError, match=r"C\+\+ block is incomplete: decl"):
    cpp_block("decl { nested {}", "decl")


def test_checked_reads_and_failure_keep_context(tmp_path):
  path = tmp_path / "retained"
  path.write_bytes(b"source")
  assert read_pinned(path, hashlib.sha256(b"source").hexdigest(), "fixture") == b"source"
  with pytest.raises(RuntimeError, match="fixture hash mismatch"):
    read_pinned(path, "0" * 64, "fixture")
  with pytest.raises(RuntimeError, match="input context"):
    require(False, "input context")
