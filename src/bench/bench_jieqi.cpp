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

/** @brief Every solar term of ten consecutive years -- 240 distinct keys, none repeated. */
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

  const std::vector<bench::Case> cases {
    {
      // A cache that cannot be emptied -- `util/cache.hpp` still carries the TODO -- can only be
      // made cold by being new, so this builds one per round rather than reusing the global.
      // Per round, not per call: one wrapper construction amortized over `iterations` is
      // invisible next to a Newton search, one per call would not be. Indexing `keys` directly
      // rather than modulo is what keeps every call a miss: no key is asked for twice in a round.
      .name = "jieqi_jde -- cold (every call a miss)",
      .body = [&](const std::size_t iterations) {
        const auto cold = util::cache::cache_func(calendar::jieqi::calc_jieqi_jde);
        for (std::size_t i = 0; i < iterations; ++i) {
          const auto [year, jq] = keys.at(i);
          sink = sink + cold(year, jq);
        }
      },
    },
    {
      // The global cache, whose keys the warm-up has already filled in. Modulo here where the cold
      // case above indexes directly: a hit does not care how often a key repeats, and wrapping is
      // what lets this same case be reused below with far more iterations than there are keys.
      .name = "jieqi_jde -- warm (every call a hit)",
      .body = [&](const std::size_t iterations) {
        for (std::size_t i = 0; i < iterations; ++i) {
          const auto [year, jq] = keys.at(i % keys.size());
          sink = sink + calendar::jieqi::jieqi_jde(year, jq);
        }
      },
    },
  };

  // The pair answers what the cache buys, and 240 iterations is as many as it can answer it with:
  // the cold case pays a Newton search each time, so a round already costs some 64 ms.
  bench::run({ .title = "Solar terms", // `keys.at(i)` above reads `iterations` of them, so they agree.
               .iterations = keys.size() },
             cases, std::cout);

  // And then the hit on its own, with rounds long enough to measure it. At 240 iterations a round's
  // fixed cost is around 600 ns, which lands as +30% on an 8 ns operation and a p10..p90 spanning
  // nearly a factor of two -- fine for "four orders of magnitude", useless for tracking a 10%
  // change to the hash or the lock. At 24000 the same figure repeats to within 1% across runs.
  // Measured, on one machine: 10.6/10.1/10.6 ns and 8.3..15.8 at 240, against 8.1/8.1/8.0 ns and
  // 8.0..9.0 at 24000. The two agree on `min`, which is what says the difference is per-round
  // overhead amortized away rather than the machine wandering between runs.
  const std::vector<bench::Case> hit_only { cases[1] };
  bench::run({ .title = "Solar terms -- cache hit alone", .iterations = 24000 }, hit_only, std::cout);

  return 0;
}
