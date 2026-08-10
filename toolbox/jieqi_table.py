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
# License: GNU General Public License v3.0
# 
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import sys
import json
import re
import ctypes
import argparse

from ctypes import c_int32, c_uint32, c_uint8, c_double, c_bool, c_char, POINTER, Structure
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import Final


# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import paths, run_cmd


JIEQI_PER_YEAR: Final[int] = 24

# The default table window (#164): 2051 is the tail-margin year, so every moment of 1950–2050
# has its successor inside the table.
DEFAULT_START_YEAR: Final[int] = 1950
DEFAULT_END_YEAR:   Final[int] = 2051

# The window this export can actually honor: below 401 the ABI's UT1 chain returns
# `valid = false` (its own floor is JD 1867522.5 = 401-01-01, enforced library-side);
# above 9999 the datetime-based rendering overflows Python's year range.
MIN_YEAR: Final[int] = 401
MAX_YEAR: Final[int] = 9999

MS_PER_DAY: Final[int] = 86_400_000
UNIX_EPOCH: Final[date] = date(1970, 1, 1)

# Honesty over precision (#164): the moments are UT1, and how far that sits from UTC depends
# on the era -- so the table states the gap in three segments instead of one blanket claim.
TIMESCALE_NOTE: Final[str] = (
  "Moments are modelled UT1 (query_jieqi_moment); unix_ms and iso_utc write those UT1 civil "
  "fields onto the proleptic UTC calendar. Before 1972 the library has no UTC to model "
  "(pre-leap-second era) -- read the fields as UT1 civil time. 1972-2017: UT1 matches UTC to "
  "within |DUT1| <= 0.9 s. From 2018 the ΔAT table is frozen at 37 s, and the modelled "
  "UT1-UTC follows (ΔAT+32.184 s)-ΔT, measured -2.57 s at 2050."
)


class _JieqiMomentQuery(Structure):
  """Minimal mirror of `JieqiMomentQuery` in `celestial.h` -- deliberately local (#164:
  sharing the `statistics/common.py` binding is the open follow-up, not this PR)."""
  _fields_ = [
    ("valid",  c_bool),
    ("jq_idx", c_uint8),
    ("y",      c_int32),
    ("m",      c_uint32),
    ("d",      c_uint32),
    ("frac",   c_double),
  ]


def find_shared_lib() -> Path:
  """Locate the built `libcelestial_calendar` under `build/shared_lib` (build it first)."""
  exts = { "win32": ".dll", "darwin": ".dylib", "linux": ".so" }
  if sys.platform not in exts:
    raise OSError(f"Unsupported platform: {sys.platform}")
  ext = exts[sys.platform]

  # Real library outputs only, unversioned or versioned -- a substring test would also take
  # *.dll.manifest / *.dll.a and the like.
  name_re = re.compile(rf"^(?:lib)?celestial_calendar(?:\.\d+)*{re.escape(ext)}(?:\.\d+)*$")
  folder = paths.shared_lib_dir()
  candidates = [
    p for p in folder.iterdir() if p.is_file() and name_re.match(p.name)
  ] if folder.is_dir() else []

  # Prefer the unversioned name (the latest build's link): versioned outputs accumulate
  # in the build dir. The name tiebreak keeps the pick deterministic.
  candidates.sort(key=lambda p: (p.name != f"libcelestial_calendar{ext}", p.name))
  if not candidates:
    raise FileNotFoundError(f"Shared library not found under {folder} -- run ./project.py --build first")
  return candidates[0]


def load_lib() -> ctypes.CDLL:
  """Load the shared library and declare the two entry points this export uses."""
  lib = ctypes.CDLL(str(find_shared_lib()))

  lib.query_jieqi_moment.argtypes = [c_int32, c_uint8]
  lib.query_jieqi_moment.restype = _JieqiMomentQuery

  # `buf` is an output buffer, so it is typed as a pointer rather than `c_char_p` - the latter
  # reads as "takes a string" and invites passing a `bytes`, which the C side would write into.
  lib.get_jieqi_name.argtypes = [c_uint8, POINTER(c_char), c_uint32]
  lib.get_jieqi_name.restype = c_bool

  return lib


def jieqi_name_zh(lib: ctypes.CDLL, jq_idx: int) -> str:
  """Echo the ABI's own canonical name (`get_jieqi_name`), not a new translation (#164)."""
  buf = (c_char * 16)()  # 2-3 UTF-8 bytes per Han character, three characters plus NUL fit.
  if not lib.get_jieqi_name(jq_idx, buf, len(buf)):
    raise RuntimeError(f"get_jieqi_name failed for jq_idx {jq_idx}")
  return buf.value.decode("utf-8")


def to_unix_ms(y: int, m: int, d: int, frac: float) -> int:
  """Milliseconds since the Unix epoch; sub-millisecond truncated, never rounded."""
  return (date(y, m, d) - UNIX_EPOCH).days * MS_PER_DAY + int(frac * MS_PER_DAY)


def iso_from_unix_ms(unix_ms: int) -> str:
  """ISO-8601 rendering of the same millisecond, so the two fields cannot drift apart."""
  dt = datetime(1970, 1, 1) + timedelta(milliseconds=unix_ms)
  return dt.strftime("%Y-%m-%dT%H:%M:%S.") + f"{dt.microsecond // 1000:03d}Z"


def source_commit() -> str:
  """The commit the table is generated from -- provenance, pinned per checkout."""
  ret = run_cmd(
    ["git", "rev-parse", "HEAD"],
    cwd=str(paths.proj_root()), print_cmd=False, print_stdout=False, print_stderr=False,
  )
  if ret.retcode != 0:
    raise RuntimeError("git rev-parse HEAD failed -- the table records its source commit")
  return ret.stdout.strip()


def generate(start_year: int, end_year: int) -> dict:
  """Query every Jieqi moment in `start_year..end_year` (inclusive) and build the table."""
  if not MIN_YEAR <= start_year <= end_year <= MAX_YEAR:
    raise ValueError(
      f"year range must satisfy {MIN_YEAR} <= start <= end <= {MAX_YEAR}, "
      f"got {start_year}..{end_year}"
    )

  lib = load_lib()
  names = [jieqi_name_zh(lib, idx) for idx in range(JIEQI_PER_YEAR)]

  entries = []
  for year in range(start_year, end_year + 1):
    for idx in range(JIEQI_PER_YEAR):
      query = lib.query_jieqi_moment(year, idx)
      if not query.valid:
        raise RuntimeError(f"query_jieqi_moment({year}, {idx}) returned invalid")
      if query.jq_idx != idx:
        raise RuntimeError(f"query_jieqi_moment({year}, {idx}) echoed jq_idx {query.jq_idx}")

      unix_ms = to_unix_ms(query.y, query.m, query.d, query.frac)
      entries.append({
        "year":    year,
        "idx":     idx,
        "name_zh": names[idx],
        "unix_ms": unix_ms,
        "iso_utc": iso_from_unix_ms(unix_ms),
      })

  # Time order, not index order: within a calendar year the ABI index runs 22, 23, 0, …, 21
  # (小寒/大寒 lead the year), and sorting here saves every consumer from re-sorting (#164).
  entries.sort(key=lambda e: e["unix_ms"])

  return {
    "schema": "celestial-calendar/jieqi-table@1",
    "source": {
      "repo":   "https://github.com/0xf3cd/celestial-calendar",
      "commit": source_commit(),
    },
    "parameters": {
      "start_year": start_year,
      "end_year":   end_year,
    },
    "timescale": "UT1",
    "timescale_note": TIMESCALE_NOTE,
    "entries": entries,
  }


def serialize(doc: dict) -> str:
  """Byte-stable across runs: no generation timestamp, fixed key order, readable UTF-8."""
  return json.dumps(doc, ensure_ascii=False, indent=2) + "\n"


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
    description="Export the Jieqi (节气) moments over a year range as one JSON table."
  )
  parser.add_argument("--start-year", type=int, default=DEFAULT_START_YEAR,
                      help=f"first year, inclusive (default {DEFAULT_START_YEAR})")
  parser.add_argument("--end-year", type=int, default=DEFAULT_END_YEAR,
                      help=f"last year, inclusive (default {DEFAULT_END_YEAR}: one tail-margin year)")
  parser.add_argument("-o", "--output", type=Path, default=None,
                      help="write the table here instead of stdout")
  args = parser.parse_args()

  text = serialize(generate(args.start_year, args.end_year))
  if args.output is None:
    print(text, end="")
  else:
    args.output.write_text(text, encoding="utf-8")
