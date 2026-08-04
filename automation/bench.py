#!/usr/bin/env python3
# type: ignore
#
# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import os

from pathlib import Path
from typing import List

from . import paths
from .utils import run_cmd, ProcReturn, green_print, red_print, blue_print, yellow_print

BUILD_DIR = paths.build_dir()

# Benchmarks are `EXCLUDE_FROM_ALL`, so `--build` never compiles them and the CI legs never pay for
# them. That also means they have to be asked for by name before they can be run.
BENCH_TARGET = "benchmarks"
BENCH_OUTPUT_DIR = BUILD_DIR / "bench"


def build_benchmarks(cpu_cores: int = 8) -> int:
  """Build the benchmark binaries, which the default target deliberately leaves out."""
  print("#" * 60)

  if not BUILD_DIR.is_dir():
    red_print("# Build directory not found -- run with --cmake first")
    print("#" * 60)
    return 1

  yellow_print(f"# Building the benchmarks (target: {BENCH_TARGET})...")
  ret: ProcReturn = run_cmd(["cmake", "--build", ".", "--target", BENCH_TARGET,
                             "--parallel", str(cpu_cores)],
                            cwd=BUILD_DIR, env=os.environ.copy())

  print("#" * 60)
  return ret.retcode


def find_benchmarks() -> List[Path]:
  """Every executable under the benchmark output directory, sorted by name.

  Discovery is by directory rather than by a hardcoded list so that adding a `bench_*.cpp`
  is the whole job of adding a benchmark (#133).
  """
  if not BENCH_OUTPUT_DIR.is_dir():
    return []
  return sorted(p for p in BENCH_OUTPUT_DIR.iterdir()
                if p.is_file() and os.access(p, os.X_OK))


def run_benchmarks() -> int:
  """Run every benchmark binary and pass their reports through.

  Numbers from different runs of the same binary are comparable; numbers from different machines,
  or from a machine under load, are not. Each report says how it was measured.
  """
  print("#" * 60)

  benchmarks = find_benchmarks()
  if not benchmarks:
    red_print(f"# No benchmark binaries under {BENCH_OUTPUT_DIR}")
    print("#" * 60)
    return 1

  blue_print(f"# Running {len(benchmarks)} benchmark(s)")
  print("#" * 60)

  failed = 0
  for benchmark in benchmarks:
    yellow_print(f"# {benchmark.name}")
    ret: ProcReturn = run_cmd([str(benchmark)], env=os.environ.copy())
    if ret.retcode != 0:
      red_print(f"# {benchmark.name} exited with {ret.retcode}")
      failed += 1

  print("#" * 60)
  if failed == 0:
    green_print(f"# All {len(benchmarks)} benchmark(s) completed")
  else:
    red_print(f"# {failed} of {len(benchmarks)} benchmark(s) failed")
  print("#" * 60)

  return 1 if failed else 0
