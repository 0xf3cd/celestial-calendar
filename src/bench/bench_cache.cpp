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

// The cache pays one `hash_combine` per lookup. The mixer's quality is a test
// (`Util.HashCombineAvalanche`); its cost is the old/new pair below (see harness.hpp for why
// the paired ratio is the figure to read).

#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <iostream>
#include <functional>
#include <type_traits>

#include "hash.hpp"
#include "harness.hpp"

namespace {

using Key = std::pair<int32_t, uint8_t>; // stands in for (year, Jieqi) -- Jieqi is an enum class
                                         // over uint8_t, so its std::hash sees identical bits

/** @brief The mixer as it was before the xorshift finalizer, frozen for the paired measurement.
 *         Never sync this with `hash_combine` -- the pair is meaningful only while they differ. */
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

  // Volatile so the accumulated sums -- and the hashing they come from -- survive the optimizer.
  // One read-modify-write per round, not per iteration: a volatile chain inside the loop would
  // set a floor comparable to the hashing itself at ~3 ns a key.
  volatile std::size_t sink = 0;

  // The two bodies differ by exactly one identifier (`old_`), so the pair audits at a glance.
  const bench::Case old_mixer {
    .name = "cache key hash -- old mixer",
    .body = [&](const std::size_t iterations) {
      std::size_t acc = 0;
      std::size_t index = 0;
      for (std::size_t i = 0; i < iterations; ++i) {
        const auto& [year, term] = keys[index];
        acc += old_hash_combine(util::hash::hash(year), term);
        if (++index == keys.size()) {
          index = 0;
        }
      }
      sink = sink + acc;
    },
  };

  const bench::Case new_mixer {
    .name = "cache key hash -- new mixer",
    .body = [&](const std::size_t iterations) {
      std::size_t acc = 0;
      std::size_t index = 0;
      for (std::size_t i = 0; i < iterations; ++i) {
        const auto& [year, term] = keys[index];
        acc += util::hash::hash_combine(util::hash::hash(year), term);
        if (++index == keys.size()) {
          index = 0;
        }
      }
      sink = sink + acc;
    },
  };

  // 240000 iterations: at ~3 ns a key a round is ~0.7 ms, long enough for a round's fixed cost
  // to divide out. bench_jieqi's 24000 was calibrated on a ~20 ns cache hit; borrowed here it
  // left the paired ratio under machine noise (p10..p90 crossing zero).
  const std::array mixer_pair { old_mixer, new_mixer };
  const bench::Plan plan { .title = "Cache key hashing", .iterations = 240000 };
  bench::run(plan, mixer_pair, std::cout);

  return 0;
}
