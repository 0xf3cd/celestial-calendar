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

  name: str        # As written in the code, e.g. "std::views::enumerate".
  token: str       # Substring that identifies this feature in a TODO comment.
  issue: str       # Where the adoption work is tracked.
  program: str     # Must *use* the feature -- a feature-test macro is not evidence (see below).
  deferred: str = ""  # Why it stays hand-rolled even where it compiles. Empty means adopt on sight.


# Every probe compiles a real use of the feature. Reading `__cpp_lib_*` is not enough: libc++
# 210106 implements `std::ranges::fold_left` and does not define `__cpp_lib_ranges_fold`, so a
# macro scan reports a usable feature as missing -- which is exactly how #131 came to be filed
# as "blocked on Apple Clang" when it was not.
FEATURES: Final[List[Feature]] = [
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
    name="std::ranges::fold_left",
    token="fold_left",
    issue="#131",
    deferred=("compiles everywhere, but MS STL's result drifts 1-3 ulp from the hand-rolled sum "
              "from n~63 up, which would fork the golden data per platform (#131)"),
    program="""
      #include <algorithm>
      #include <functional>
      #include <vector>
      auto main() -> int {
        const std::vector<int> v { 1, 2, 3 };
        return std::ranges::fold_left(v, 0, std::plus {}) == 6 ? 0 : 1;
      }
    """,
  ),
  Feature(
    name="std::views::enumerate",
    token="enumerate",
    issue="#131",
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
    issue="#131",
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
    issue="#131",
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
    name="std::views::join_with",
    token="join_with",
    issue="#131",
    program="""
      #include <ranges>
      #include <string>
      #include <vector>
      auto main() -> int {
        const std::vector<std::string> parts { "a", "b" };
        for (const char c : parts | std::views::join_with(std::string { ", " })) { (void) c; }
      }
    """,
  ),
  Feature(
    # C++26, so it stays unavailable until the project's own -std moves too. Kept in the table
    # because two sites already hand-roll around it and this is where they will find out.
    name="std::views::concat",
    token="concat",
    issue="#131",
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
]


# What each CI leg supports, measured by this probe on 2026-08-02 -- not inferred from release
# notes, and not guessed. A mismatch is the point of the gate: it fires the day a runner's
# toolchain gains one of these, which is the day the waiting TODOs become actionable, and
# nothing else in the repo would ever notice.
#
#   libstdc++  clang 18.1.3 on ubuntu-24.04
#   libc++     Apple clang 21.0.0 on macos-latest
#   msvc-stl   clang 20.1.8 on windows-latest
EXPECTED: Final[Dict[str, Dict[str, bool]]] = {
  "libstdc++": {
    "std::generator": False,
    "std::ranges::fold_left": True,
    "std::views::enumerate": True,
    "std::views::pairwise": True,
    "std::views::slide": True,
    "std::views::join_with": True,
    "std::views::concat": False,
    "std::function_ref": False,
  },
  "libc++": {
    "std::generator": False,
    "std::ranges::fold_left": True,
    "std::views::enumerate": False,
    "std::views::pairwise": False,
    "std::views::slide": False,
    "std::views::join_with": True,
    "std::views::concat": False,
    "std::function_ref": False,
  },
  "msvc-stl": {
    "std::generator": True,
    "std::ranges::fold_left": True,
    "std::views::enumerate": True,
    "std::views::pairwise": True,
    "std::views::slide": True,
    "std::views::join_with": True,
    "std::views::concat": False,
    "std::function_ref": False,
  },
}


def todo_sites(token: str) -> List[str]:
  """Find the TODO comments waiting on `token`, with their current line numbers.

  Derived, not listed: a hand-maintained site list would go stale between the day a feature
  unlocks and the day someone reads this file -- which is the whole interval this gate exists
  to cover.
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

  Detecting *changes* is not enough on its own. Once a feature is recorded as usable everywhere
  the comparison above goes quiet forever, and the TODOs it unblocked would sit there unread --
  the exact failure mode this gate replaces. So the recorded state is also read forwards: usable
  on every leg, nothing recorded holding it back, and sites still working around it.
  """
  if feature.deferred or not all(EXPECTED[leg][feature.name] for leg in EXPECTED):
    return []
  return todo_sites(feature.token)


def probe(cxx: str, feature: Feature) -> bool:
  """Compile a program that uses `feature`, and report whether it built."""
  with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / "probe.cpp"
    source.write_text(feature.program, encoding="utf-8")
    ret = run_cmd(
      [cxx, f"-std={CXX_STANDARD}", "-fsyntax-only", str(source)],
      print_cmd=False,
      print_stdout=False,
      print_stderr=False,
    )
    return ret.retcode == 0


def probe_features(leg: Optional[str] = None) -> int:
  """Report which awaited C++ features this toolchain can actually compile.

  With `leg` naming a CI leg, the result is compared against `EXPECTED` and a difference is
  an error. Without one, the probe only reports -- a local toolchain is nobody's baseline.
  """
  print("#" * 60)
  yellow_print("Probing C++ features the codebase is waiting on...")

  cxx = os.environ.get("CXX", "clang++")
  version = run_cmd([cxx, "--version"], print_cmd=False, print_stdout=False, print_stderr=False)
  blue_print(f"# Compiler: {cxx} -std={CXX_STANDARD}")
  blue_print(f"# {(version.stdout or '').splitlines()[0] if version.stdout else 'version unknown'}")

  if leg is not None:
    if leg not in EXPECTED:
      red_print(f"Unknown CI leg '{leg}'. Known legs: {', '.join(sorted(EXPECTED))}")
      return 1
    # A feature added to FEATURES but not to EXPECTED would otherwise read as "just unlocked"
    # on every leg forever, which is a gate that cries wolf rather than one that has a baseline.
    unrecorded = [f.name for f in FEATURES if f.name not in EXPECTED[leg]]
    if unrecorded:
      red_print(f"No recorded state on '{leg}' for: {', '.join(unrecorded)}")
      yellow_print(f"Run `./linter.py --features` on that toolchain and add the result to EXPECTED['{leg}'].")
      return 1

  actual: Dict[str, bool] = {}
  for feature in FEATURES:
    actual[feature.name] = probe(cxx, feature)
    (green_print if actual[feature.name] else yellow_print)(
      f"{'USABLE  ' if actual[feature.name] else 'GATED   '}{feature.name}"
    )

  print("#" * 60)
  if leg is None:
    green_print(f"{sum(actual.values())} of {len(FEATURES)} probed feature(s) usable here (report only)")
    return 0

  changed = [f for f in FEATURES if actual[f.name] != EXPECTED[leg][f.name]]
  if not changed:
    green_print(f"All {len(FEATURES)} feature(s) match the recorded state of '{leg}'")

    ready = {f: sites for f in FEATURES if (sites := adoptable(f))}
    if not ready:
      return 0

    red_print("These compile on every leg, and the code is still working around them:")
    for feature, sites in ready.items():
      red_print(f"  {feature.name} ({feature.issue}) -- {len(sites)} site(s):")
      for site in sites:
        red_print(f"    {site}")
    yellow_print("Adopt them and drop the TODO, or record why not in that Feature's `deferred` field.")
    return 1

  red_print(f"The toolchain on '{leg}' no longer matches what this repo recorded:")
  for feature in changed:
    if actual[feature.name]:
      green_print(f"  + {feature.name} is now USABLE (recorded as gated)")
      sites = todo_sites(feature.token)
      if sites:
        yellow_print(f"    Adopt it at {len(sites)} waiting site(s), then delete the TODO:")
        for site in sites:
          yellow_print(f"      {site}")
      else:
        yellow_print(f"    Adoption is tracked in {feature.issue}.")
    else:
      red_print(f"  - {feature.name} is GATED but was recorded as usable -- the toolchain regressed.")

  yellow_print("Then set the new state in EXPECTED['%s'] in automation/feature_probe.py." % leg)
  return 1
