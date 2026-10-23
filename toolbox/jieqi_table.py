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

from ctypes import c_int32, c_uint32, c_uint8, c_double, c_bool, c_char, POINTER, Structure
from datetime import date
from pathlib import Path
from typing import Final


# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import paths
from automation.abi_layout import load_abi
from automation.library_provenance import find_shared_lib, verify_library


JIEQI_PER_YEAR: Final[int] = 24
MODEL_KIND_ALL: Final[int] = 0xFFFFFFFF
MODEL_KIND_DELTA_T: Final[int] = 0
MODEL_DEFAULT_ALIAS: Final[int] = 0
CAPABILITY_OPERATION_MODEL_IDENTITY: Final[int] = 4

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
  "Moments are modelled UT1 from query_jieqi_moment, using the library's resolved default ΔT model. "
  "jd_ut1 is a Julian day on that scale; iso_ut1 renders the same civil moment, truncated to milliseconds, "
  "without Z or a time-zone offset. No UTC conversion or leap-second table is used, including before 1972, "
  "near leap seconds and for future dates. These values do not predict future UTC or DUT1."
)


class _JieqiMomentQuery(Structure):
  """Minimal mirror of `JieqiMomentQuery` in `celestial.h`."""

  _fields_ = [
    ("valid", c_bool),
    ("jq_idx", c_uint8),
    ("y", c_int32),
    ("m", c_uint32),
    ("d", c_uint32),
    ("frac", c_double),
  ]


def load_lib(artifact: Path) -> tuple[ctypes.CDLL, dict]:
  """Load a verified private artifact copy and resolve the model metadata from that same library."""
  lib = ctypes.CDLL(str(artifact))
  abi = load_abi()

  lib.query_jieqi_moment.argtypes = [c_int32, c_uint8]
  lib.query_jieqi_moment.restype = _JieqiMomentQuery

  # `buf` is an output buffer, so it is typed as a pointer rather than `c_char_p` - the latter
  # reads as "takes a string" and invites passing a `bytes`, which the C side would write into.
  lib.get_jieqi_name.argtypes = [c_uint8, POINTER(c_char), c_uint32]
  lib.get_jieqi_name.restype = c_bool

  for name in ("celestial_model_identities", "celestial_capability_text", "last_error"):
    arguments, result = abi.BINDING_SPECS[name]
    function = getattr(lib, name)
    function.argtypes, function.restype = arguments, result

  count = c_uint32()
  lib.celestial_model_identities(MODEL_KIND_ALL, ctypes.byref(count), None, 0)
  if not count.value or lib.last_error():
    raise RuntimeError("Cannot resolve the calculation model registry")
  identities = (abi.ModelIdentityRecordV1 * count.value)()
  written = lib.celestial_model_identities(MODEL_KIND_ALL, ctypes.byref(count), identities, len(identities))
  if written != len(identities) or count.value != len(identities):
    raise RuntimeError("Model registry changed during export")
  defaults = [
    (index, identity)
    for index, identity in enumerate(identities)
    if identity.kind_code == MODEL_KIND_DELTA_T and identity.default_alias_code == MODEL_DEFAULT_ALIAS
  ]
  if len(defaults) != 1:
    raise RuntimeError("Cannot identify the default ΔT model")
  ordinal, identity = defaults[0]
  required = lib.celestial_capability_text(CAPABILITY_OPERATION_MODEL_IDENTITY, ordinal, None, 0)
  if not required:
    raise RuntimeError("Cannot read the default ΔT model description")
  description = (c_char * (required + 1))()
  written = lib.celestial_capability_text(CAPABILITY_OPERATION_MODEL_IDENTITY, ordinal, description, len(description))
  if written != required:
    raise RuntimeError("Default ΔT model description changed during export")
  return lib, {
    "delta_t": {
      "kind_code": identity.kind_code,
      "identity_code": identity.identity_code,
      "description": description.value.decode("utf-8"),
    },
    "leap_seconds": None,
  }


def jieqi_name_zh(lib: ctypes.CDLL, jq_idx: int) -> str:
  """Echo the ABI's own canonical name (`get_jieqi_name`), not a new translation (#164)."""
  buf = (c_char * 16)()  # 2-3 UTF-8 bytes per Han character, three characters plus NUL fit.
  if not lib.get_jieqi_name(jq_idx, buf, len(buf)):
    raise RuntimeError(f"get_jieqi_name failed for jq_idx {jq_idx}")
  return buf.value.decode("utf-8")


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
      query = lib.query_jieqi_moment(year, idx)
      if not query.valid:
        raise RuntimeError(f"query_jieqi_moment({year}, {idx}) returned invalid")
      if query.jq_idx != idx:
        raise RuntimeError(f"query_jieqi_moment({year}, {idx}) echoed jq_idx {query.jq_idx}")

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
