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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

// Every cache lookup pays one `hash_combine`, so the mixer's cost belongs to the cache's budget.
// Its quality is a test (`Util.HashCombineAvalanche`); its cost is the pair below. Both mixers
// live in the same binary and are timed interleaved, because the per-round paired ratio is the
// figure that survives machine drift -- absolute nanoseconds do not (see harness.hpp).

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>
#include <vector>

#include "hash.hpp"
#include "harness.hpp"

namespace {

using Key = std::pair<int32_t, uint8_t>; // stands in for (year, Jieqi), the jieqi cache key

/** @brief The mixer as it was before the xorshift finalizer, frozen for the paired measurement. */
template <typename T>
[[nodiscard]] auto old_hash_combine(std::size_t seed, T&& v) -> std::size_t {
  auto v_hash = std::hash<std::decay_t<T>>{}(std::forward<T>(v));
  v_hash ^= seed * 0xc4ceb9fe1a85ec53;
  v_hash ^= (v_hash >> 13) * 0xff51afd7ed558ccd;
  v_hash *= 0x9e3779b9;
  return v_hash;
}

/** @brief Ten years of solar terms, each key distinct -- the same shape bench_jieqi samples. */
[[nodiscard]] auto sample_keys() -> std::vector<Key> {
  constexpr int32_t FIRST_YEAR = 2000;
  constexpr int32_t YEAR_COUNT = 10;
  constexpr uint8_t TERMS_PER_YEAR = 24;

  std::vector<Key> keys;
  keys.reserve(static_cast<std::size_t>(YEAR_COUNT) * TERMS_PER_YEAR);
  for (int32_t year = FIRST_YEAR; year < FIRST_YEAR + YEAR_COUNT; ++year) {
    for (uint8_t term = 0; term < TERMS_PER_YEAR; ++term) {
      keys.emplace_back(year, term);
    }
  }
  return keys;
}

} // namespace


auto main() -> int {
  const auto keys = sample_keys();

  // Volatile so neither the sums nor the hashing they come from can be optimized away.
  volatile std::size_t sink = 0;

  const bench::Case old_mixer {
    // The old chain, spelled out: hash the first field, then combine the second with the
    // frozen old mixer. Same work as the new case, which calls the public `hash` directly.
    .name = "cache key hash -- old mixer",
    .body = [&](const std::size_t iterations) {
      for (std::size_t i = 0; i < iterations; ++i) {
        const auto& [year, term] = keys.at(i % keys.size());
        sink = sink + old_hash_combine(util::hash::hash(year), term);
      }
    },
  };

  const bench::Case new_mixer {
    .name = "cache key hash -- new mixer",
    .body = [&](const std::size_t iterations) {
      for (std::size_t i = 0; i < iterations; ++i) {
        const auto& [year, term] = keys.at(i % keys.size());
        sink = sink + util::hash::hash(year, term);
      }
    },
  };

  // 24000 iterations per round, per bench_jieqi's calibration: long enough for a round's fixed
  // cost to amortize away, which is what makes the ratio repeatable across runs.
  const std::array mixer_pair { old_mixer, new_mixer };
  const bench::Plan plan { .title = "Cache key hashing", .iterations = 24000 };
  bench::run(plan, mixer_pair, std::cout);

  return 0;
}
