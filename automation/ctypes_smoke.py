# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import importlib.util
import ctypes

from datetime import date
from types import ModuleType
from typing import Callable, List, Tuple

from . import paths
from .abi_layout import check_runtime_layout
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

  Compare decoded fields against raw C results for the same inputs. External numerical
  references belong to the C++ tests; this gate detects Python field and buffer errors.

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

  if check_runtime_layout(common._ABI, library=common.LIB):
    return 1

  algo1 = common.LunarAlgo.ALGO_1
  algo3 = common.LunarAlgo.ALGO_3

  def valid(result):
    if not result.valid:
      raise RuntimeError("raw parity call failed")
    return result

  def raw_lunar(algo, month, day):
    result = valid(common.LIB.celestial_gregorian_to_lunar(algo.value, 2023, month, day))
    return common.LunarDate(result.year, result.month, result.is_leap, result.day)

  def raw_gregorian(month, leap):
    result = valid(common.LIB.celestial_lunar_to_gregorian(algo1.value, 2023, month, leap, 1))
    return date(result.year, result.month, result.day)

  def raw_range():
    result = valid(common.LIB.celestial_get_supported_lunar_year_range(algo3.value))
    return common.SupportedLunarYearRange(result.start, result.end)

  def raw_illumination():
    result = valid(common.LIB.celestial_moon_illumination(2448724.5))
    return common.MoonIllumination(result.illumination, result.elongation_deg)

  def raw_phases(phase):
    count = ctypes.c_uint32()
    written = common.LIB.celestial_moon_phase_moments(2024, phase.value, ctypes.byref(count), None, 0)
    if written != 0 or count.value == 0:
      raise RuntimeError("raw phase count failed")
    slots = (ctypes.c_double * count.value)()
    written = common.LIB.celestial_moon_phase_moments(2024, phase.value, ctypes.byref(count), slots, len(slots))
    if written != len(slots) or count.value != len(slots):
      raise RuntimeError("raw phase fill failed")
    return list(slots)

  value_cases = [
    ("lunar leap month", lambda: common.gregorian_to_lunar(algo1, 2023, 3, 22), lambda: raw_lunar(algo1, 3, 22)),
    ("lunar ordinary month", lambda: common.gregorian_to_lunar(algo1, 2023, 4, 20), lambda: raw_lunar(algo1, 4, 20)),
    ("lunar algo3", lambda: common.gregorian_to_lunar(algo3, 2023, 3, 22), lambda: raw_lunar(algo3, 3, 22)),
    (
      "Gregorian leap month",
      lambda: common.lunar_to_gregorian(algo1, 2023, 2, True, 1),
      lambda: raw_gregorian(2, True),
    ),
    (
      "Gregorian ordinary month",
      lambda: common.lunar_to_gregorian(algo1, 2023, 3, False, 1),
      lambda: raw_gregorian(3, False),
    ),
    ("supported range", lambda: common.get_supported_lunar_year_range(algo3), raw_range),
    ("illumination fields", lambda: common.moon_illumination(2448724.5), raw_illumination),
    (
      "sidereal value",
      lambda: common.local_apparent_sidereal_time(2460463.0, 120.0),
      lambda: valid(common.LIB.celestial_local_apparent_sidereal_time(2460463.0, 120.0)).value,
    ),
    (
      "position angle",
      lambda: common.moon_position_angle(2448724.5).angle_deg,
      lambda: valid(common.LIB.celestial_moon_position_angle(2448724.5)).angle_deg,
    ),
    *[
      (
        f"phase {phase.name} all elements",
        lambda phase=phase: common.moon_phase_moments(2024, phase).jdes,
        lambda phase=phase: raw_phases(phase),
      )
      for phase in common.MoonPhaseKind
    ],
  ]

  # (label, call) -- invalid inputs must surface as ValueError, not a wrong date.
  raising_cases: List[Tuple[str, Callable[[], object]]] = [
    (
      "lunar_to_gregorian(2024, leap 2nd, 1): 2024 has no leap month",
      lambda: common.lunar_to_gregorian(algo1, 2024, 2, True, 1),
    ),
    (
      "lunar_to_gregorian(2023, leap 5th, 1): 2023's leap month is the 2nd",
      lambda: common.lunar_to_gregorian(algo1, 2023, 5, True, 1),
    ),
    (
      "lunar_to_gregorian(2023, leap 2nd, 30): the leap 2nd month has 29 days",
      lambda: common.lunar_to_gregorian(algo1, 2023, 2, True, 30),
    ),
    ("gregorian_to_lunar(2023-13-01): month out of range", lambda: common.gregorian_to_lunar(algo1, 2023, 13, 1)),
    (
      "local_apparent_sidereal_time(1000000.0, 0): outside the [401, 32766] window",
      lambda: common.local_apparent_sidereal_time(1000000.0, 0.0),
    ),
  ]

  failures: List[str] = []

  for label, call, reference in value_cases:
    try:
      actual = call()
      expected = reference()
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
