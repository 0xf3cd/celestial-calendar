# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Bind production observation/emission samples to emitted proof pieces and source-derived reserves."""

import ast
from fractions import Fraction as F
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def test_production_samples_stay_in_emitted_pieces_and_source_derived_light_time_rectangles(tmp_path):
  work = tmp_path
  shutil.copytree(ROOT / "src", work / "src")
  instrument = r"""
#include <cstdint>
#include <cstdio>
namespace trace {
inline double time;
inline double left;
inline double right;
inline uint32_t target;
inline bool active = false;
inline auto observation(uint32_t code, double t, double a, double b) -> void {
  time=t; left=a; right=b; target=code; active=true;
  std::printf("O %u %.17g %.17g %.17g\n",code,t,a,b);
}
inline auto emission(double t, double at) -> void {
  if(active && at==time) std::printf("E %u %.17g %.17g %.17g %.17g\n",target,at,t,left,right);
}
}
"""
  (work / "trace.hpp").write_text(instrument)
  replacements = {
    "astro/altitude_event_search.hpp": [
      (
        "    const auto position = astro::ephemeris::equatorial_position(target, jde_tt);",
        "    trace::observation(static_cast<uint32_t>(target), jde_tt, envelope.start_jde_tt, envelope.end_jde_tt);\n"
        "    const auto position = astro::ephemeris::equatorial_position(target, jde_tt);",
      ),
      (
        "        const auto quarter = search::sample(altitude, at, budget);",
        '        std::puts("ROLE quarter");\n        const auto quarter = search::sample(altitude, at, budget);',
      ),
    ],
    "astro/altitude_event_query.hpp": [
      (
        "      const int32_t seed = search::sign("
        "search::sample(provider, scheduled.census.start_jde_tt, budget).evaluated);",
        '      std::puts("ROLE seed");\n      const int32_t seed = search::sign('
        "search::sample(provider, scheduled.census.start_jde_tt, budget).evaluated);",
      ),
      (
        "        const auto observed = provider.observe(event.jde_tt);",
        '        std::puts("ROLE forward");\n        const auto observed = provider.observe(event.jde_tt);',
      ),
      (
        "      static_cast<void>(certify_clock_collar(left, right, bounds.first_derivative, piece.jump));",
        '      std::puts("ROLE collar");\n'
        "      static_cast<void>(certify_clock_collar(left, right, bounds.first_derivative, piece.jump));",
      ),
    ],
    "astro/planet.hpp": [
      (
        "    const auto target = heliocentric(planet, retarded_jde_tt, jde_tt);",
        "    trace::emission(retarded_jde_tt, jde_tt);\n"
        "    const auto target = heliocentric(planet, retarded_jde_tt, jde_tt);",
      ),
    ],
  }
  for name, edits in replacements.items():
    path = work / "src" / name
    text = path.read_text()
    for before, after in edits:
      expected = 2 if before.startswith("      static_cast<void>(certify_clock_collar") else 1
      assert text.count(before) == expected, (name, before)
      text = text.replace(before, after)
    path.write_text(text)
  program = r"""
#include <array>
#include <cmath>
#include <exception>
#include "altitude_events.hpp"
auto main() -> int {
  using namespace astro;
  namespace ae=observer::detail::altitude_events;
  namespace bounds=detail::altitude_bounds;
  namespace search=detail::event_search;
  std::printf("CLOCK %.17g %.17g\n",bounds::CLOCK_START,bounds::CLOCK_END);
  for(const auto& row:bounds::LOCAL_CLOCKS) std::printf("LOCAL %.17g %.17g\n",row.start,row.end);
  const observer::Observer site {
    .location={.latitude=toolbox::AngleDeg{30.0},.longitude=toolbox::AngleDeg{-77.0}},
    .elevation=toolbox::DistanceKm{0.0},
  };
  for(uint32_t target=0;target<10;++target) {
    const observer::AltitudeCondition condition {
      .target=static_cast<ephemeris::Target>(target),
      .level=observer::GeometricLevel{.altitude=toolbox::AngleDeg{0.0}},.sense=observer::AltitudeSense::ABOVE,
    };
    try {static_cast<void>(observer::altitude_events(site,condition,2451545.0,2451546.0));}
    catch(const std::exception& e) {std::printf("FAIL %s\n",e.what());}
    try {static_cast<void>(observer::upper_transits(site,condition.target,2451545.0,2451546.0));}
    catch(const std::exception& e) {std::printf("FAIL %s\n",e.what());}
  }
  const observer::AltitudeCondition far {
    .target=ephemeris::Target::MARS,.level=observer::GeometricLevel{.altitude=toolbox::AngleDeg{0.0}},
    .sense=observer::AltitudeSense::ABOVE,
  };
  const observer::AltitudeCondition collar {
    .target=ephemeris::Target::SUN,.level=observer::GeometricLevel{.altitude=toolbox::AngleDeg{89.0}},
    .sense=observer::AltitudeSense::ABOVE,
  };
  try {static_cast<void>(observer::altitude_events(site,collar,2451544.4,2451544.6));}
  catch(const std::exception& e) {std::printf("FAIL %s\n",e.what());}
  for(const auto& row:bounds::LOCAL_CLOCKS) {
    try {static_cast<void>(observer::altitude_events(site,far,std::nextafter(row.start,row.end),row.end));}
    catch(const std::exception& e) {std::printf("FAIL %s\n",e.what());}
  }
  ae::Altitude provider {
    .target=ephemeris::Target::SUN,.observer=site,.level_sine=0.0,.residual_error=0x1p-40,
    .envelope={.start_jde_tt=2451545.0,.end_jde_tt=2451545.25},.meridian=false,
  };
  const auto middle=provider.observe(2451545.125);
  provider.level_sine=middle.up/middle.q;
  search::Budget budget{.evaluations_remaining=100,.candidates_remaining=100};
  const search::Candidate candidate {
    .left=search::sample(provider,2451545.0,budget),.right=search::sample(provider,2451545.25,budget),
    .isolation=search::Isolation::UNIQUE,
  };
  static_cast<void>(ae::prepare_refinement(provider,candidate,{.first_derivative=100.0,.second_derivative=1000.0},budget,0x1p-20));
}
"""
  (work / "probe.cpp").write_text(program)
  compiler = shlex.split(os.environ.get("CXX", "clang++"))
  command = [
    *compiler,
    "-std=c++23",
    "-O2",
    "-include",
    str(work / "trace.hpp"),
    *["-I" + str(work / "src" / part) for part in ("astro", "calendar", "util")],
    str(work / "probe.cpp"),
    "-o",
    str(work / "probe"),
  ]
  built = subprocess.run(command, capture_output=True, text=True)
  (work / "compile.log").write_text(built.stdout + built.stderr)
  assert built.returncode == 0, str(work)
  result = subprocess.run([str(work / "probe")], capture_output=True, text=True)
  (work / "trace.log").write_text(result.stdout + result.stderr)
  assert result.returncode == 0
  clock = None
  local = []
  observations, emissions, roles = [], [], set()
  for line in result.stdout.splitlines():
    if line.startswith("CLOCK "):
      clock = tuple(F(value) for value in line.split()[1:])
    elif line.startswith("LOCAL "):
      local.append(tuple(F(value) for value in line.split()[1:]))
    elif line.startswith("O "):
      observations.append((int(line.split()[1]), *(F(value) for value in line.split()[2:])))
    elif line.startswith("E "):
      emissions.append((int(line.split()[1]), *(F(value) for value in line.split()[2:])))
    elif line.startswith("ROLE "):
      roles.add(line.split()[1])
  assert roles == {"seed", "forward", "collar", "quarter"}, roles
  assert observations and emissions and clock is not None and len(local) == 2
  source = (ROOT / "statistics/altitude_event_bounds.py").read_text()
  radii = {}
  for node in ast.walk(ast.parse(source)):
    if isinstance(node, ast.Assign) and isinstance(node.value, ast.Call):
      call = node.value
      if isinstance(call.func, ast.Name) and call.func.id == "derive_vsop_record":
        assignment = node.targets[0]
        assert isinstance(assignment, ast.Subscript) and isinstance(assignment.value, ast.Name)
        radius = call.args[-1]
        assert isinstance(radius, ast.Call) and radius.func.id == "F"
        radii[assignment.value.id] = F(*(ast.literal_eval(arg) for arg in radius.args))
  assert set(radii) == {"near_raw", "far_raw"}
  for _, at, left, right in observations:
    assert left <= at <= right
    assert clock[0] <= left < right <= clock[1] or any(a <= left < right <= b for a, b in local)
  assert any(left < F(2451545) for _, _, left, _ in observations), "preceding-cell samples absent"
  for _, at, emitted, left, right in emissions:
    assert left <= at <= right and emitted <= at
    radius = radii["near_raw"] if clock[0] <= left < right <= clock[1] else radii["far_raw"]
    assert abs(emitted - 2451545) / 365250 <= radius
  sys.path.insert(0, str(ROOT / "statistics"))
  from altitude_bounds import coefficient_reader as reader
  from altitude_bounds.local_apparent_value import local_rounded_geometry
  from altitude_bounds.local_vector_separation import raw_cartesian_enclosure
  from altitude_bounds.source_c2_records import derive_vsop_record

  precession = re.sub(
    r"/\*.*?\*/|//[^\n]*", " ", (ROOT / "src/astro/earth/precession.hpp").read_text(), flags=re.DOTALL
  )
  tables, records = {}, {}
  for name in ("earth", "mars"):
    path = ROOT / "src/astro/vsop87d" / (name + "_coeff.hpp")
    tables[name] = reader.read_coefficients(path)
    records[name] = derive_vsop_record(name, path, tables[name], radii["far_raw"])
  reserves = []
  for left, right in local:
    centre, half = (left + right) / 2, (right - left) / 2
    earth = raw_cartesian_enclosure(tables["earth"], centre)
    mars = raw_cartesian_enclosure(tables["mars"], centre)
    point = tuple(a - b for a, b in zip(mars, earth, strict=True))
    geometry = local_rounded_geometry(records["mars"], records["earth"], centre, half, point, precession)
    reserve = geometry["reserve_days"]
    matching = [row for row in emissions if row[0] == 4 and left <= row[3] < row[4] <= right]
    assert matching, "far emission trace absent"
    assert all(left - reserve <= emitted <= right for _, _, emitted, _, _ in matching)
    reserves.append({"left": str(left), "right": str(right), "reserve": str(reserve), "emissions": len(matching)})
  record = {
    "work": str(work),
    "observations": len(observations),
    "emissions": len(emissions),
    "roles": sorted(roles),
    "source_radii": {name: str(value) for name, value in radii.items()},
    "local_rectangles": reserves,
    "exit": result.returncode,
    "scope": (
      "Production query samples traced in copied source. Observations stay in emitted proof pieces; "
      "VSOP emission times stay in the generator source radius and both far Mars invariant rectangles, "
      "recomputed from coefficient sources. Execution binding only, not proof by sampling."
    ),
  }
  (work / "receipt.json").write_text(json.dumps(record, indent=2) + "\n")
  print(json.dumps(record), flush=True)
