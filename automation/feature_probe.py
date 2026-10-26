# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import os
import tempfile

from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Final, List, Optional

from . import paths
from .utils import run_cmd, green_print, red_print, yellow_print, blue_print


# The standard the project builds with. A feature reachable only under a later -std is not
# usable here, so probing under anything else would report a capability we cannot spend.
CXX_STANDARD: Final[str] = "c++23"


@dataclass(frozen=True)
class Feature:
  """A library feature the codebase is waiting on, and a program that really uses it."""

  name: str  # As written in the code, e.g. "std::views::enumerate".
  token: str  # Substring that identifies this feature in a TODO comment.
  issue: str  # Where the adoption work is tracked.
  program: str  # Must *use* the feature -- a feature-test macro is not evidence (see below).
  deferred: str = ""  # Why it stays hand-rolled even where it compiles. Empty means adopt on sight.


# Every probe compiles a real use of the feature. Reading `__cpp_lib_*` is not enough: libc++
# 210106 implements `std::ranges::fold_left` and does not define `__cpp_lib_ranges_fold`, so a
# macro scan reports a usable feature as missing -- which is exactly how #131 came to be filed
# as "blocked on Apple Clang" when it was not.
FEATURES: Final[List[Feature]] = [
  Feature(
    name="std::tuple_like",
    token="tuple_like",
    issue="#81",
    program="""
      #include <tuple>
      #include <utility>
      template <std::tuple_like T> struct Probe {};
      auto main() -> int {
        Probe<std::pair<int, int>> probe;
        (void) probe;
      }
    """,
  ),
  Feature(
    name="std::generator",
    token="generator",
    issue="#99",
    program="""
      #include <generator>
      auto gen() -> std::generator<int> { co_yield 1; }
      auto main() -> int {
        for (const int v : gen()) { (void) v; }
      }
    """,
  ),
  Feature(
    name="std::views::enumerate",
    token="enumerate",
    issue="#203",
    program="""
      #include <ranges>
      #include <vector>
      auto main() -> int {
        const std::vector<int> v { 1, 2, 3 };
        for (const auto& [i, x] : std::views::enumerate(v)) { (void) i; (void) x; }
      }
    """,
  ),
  Feature(
    name="std::views::pairwise",
    token="pairwise",
    issue="#203",
    program="""
      #include <ranges>
      #include <vector>
      auto main() -> int {
        const std::vector<int> v { 1, 2, 3 };
        for (const auto& pair : v | std::views::pairwise) { (void) pair; }
      }
    """,
  ),
  Feature(
    name="std::views::slide",
    token="slide",
    issue="#203",
    program="""
      #include <ranges>
      #include <vector>
      auto main() -> int {
        const std::vector<int> v { 1, 2, 3 };
        for (const auto& window : v | std::views::slide(2)) { (void) window; }
      }
    """,
  ),
  Feature(
    # C++26, so it stays unavailable until the project's own -std moves too. Kept in the table
    # because two sites already hand-roll around it and this is where they will find out.
    name="std::views::concat",
    token="concat",
    issue="#203",
    program="""
      #include <ranges>
      #include <vector>
      auto main() -> int {
        const std::vector<int> a { 1 };
        const std::vector<int> b { 2 };
        for (const int x : std::views::concat(a, b)) { (void) x; }
      }
    """,
  ),
  Feature(
    # C++26. Wanted by the type-erasure cleanup (#98): `std::function` allocates where a
    # non-owning callable reference would not.
    name="std::function_ref",
    token="function_ref",
    issue="#98",
    program="""
      #include <functional>
      auto twice(std::function_ref<int(int)> f) -> int { return f(1) + f(2); }
      auto main() -> int { return twice([](const int x) { return x; }); }
    """,
  ),
  Feature(
    # The odd row out: nothing waits on this one, the code already calls `std::remainder` from
    # `constexpr` functions and no standard library we target has shipped P0533, which makes
    # evaluating one in a constant expression IF-NDR (#82). Standalone because `compiles()` gets no
    # `-I` -- which also keeps it measuring the library rather than our own call site.
    name="constexpr std::remainder",
    token="remainder_constexpr",
    issue="#82",
    program="""
      #include <cmath>
      constexpr auto normalize_deg(const double deg) -> double {
        const double rem = std::remainder(deg, 360.0);
        return rem < 0.0 ? rem + 360.0 : rem;
      }
      auto main() -> int {
        constexpr double normalized = normalize_deg(361.0);
        static_assert(normalized > 0.99 and normalized < 1.01);
      }
    """,
  ),
]


# What each CI leg supports, measured by this probe -- not inferred from release notes, and not
# guessed. Lost capability fails the gate; new capability is a maintenance notice.
#
# The compiler is only half of what decides a row -- the standard library is the other half,
# and on Linux the two ship separately. Record both, or a runner image bumping its default GCC
# leaves a flipped row with nothing to explain it.
#
#   libstdc++  clang 22.1.2 + ubuntu-26.04's default libstdc++ (GCC 15)
#   libc++     Apple clang 21.0.0 + its bundled libc++
#   msvc-stl   clang 22.1.7 + the MSVC STL on the runner image
# `std::tuple_like` (#81): False on all three legs. libstdc++ exposes only the internal
# `__glibcxx_want_tuple_like` machinery, with nothing under `std::`.
#
# `constexpr std::remainder` (#82): False on all three legs, and the compiler column above is
# why -- every leg here is measured by clang or MSVC, and none of them folds it. GCC does, as a
# builtin rather than because libstdc++ shipped P0533, so a g++ reading does not belong in this
# column at all.
EXPECTED: Final[Dict[str, Dict[str, bool]]] = {
  "libstdc++": {
    "std::tuple_like": False,
    "std::generator": True,
    "std::views::enumerate": True,
    "std::views::pairwise": True,
    "std::views::slide": True,
    "std::views::concat": False,
    "std::function_ref": False,
    "constexpr std::remainder": False,
  },
  "libc++": {
    "std::tuple_like": False,
    "std::generator": False,
    "std::views::enumerate": False,
    "std::views::pairwise": False,
    "std::views::slide": False,
    "std::views::concat": False,
    "std::function_ref": False,
    "constexpr std::remainder": False,
  },
  "msvc-stl": {
    "std::tuple_like": False,
    "std::generator": True,
    "std::views::enumerate": True,
    "std::views::pairwise": True,
    "std::views::slide": True,
    "std::views::concat": False,
    "std::function_ref": False,
    "constexpr std::remainder": False,
  },
}


# Recorded TODO counts expose wrapped markers and new workarounds (#175).
# Drift is a maintenance notice, not a compiler-capability failure.
SITE_COUNTS: Final[Dict[str, int]] = {
  "tuple_like": 0,
  "generator": 2,
  "enumerate": 2,
  "pairwise": 4,
  "slide": 4,
  "concat": 2,
  "function_ref": 0,
  "remainder_constexpr": 0,
}


def todo_sites(token: str) -> List[str]:
  """Find the TODO comments waiting on `token`, with their current line numbers.

  TODO and the feature name must share a line. Wrapped names are not matched.
  """
  src_dir = paths.cpp_src_dir()
  root = paths.proj_root()
  sites: List[str] = []

  for source in sorted(src_dir.rglob("*")):
    if source.suffix not in {".hpp", ".cpp", ".h"}:
      continue
    for number, line in enumerate(source.read_text(encoding="utf-8").splitlines(), start=1):
      if "TODO" in line and token in line:
        sites.append(f"{source.relative_to(root)}:{number}")

  return sites


def adoptable(feature: Feature) -> List[str]:
  """The sites that could stop hand-rolling `feature` today, if any.

  Report marked sites when the feature is usable on every recorded leg and is not deferred.

  Reach: only hand-rolling that carries a `TODO` naming the feature is visible here. An unmarked
  workaround cannot be named.
  """
  if feature.deferred or not all(EXPECTED[leg][feature.name] for leg in EXPECTED):
    return []
  return todo_sites(feature.token)


def site_drift() -> List[str]:
  """Features whose visible TODO-site count no longer matches SITE_COUNTS (#175).

  `todo_sites` derives the sites, so the count is the only thing to hold: a wrapped or
  reworded TODO drops it, a newly tagged workaround raises it. `monuments()` alone only
  catches the count reaching zero; this catches the step before.
  """
  drift = []
  for f in FEATURES:
    visible = todo_sites(f.token)
    recorded = SITE_COUNTS[f.token]
    if len(visible) != recorded:
      drift.append(
        f"{f.name} ({f.issue}): {recorded} recorded, {len(visible)} visible "
        f"({', '.join(visible) if visible else 'none'})"
      )
  return drift


def monuments() -> List[Feature]:
  """Features that are usable everywhere with nothing left waiting on them.

  Such a row can never speak again: `adoptable` finds no sites, and the per-leg comparison only
  fires on a regression the build would catch first. Either it was adopted -- then it belongs in
  the git history, not in this table -- or its workarounds carry no TODO and the gate is blind
  to them. Both need a person, so say so rather than keeping a decoration.
  """
  return [
    f for f in FEATURES if not f.deferred and all(EXPECTED[leg][f.name] for leg in EXPECTED) and not todo_sites(f.token)
  ]


# Nothing exotic -- if this will not compile, the toolchain is broken and every `False` below
# means "the compiler did not run", not "the feature is missing".
CANARY: Final[str] = """
  #include <vector>
  auto main() -> int { const std::vector<int> v { 1 }; return v.empty() ? 1 : 0; }
"""

# The mirror image: a program every real compiler must REJECT. A `CXX` that accepts everything
# (a wrapper script, `/bin/echo`) "compiles" this too -- and then every probe below reports
# USABLE, which the baseline comparison only catches on rows that happen to record False
# (#174). Any real front end stops at the #error, so accepting is the tell, not the verdict.
ANTI_CANARY: Final[str] = """
  #error This program must not compile -- it exists to unmask a CXX that accepts everything.
  auto main() -> int { return 0; }
"""


def compiles(cxx: str, program: str) -> bool:
  """Compile `program` on its own, and report whether the front end accepted it."""
  with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / "probe.cpp"
    source.write_text(program, encoding="utf-8")
    ret = run_cmd(
      [cxx, f"-std={CXX_STANDARD}", "-fsyntax-only", str(source)],
      print_cmd=False,
      print_stdout=False,
      print_stderr=False,
    )
    diagnostics = (ret.stdout + ret.stderr).lower()
    if (
      ret.retcode not in (0, 1)
      or any(
        marker in diagnostics
        for marker in ("internal compiler error", "please submit a bug report", "frontend command failed")
      )
      or (ret.retcode == 1 and not diagnostics.strip())
    ):
      raise RuntimeError(f"Abnormal compiler result (exit {ret.retcode}): {diagnostics.strip()}")
    return ret.retcode == 0


def probe(cxx: str, feature: Feature) -> bool:
  """Compile a program that uses `feature`, and report whether it built."""
  return compiles(cxx, feature.program)


def probe_features(leg: Optional[str] = None) -> int:
  """Report which awaited C++ features this toolchain can actually compile.

  With `leg`, lost capability and malformed records fail. Unlocks and TODO maintenance
  are informational. Without one, capability results have no baseline to regress against.
  """
  print("#" * 60)
  yellow_print("Probing C++ features the codebase is waiting on...")

  cxx = os.environ.get("CXX", "clang++")
  try:
    version = run_cmd([cxx, "--version"], print_cmd=False, print_stdout=False, print_stderr=False)
  except OSError as error:
    red_print(f"Cannot run compiler {cxx}: {error}")
    return 1
  if version.retcode != 0:
    red_print(f"Compiler version query failed (exit {version.retcode}): {version.stderr}")
    return 1
  blue_print(f"# Compiler: {cxx} -std={CXX_STANDARD}")
  blue_print(f"# {(version.stdout or '').splitlines()[0] if version.stdout else 'version unknown'}")

  if leg is not None:
    if leg not in EXPECTED:
      red_print(f"Unknown CI leg '{leg}'. Known legs: {', '.join(sorted(EXPECTED))}")
      return 1
    probed = {f.name for f in FEATURES}
    tokens = {f.token for f in FEATURES}
    if len(probed) != len(FEATURES) or len(tokens) != len(FEATURES):
      red_print("Duplicate feature name or TODO token in FEATURES")
      return 1
    for recorded_leg, baseline in EXPECTED.items():
      if set(baseline) != probed or any(type(value) is not bool for value in baseline.values()):
        red_print(f"Malformed capability baseline for '{recorded_leg}': expected one boolean per feature")
        return 1
    if set(SITE_COUNTS) != tokens or any(type(count) is not int or count < 0 for count in SITE_COUNTS.values()):
      red_print("Malformed SITE_COUNTS: expected one nonnegative integer per feature token")
      return 1

  actual: Dict[str, bool] = {}
  try:
    if not compiles(cxx, CANARY):
      red_print(f"{cxx} cannot compile a trivial -std={CXX_STANDARD} program.")
      yellow_print("Fix the toolchain first; missing features cannot be measured with a broken compiler.")
      return 1
    if compiles(cxx, ANTI_CANARY):
      red_print(f"{cxx} accepted a program that must not compile -- it is not a real compiler.")
      return 1
    for feature in FEATURES:
      actual[feature.name] = probe(cxx, feature)
      (green_print if actual[feature.name] else yellow_print)(
        f"{'USABLE  ' if actual[feature.name] else 'GATED   '}{feature.name}"
      )
  except (OSError, RuntimeError) as error:
    red_print(f"Compiler probe failed: {error}")
    return 1

  print("#" * 60)
  if leg is None:
    green_print(f"{sum(actual.values())} of {len(FEATURES)} probed feature(s) usable here (report only)")
    return 0

  regressed = [f for f in FEATURES if EXPECTED[leg][f.name] and not actual[f.name]]
  unlocked = [f for f in FEATURES if actual[f.name] and not EXPECTED[leg][f.name]]
  if not regressed and not unlocked:
    green_print(f"All {len(FEATURES)} feature(s) match the recorded state of '{leg}'")
  for feature in regressed:
    red_print(f"REGRESSION: {feature.name} is GATED on '{leg}' but was recorded as usable")
  for feature in unlocked:
    yellow_print(f"INFO: {feature.name} is now USABLE on '{leg}' ({feature.issue})")
    for site in todo_sites(feature.token):
      yellow_print(f"  {site}")
  if unlocked:
    yellow_print(f"Record the measured state in EXPECTED['{leg}']; adoption still waits for every leg.")

  for line in site_drift():
    yellow_print(f"INFO: TODO-site count drift (#175): {line}")
  for feature in FEATURES:
    sites = adoptable(feature)
    if sites:
      yellow_print(f"INFO: {feature.name} is usable on every recorded leg ({feature.issue}):")
      for site in sites:
        yellow_print(f"  {site}")
      yellow_print("Adopt it or record the reason for deferral.")
  for feature in monuments():
    yellow_print(f"INFO: {feature.name} has no waiting TODO sites ({feature.issue}); check whether to retire this row.")
  return int(bool(regressed))
