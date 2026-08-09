/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

// The lunar calendar finds twelve or thirteen new moons per year by Newton iteration, and every step
// of that evaluates the Moon's apparent position -- which is most of what this binary times.
// `evaluate` is reported next to the full chain so a change to one can be read against the other.

#include <vector>
#include <cstddef>
#include <iostream>

#include "moon.hpp"
#include "harness.hpp"
#include "julian_day.hpp"
#include "elp2000_82b.hpp"

namespace {

/** @brief Julian centuries spanning roughly year 0 to year 4000. */
[[nodiscard]] auto sample_jcs() -> std::vector<double> {
  constexpr std::size_t COUNT = 4096;
  std::vector<double> jcs;
  jcs.reserve(COUNT);
  for (std::size_t i = 0; i < COUNT; ++i) {
    jcs.push_back(-20.0 + ((40.0 * static_cast<double>(i)) / static_cast<double>(COUNT)));
  }
  return jcs;
}

} // namespace


// Nothing downstream reads this exit code, and a bench that cannot build its inputs has
// nothing left to measure -- terminating reports that as loudly as a catch block would.
// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int {
  const auto jcs = sample_jcs();

  // Volatile so neither the sums nor the calls they come from can be optimized away.
  volatile double sink = 0.0;

  const std::vector<bench::Case> cases {
    {
      .name = "elp2000_82b::evaluate",
      .body = [&](const std::size_t iterations) {
        for (std::size_t i = 0; i < iterations; ++i) {
          const auto evaluated = astro::elp2000_82b::evaluate(jcs.at(i % jcs.size()));
          sink = sink + evaluated.Σl + evaluated.Σb + evaluated.Σr;
        }
      },
    },
    {
      .name = "moon::geocentric_coord::apparent",
      .body = [&](const std::size_t iterations) {
        for (std::size_t i = 0; i < iterations; ++i) {
          const auto jde = astro::julian_day::jc_to_jde(jcs.at(i % jcs.size()));
          sink = sink + astro::moon::geocentric_coord::apparent(jde).λ.deg();
        }
      },
    },
  };

  const bench::Plan plan {
    .title = "ELP2000-82B",
    .iterations = jcs.size(),
  };

  bench::run(plan, cases, std::cout);
  return 0;
}
