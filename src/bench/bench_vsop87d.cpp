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

// Solar-term moments come out of a Newton search on the Sun's apparent longitude, and every step of
// that search evaluates Earth's VSOP87D series -- which is most of what this binary times.
// `evaluate` is reported next to the full chain so a change to one can be read against the other.

#include <vector>
#include <cstddef>
#include <iostream>

#include "sun.hpp"
#include "harness.hpp"
#include "julian_day.hpp"
#include "vsop87d/vsop87d.hpp"
#include "vsop87d/defines.hpp"

namespace {

/** @brief JDEs spanning roughly year 0 to year 4000. */
[[nodiscard]] auto sample_jdes() -> std::vector<double> {
  constexpr std::size_t COUNT = 4096;
  std::vector<double> jdes;
  jdes.reserve(COUNT);
  for (std::size_t i = 0; i < COUNT; ++i) {
    const double jc = -20.0 + ((40.0 * static_cast<double>(i)) / static_cast<double>(COUNT));
    jdes.push_back(astro::julian_day::jc_to_jde(jc));
  }
  return jdes;
}

} // namespace


// Nothing downstream reads this exit code, and a bench that cannot build its inputs has
// nothing left to measure -- terminating reports that as loudly as a catch block would.
// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int {
  const auto jdes = sample_jdes();

  // Volatile so neither the sums nor the calls they come from can be optimized away.
  volatile double sink = 0.0;

  const std::vector<bench::Case> cases {
    {
      .name = "vsop87d::evaluate<EAR>",
      .body = [&](const std::size_t iterations) {
        for (std::size_t i = 0; i < iterations; ++i) {
          const auto jm = astro::julian_day::jde_to_jm(jdes.at(i % jdes.size()));
          const auto evaluated = astro::vsop87d::evaluate<astro::vsop87d::Planet::EAR>(jm);
          sink = sink + evaluated.λ + evaluated.β + evaluated.r;
        }
      },
    },
    {
      .name = "sun::geocentric_coord::apparent",
      .body = [&](const std::size_t iterations) {
        for (std::size_t i = 0; i < iterations; ++i) {
          sink = sink + astro::sun::geocentric_coord::apparent(jdes.at(i % jdes.size())).λ.deg();
        }
      },
    },
  };

  const bench::Plan plan {
    .title = "VSOP87D",
    .iterations = jdes.size(),
  };

  bench::run(plan, cases, std::cout);
  return 0;
}
