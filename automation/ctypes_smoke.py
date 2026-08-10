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

import importlib.util

from datetime import date
from types import ModuleType
from typing import Callable, List, Tuple

from . import paths
from .utils import green_print, red_print, yellow_print


def load_common() -> ModuleType:
  """Import `statistics/common.py`, which loads the real built library at module level.

  Imported from a file location rather than via `sys.path`, so the generic module
  name `common` cannot collide with anything else on the import path.
  """
  common_py = paths.proj_root() / "statistics" / "common.py"
  spec = importlib.util.spec_from_file_location("common", common_py)
  if spec is None or spec.loader is None:
    raise RuntimeError(f"Cannot load module spec from {common_py}")
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


def check_ctypes_smoke() -> int:
  """Run the `statistics/common.py` wrappers against the real built library.

  The ABI layout gate parses `Structure._fields_` without executing anything, and the
  C ABI tests stop at the C boundary -- so neither can see a wrapper body that fills a
  dataclass field from the wrong struct member. The goldens below are the HKO-sourced
  2023 leap-2nd-month pins from `cabi_smoke_test.cpp`, re-read through the Python
  wrappers: if the values come back right end to end, the field plumbing is right.

  Needs the built library (`./project.py --build`): a missing build turns the gate
  red at import, never a silent skip.
  """
  print("#" * 60)
  yellow_print("Smoke-testing the ctypes wrappers against the built library...")

  try:
    common = load_common()
  except Exception as e:
    red_print(f"Importing statistics/common.py failed (run ./project.py --build first): {e}")
    return 1

  algo1 = common.LunarAlgo.ALGO_1
  algo3 = common.LunarAlgo.ALGO_3

  # (label, call, expected return value).
  value_cases: List[Tuple[str, Callable[[], object], object]] = [
    ("gregorian_to_lunar(2023-03-22) -> leap 2nd month, day 1",
     lambda: common.gregorian_to_lunar(algo1, 2023, 3, 22),
     common.LunarDate(year=2023, month=2, is_leap=True, day=1)),
    ("gregorian_to_lunar(2023-04-20) -> 3rd month, day 1",
     lambda: common.gregorian_to_lunar(algo1, 2023, 4, 20),
     common.LunarDate(year=2023, month=3, is_leap=False, day=1)),
    ("gregorian_to_lunar(2023-03-22) via algo3 agrees",
     lambda: common.gregorian_to_lunar(algo3, 2023, 3, 22),
     common.LunarDate(year=2023, month=2, is_leap=True, day=1)),
    ("lunar_to_gregorian(2023, leap 2nd, 1) -> 2023-03-22",
     lambda: common.lunar_to_gregorian(algo1, 2023, 2, True, 1),
     date(2023, 3, 22)),
    ("lunar_to_gregorian(2023, 3rd, 1) -> 2023-04-20",
     lambda: common.lunar_to_gregorian(algo1, 2023, 3, False, 1),
     date(2023, 4, 20)),
    ("algo3 supported range -> (1600, 2199)",
     lambda: common.get_supported_lunar_year_range(algo3),
     common.SupportedLunarYearRange(start=1600, end=2199)),
    # Meeus Example 48.a (1992-04-12 0h TT): both fields must come back from the right
    # struct members — that is the plumbing this gate exists to pin.
    ("moon_illumination(1992-04-12 0h TT) -> Example 48.a",
     lambda: round(common.moon_illumination(2448724.5).illumination, 4),
     0.6786),
    ("moon_illumination elongation_deg from the right member",
     lambda: round(common.moon_illumination(2448724.5).elongation_deg, 4),
     110.8275),
    ("local_apparent_sidereal_time(2460463.0, +120E) -> 190.4627",
     lambda: round(common.local_apparent_sidereal_time(2460463.0, 120.0), 4),
     190.4627),
  ]

  # (label, call) -- invalid inputs must surface as ValueError, not a wrong date.
  raising_cases: List[Tuple[str, Callable[[], object]]] = [
    ("lunar_to_gregorian(2024, leap 2nd, 1): 2024 has no leap month",
     lambda: common.lunar_to_gregorian(algo1, 2024, 2, True, 1)),
    ("lunar_to_gregorian(2023, leap 5th, 1): 2023's leap month is the 2nd",
     lambda: common.lunar_to_gregorian(algo1, 2023, 5, True, 1)),
    ("lunar_to_gregorian(2023, leap 2nd, 30): the leap 2nd month has 29 days",
     lambda: common.lunar_to_gregorian(algo1, 2023, 2, True, 30)),
    ("gregorian_to_lunar(2023-13-01): month out of range",
     lambda: common.gregorian_to_lunar(algo1, 2023, 13, 1)),
    ("local_apparent_sidereal_time(1000000.0, 0): outside the [401, 32767] window",
     lambda: common.local_apparent_sidereal_time(1000000.0, 0.0)),
  ]

  failures: List[str] = []

  for label, call, expected in value_cases:
    try:
      actual = call()
    except Exception as e:
      failures.append(f"{label}: raised {type(e).__name__}: {e}")
      continue
    if actual != expected:
      failures.append(f"{label}: returned {actual!r}, expected {expected!r}")

  for label, call in raising_cases:
    try:
      actual = call()
    except ValueError:
      continue
    except Exception as e:
      failures.append(f"{label}: raised {type(e).__name__}: {e}, expected ValueError")
      continue
    failures.append(f"{label}: returned {actual!r}, expected ValueError")

  print("#" * 60)
  if failures:
    red_print(f"ctypes smoke gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"ctypes wrappers agree with the built library ({len(value_cases) + len(raising_cases)} cases)")
  return 0
