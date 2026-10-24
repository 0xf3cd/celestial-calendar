#!/usr/bin/env python3
#
# Export the Jieqi (节气) moments over a year range as one JSON table (#164).
#
#########################################################################################
#
# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import sys
import json
import ctypes
import argparse
import hashlib
import math
import tempfile

from ctypes import c_char
from datetime import date
from pathlib import Path
from typing import Final


# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import paths
from automation.abi_layout import load_abi
from automation.library_provenance import find_shared_lib, verify_library


JIEQI_PER_YEAR: Final[int] = 24
# The default table window (#164): 2051 is the tail-margin year, so every moment of 1950–2050
# has its successor inside the table.
DEFAULT_START_YEAR: Final[int] = 1950
DEFAULT_END_YEAR: Final[int] = 2051

# The window this export can actually honor: below 401 the ABI's UT1 chain returns
# `valid = false` (its own floor is JD 1867522.5 = 401-01-01, enforced library-side);
# above 9999 the datetime-based rendering overflows Python's year range.
MIN_YEAR: Final[int] = 401
MAX_YEAR: Final[int] = 9999

MS_PER_DAY: Final[int] = 86_400_000
JD_AT_ORDINAL_ZERO: Final[float] = 1721424.5

TIMESCALE_NOTE: Final[str] = (
  "Moments are modelled UT1 from celestial_query_jieqi_moment, using its fixed Algo5 ΔT convention. "
  "jd_ut1 is a Julian day on that scale; iso_ut1 renders the same civil moment, truncated to milliseconds, "
  "without Z or a time-zone offset. No UTC conversion or leap-second table is used, including before 1972, "
  "near leap seconds and for future dates. These values do not predict future UTC or DUT1."
)


def _check_error(lib: ctypes.CDLL, operation: str) -> None:
  """Check the preserving error kind even when no message could be allocated."""
  if lib.celestial_last_error_kind():
    message = lib.celestial_last_error()
    detail = message.decode("utf-8", errors="replace") if message else "native failure without a message"
    raise RuntimeError(f"{operation}: {detail}")


def _sized_text(lib: ctypes.CDLL, function_name: str, *arguments: int) -> str:
  """Size and fill caller-owned text, checking the including-NUL byte count on both calls."""
  function = getattr(lib, function_name)
  required = function(*arguments, None, 0)
  _check_error(lib, function_name)
  if required < 2:
    raise RuntimeError(f"{function_name} returned an empty or invalid text size")

  buffer = (c_char * required)()
  ctypes.memset(buffer, 0xFF, required)
  filled = function(*arguments, buffer, required)
  _check_error(lib, function_name)
  if filled != required or buffer.raw[-1:] != b"\0" or b"\0" in buffer.raw[:-1]:
    raise RuntimeError(f"{function_name} returned inconsistent text")
  return buffer.raw[:-1].decode("utf-8")


def load_lib(artifact: Path) -> tuple[ctypes.CDLL, dict]:
  """Load a verified private artifact copy and resolve the model metadata from that same library."""
  lib = ctypes.CDLL(str(artifact))
  abi = load_abi()

  for name in (
    "celestial_query_jieqi_moment",
    "celestial_get_jieqi_name",
    "celestial_delta_t",
    "celestial_model_identity_text",
    "celestial_last_error",
    "celestial_last_error_kind",
  ):
    arguments, result = abi.BINDING_SPECS[name]
    function = getattr(lib, name)
    function.argtypes, function.restype = arguments, result

  codes = abi.C_CODES
  model = codes["CELESTIAL_DELTA_T_MODEL_ALGO5"]
  identity = lib.celestial_delta_t(2000.0, model)
  _check_error(lib, "celestial_delta_t")
  if not identity.valid or identity.model_identity_code != model:
    raise RuntimeError("Cannot resolve the fixed Jieqi UT1 conversion model")
  kind = codes["CELESTIAL_MODEL_KIND_DELTA_T"]
  description = _sized_text(lib, "celestial_model_identity_text", kind, identity.model_identity_code)
  return lib, {
    "delta_t": {
      "kind_code": kind,
      "identity_code": identity.model_identity_code,
      "description": description,
    },
    "leap_seconds": None,
  }


def jieqi_name_zh(lib: ctypes.CDLL, jq_idx: int) -> str:
  """Read the library's canonical name without assuming a fixed UTF-8 buffer size."""
  return _sized_text(lib, "celestial_get_jieqi_name", jq_idx)


def ut1_fields(y: int, m: int, d: int, fraction: float) -> tuple[float, str]:
  """Represent validated UT1 civil fields as a Julian day and an unzoned millisecond display."""
  day = date(y, m, d)
  if not math.isfinite(fraction) or not 0 <= fraction < 1:
    raise ValueError("UT1 fraction must be finite and in [0, 1)")
  milliseconds = int(fraction * MS_PER_DAY)
  hour, remainder = divmod(milliseconds, 3_600_000)
  minute, remainder = divmod(remainder, 60_000)
  second, millisecond = divmod(remainder, 1000)
  display = f"{y:04d}-{m:02d}-{d:02d}T{hour:02d}:{minute:02d}:{second:02d}.{millisecond:03d}"
  return day.toordinal() + JD_AT_ORDINAL_ZERO + fraction, display


def generate(start_year: int, end_year: int) -> dict:
  """Query every Jieqi moment in `start_year..end_year` (inclusive) and build the table."""
  if not MIN_YEAR <= start_year <= end_year <= MAX_YEAR:
    raise ValueError(f"year range must satisfy {MIN_YEAR} <= start <= end <= {MAX_YEAR}, got {start_year}..{end_year}")

  artifact = find_shared_lib()
  root = paths.proj_root()
  provenance = verify_library(artifact, root)
  data = artifact.read_bytes()
  if hashlib.sha256(data).hexdigest() != provenance["artifact"]["sha256"]:
    raise RuntimeError("Shared library changed after provenance verification")
  # A unique load path avoids ctypes returning a cached handle to an earlier build (#380).
  with tempfile.TemporaryDirectory(prefix="celestial-jieqi-", ignore_cleanup_errors=True) as directory:
    private = Path(directory) / artifact.resolve().name
    private.write_bytes(data)
    lib, models = load_lib(private)
    result = generate_entries(lib, start_year, end_year)
  if verify_library(artifact, root) != provenance:
    raise RuntimeError("Shared-library provenance changed during export")

  return {
    "schema": "celestial-calendar/jieqi-table@2",
    "source": {
      "repo": "https://github.com/0xf3cd/celestial-calendar",
      "commit": provenance["source"]["commit"],
      "snapshot_sha256": provenance["source"]["snapshot_sha256"],
      "artifact_sha256": provenance["artifact"]["sha256"],
    },
    "parameters": {"start_year": start_year, "end_year": end_year},
    "timescale": "UT1",
    "timescale_note": TIMESCALE_NOTE,
    "models": models,
    "entries": result,
  }


def generate_entries(lib: ctypes.CDLL, start_year: int, end_year: int) -> list[dict]:
  """Query and order the table using one already-verified native library."""
  names = [jieqi_name_zh(lib, idx) for idx in range(JIEQI_PER_YEAR)]

  entries = []
  for year in range(start_year, end_year + 1):
    for idx in range(JIEQI_PER_YEAR):
      query = lib.celestial_query_jieqi_moment(year, idx)
      _check_error(lib, "celestial_query_jieqi_moment")
      if not query.valid:
        raise RuntimeError(f"celestial_query_jieqi_moment({year}, {idx}) returned invalid")
      if query.jq_idx != idx:
        raise RuntimeError(f"celestial_query_jieqi_moment({year}, {idx}) echoed jq_idx {query.jq_idx}")

      jd_ut1, iso_ut1 = ut1_fields(query.y, query.m, query.d, query.frac)
      entries.append(
        {
          "year": year,
          "idx": idx,
          "name_zh": names[idx],
          "jd_ut1": jd_ut1,
          "iso_ut1": iso_ut1,
        }
      )

  # Time order, not index order: within a calendar year the ABI index runs 22, 23, 0, …, 21
  # (小寒/大寒 lead the year), and sorting here saves every consumer from re-sorting (#164).
  entries.sort(key=lambda e: e["jd_ut1"])
  return entries


def serialize(doc: dict) -> str:
  """Byte-stable across runs: no generation timestamp, fixed key order, readable UTF-8."""
  return json.dumps(doc, ensure_ascii=False, indent=2) + "\n"


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Export the Jieqi (节气) moments over a year range as one JSON table.")
  parser.add_argument(
    "--start-year", type=int, default=DEFAULT_START_YEAR, help=f"first year, inclusive (default {DEFAULT_START_YEAR})"
  )
  parser.add_argument(
    "--end-year",
    type=int,
    default=DEFAULT_END_YEAR,
    help=f"last year, inclusive (default {DEFAULT_END_YEAR}: one tail-margin year)",
  )
  parser.add_argument("-o", "--output", type=Path, default=None, help="write the table here instead of stdout")
  args = parser.parse_args()

  text = serialize(generate(args.start_year, args.end_year))
  if args.output is None:
    print(text, end="")
  else:
    args.output.write_text(text, encoding="utf-8")
