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

// Two pairs about the cache in util/cache.hpp. The mixer pair: the cache pays one
// `hash_combine` per lookup -- its quality is a test (`Util.HashCombineAvalanche`), its cost
// is the old/new pair below (see harness.hpp for why the paired ratio is the figure to read).
// The copy pair: where a hit's copy happens, inside the lock or outside it, under contention.

#include <span>
#include <array>
#include <mutex>
#include <atomic>
#include <ranges>
#include <thread>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <iostream>
#include <functional>
#include <type_traits>
#include <unordered_map>

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

/** @brief A cached value that is expensive to copy: stands in for `LunarYear`, whose
 *         `month_lengths` makes every cache hit a heap allocation. */
struct HeavyValue {
  std::vector<int32_t> month_lengths;
  int32_t leap_month = 0;
};

/** @brief The two hit-path copy placements, frozen for the paired measurement; `OUT_OF_LOCK`
 *         is the one `make_cached` makes live. Copying after releasing the mutex stands under
 *         the same never-erase premise as `util/cache.hpp`. */
template <bool OUT_OF_LOCK>
struct FrozenHitCache {
  std::mutex mtx;
  std::unordered_map<int32_t, HeavyValue> cache;

  // A lunar year has at most 13 months (12 + a leap month).
  static constexpr std::size_t MONTH_COUNT = 13;

  explicit FrozenHitCache(const std::span<const int32_t> keys) {
    for (const int32_t key : keys) {
      cache.emplace(key, HeavyValue { .month_lengths = std::vector<int32_t>(MONTH_COUNT, key), .leap_month = key });
    }
  }

  [[nodiscard]] auto get(const int32_t key) -> HeavyValue {
    if constexpr (OUT_OF_LOCK) {
      const HeavyValue* found = nullptr;
      {
        const std::scoped_lock lock { mtx };
        found = &cache.at(key);
      }
      return *found;
    } else {
      const std::scoped_lock lock { mtx };
      return cache.at(key);
    }
  }
};

/** @brief One round of contended hits: `THREADS` workers split the iterations over one shared
 *         cache. The copied bytes must survive the optimizer, so the sink folds in the
 *         allocation address itself. */
template <bool OUT_OF_LOCK>
[[nodiscard]] auto contended_hits_body(FrozenHitCache<OUT_OF_LOCK>& cache, const std::vector<int32_t>& keys) {
  return [&cache, &keys](const std::size_t iterations) {
    constexpr std::size_t THREADS = 4;
    std::atomic<std::uintptr_t> sink { 0 };
    const auto worker = [&](const std::size_t n) {
      std::uintptr_t acc = 0;
      for (std::size_t i = 0; i < n; ++i) {
        const auto value = cache.get(keys[i % keys.size()]);
        // Hash the allocation address: the copy must materialize for the address to exist,
        // and `std::hash` on a pointer is lint-clean where a cast is not.
        acc ^= std::hash<const int32_t*> {}(value.month_lengths.data()) & 0xff;
      }
      sink.fetch_xor(acc, std::memory_order_relaxed);
    };
    {
      // Split exactly: the harness contract is that `body` runs precisely `iterations`.
      std::vector<std::jthread> workers;
      workers.reserve(THREADS);
      for (std::size_t t = 0; t < THREADS; ++t) {
        const std::size_t n = (iterations / THREADS) + (t < iterations % THREADS ? 1 : 0);
        workers.emplace_back(worker, n);
      }
    }
  };
}

} // namespace


// Nothing downstream reads this exit code, and a bench that cannot build its inputs has
// nothing left to measure -- terminating reports that as loudly as a catch block would.
// NOLINTNEXTLINE(bugprone-exception-escape)
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

  // 240000 iterations: at ~2 ns a key a round is ~0.5 ms, long enough for a round's fixed cost
  // to divide out. bench_jieqi's 24000 was calibrated on a ~20 ns cache hit; borrowed here it
  // left the paired ratio under machine noise (p10..p90 crossing zero).
  const std::array mixer_pair { old_mixer, new_mixer };
  const bench::Plan plan { .title = "Cache key hashing", .iterations = 240000 };
  bench::run(plan, mixer_pair, std::cout);

  // Where a hit's copy happens. Single-threaded the two placements do the same work; the pair
  // only separates under contention, so the measurement has to be contended (4 workers on one
  // cache). The value mimics `LunarYear`: every hit is a heap allocation.
  constexpr int32_t HEAVY_KEY_COUNT = 240;
  const auto heavy_keys = std::views::iota(int32_t { 0 }, HEAVY_KEY_COUNT) | std::ranges::to<std::vector>();
  FrozenHitCache<false> in_lock_cache { heavy_keys };
  FrozenHitCache<true>  out_lock_cache { heavy_keys };
  const bench::Case in_lock {
    .name = "hit copy -- inside the lock",
    .body = contended_hits_body(in_lock_cache, heavy_keys),
  };
  const bench::Case out_lock {
    .name = "hit copy -- outside the lock",
    .body = contended_hits_body(out_lock_cache, heavy_keys),
  };
  const std::array copy_pair { in_lock, out_lock };
  const bench::Plan copy_plan { .title = "Cache hit copy (4 contended workers)", .iterations = 24000 };
  bench::run(copy_plan, copy_pair, std::cout);

  return 0;
}
