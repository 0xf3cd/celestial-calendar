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

import re

from typing import Final, List, Set

from . import paths
from .export_surface import entry_point_names, self_test_parser
from .utils import green_print, red_print, yellow_print


# Snake-case tokens that may appear in log strings without naming an entry point:
# parameter and struct-field names the messages legitimately quote. A closed set --
# a log string that needs another such token must extend this table deliberately,
# not slip past the gate (#72: log strings name the exported function, or an FFI
# user greps a name that does not exist).
ALLOWED_NON_ENTRY_TOKENS: Final[Set[str]] = {
  "is_leap",
  "jd_ut1",  # local_apparent_sidereal_time's parameter, quoted by its guard messages
  "jq_idx",
  "phase_kind",  # moon_phase_moments's parameter, quoted by its guard messages
  "root_count",
  "slot_count",
}

# Only multi-word snake tokens are held: every entry point — and every function name a
# log string could quote, by repo naming convention — is snake_case, while a lone
# lowercase word cannot be told from English prose, and CamelCase/ALL_CAPS names are
# types or constants, not functions (#72's defect class is function names).
SNAKE_TOKEN_RE: Final[re.Pattern] = re.compile(r"\b[a-z][a-z0-9]*(?:_[a-z0-9]+)+\b")
STRING_LITERAL_RE: Final[re.Pattern] = re.compile(r'"((?:[^"\\]|\\.)*)"')


def check_log_names() -> int:
  """Hold the log strings of `lib_*.cpp` to the entry-point names of `celestial.h`.

  The log messages name functions so an FFI user can grep the exported symbol; a
  message that names an internal or long-gone function sends that user chasing a
  symbol that does not exist. The entry set is parsed from celestial.h (the same
  parser the export-surface gate uses), so the check cannot drift from the header.
  Only multi-word snake_case tokens are held (see SNAKE_TOKEN_RE). Pure parsing --
  no build needed, any leg can run it.
  """
  print("#" * 60)
  yellow_print("Checking that lib_*.cpp log strings name only celestial.h entry points...")

  failures: List[str] = self_test_parser()
  if failures:
    for f in failures:
      red_print(f"  - {f}")
    red_print("Log-names gate is broken (parser self-test failed); fix the parser, not the data")
    return 1

  header = paths.proj_root() / "src" / "shared_lib" / "celestial.h"
  entries = set(entry_point_names(header))
  if not entries:
    red_print(f"No entry points parsed from {header}; the parser is broken")
    return 1
  allowed = entries | ALLOWED_NON_ENTRY_TOKENS

  failures = []
  lib_dir = paths.proj_root() / "src" / "shared_lib"
  for source in sorted(lib_dir.glob("lib*.cpp")):
    for lineno, line in enumerate(source.read_text().splitlines(), start=1):
      if line.lstrip().startswith("#include"):  # an include path is not a log string
        continue
      for literal in STRING_LITERAL_RE.findall(line):
        for token in SNAKE_TOKEN_RE.findall(literal):
          if token not in allowed:
            failures.append(
              f"{source.name}:{lineno}: log string names `{token}`, which is not a "
              f"celestial.h entry point (or an allowed parameter/field name)"
            )

  print("#" * 60)
  if failures:
    red_print(f"Log-names gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"lib_*.cpp log strings name only entry points ({len(entries)} entries)")
  return 0
