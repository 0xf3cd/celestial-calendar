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

// One solar-term moment costs a Newton search or a map lookup, depending only on whether the answer
// is already in `jieqi_jde`'s cache. Both are timed here because the gap between them is what the
// cache is for, and it is the number any proposal to change the caching has to move.

#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <iostream>

#include "cache.hpp"
#include "jieqi.hpp"
#include "harness.hpp"

namespace {

using Key = std::pair<int32_t, calendar::jieqi::Jieqi>;

/** @brief Every solar term of ten consecutive years, each key distinct. */
[[nodiscard]] auto sample_keys() -> std::vector<Key> {
  constexpr int32_t FIRST_YEAR = 2000;
  constexpr int32_t YEAR_COUNT = 10;

  std::vector<Key> keys;
  keys.reserve(static_cast<std::size_t>(YEAR_COUNT) * calendar::jieqi::JIEQI_COUNT);

  for (int32_t year = FIRST_YEAR; year < FIRST_YEAR + YEAR_COUNT; ++year) {
    for (uint8_t index = 0; index < calendar::jieqi::JIEQI_COUNT; ++index) {
      keys.emplace_back(year, calendar::jieqi::from_index(index));
    }
  }
  return keys;
}

} // namespace


auto main() -> int {
  const auto keys = sample_keys();

  // Volatile so neither the sums nor the calls they come from can be optimized away.
  volatile double sink = 0.0;

  const bench::Case cold {
    // A cache with no way to empty it -- `util/cache.hpp` still carries the TODO -- can only be
    // made cold by being new, so this builds one per round. Per round, not per call: one wrapper
    // construction amortized over `iterations` is invisible next to a Newton search. And `keys` is
    // indexed directly rather than modulo, which is what keeps every call a miss.
    .name = "jieqi_jde -- cold (every call a miss)",
    .body = [&](const std::size_t iterations) {
      const auto uncached = util::cache::cache_func(calendar::jieqi::calc_jieqi_jde);
      for (std::size_t i = 0; i < iterations; ++i) {
        const auto [year, jq] = keys.at(i);
        sink = sink + uncached(year, jq);
      }
    },
  };

  const bench::Case warm {
    // The global cache, whose keys the warm-up has already filled in. Wrapping lets this case run
    // for more iterations than there are keys, which the second round below needs.
    .name = "jieqi_jde -- warm (every call a hit)",
    .body = [&](const std::size_t iterations) {
      for (std::size_t i = 0; i < iterations; ++i) {
        const auto [year, jq] = keys.at(i % keys.size());
        sink = sink + calendar::jieqi::jieqi_jde(year, jq);
      }
    },
  };

  // What the cache buys. `iterations` is bounded by `keys.size()`, because the cold case indexes
  // `keys` directly.
  const std::array paired { cold, warm };
  const bench::Plan paired_plan { .title = "Solar terms", .iterations = keys.size() };
  bench::run(paired_plan, paired, std::cout);

  // What a hit costs, with rounds long enough to say. At 240 iterations a round's fixed cost lands
  // as some +30% on an 8 ns operation, with a p10..p90 spanning nearly a factor of two -- fine for
  // "four orders of magnitude", useless for tracking a 10% change to the hash or the lock.
  // 24000 is where that stops mattering: the figure repeats across runs to within a percent.
  const std::array hit_only { warm };
  const bench::Plan hit_plan { .title = "Solar terms -- cache hit alone", .iterations = 24000 };
  bench::run(hit_plan, hit_only, std::cout);

  return 0;
}
