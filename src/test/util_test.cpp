/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
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

#include <gtest/gtest.h>

#include <limits>
#include <print>
#include <atomic>
#include <bit>
#include <cmath>
#include <memory>
#include <ranges>
#include <thread>
#include <functional>
#include <unordered_set>
#include "util.hpp"

namespace util::test {

using namespace util;

TEST(Util, ToYmd) {
  using namespace std::literals;

  ASSERT_EQ(to_ymd(1901, 1, 1), 1901y / 1 / 1);
  ASSERT_EQ(to_ymd(2024, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024.0F, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024U, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024LL, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024LLU, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(static_cast<int32_t>(2024), 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(static_cast<int16_t>(2024), 3, 15), 2024y / 3 / 15);
}

/*! @brief Whether `to_ymd(y, m, d)` is a viable call. A concept rather than a bare
           requires-expression: outside a template an ill-formed expression is a hard error,
           not `false`. */
template <typename Y, typename M, typename D>
concept ToYmdCallable = requires (Y y, M m, D d) { to_ymd(y, m, d); };


TEST(Util, ToYmdInputTypes) {
  using namespace std::chrono;

  // #83: month and day are counts, so calendar fields and durations must not reach the body.
  static_assert(not ToYmdCallable<int, int, days>);
  static_assert(not ToYmdCallable<int, int, day>);
  static_assert(not ToYmdCallable<int, month, int>);

  static_assert(ToYmdCallable<int, int, int>);
  static_assert(ToYmdCallable<unsigned, unsigned, unsigned>);
  static_assert(ToYmdCallable<int, std::size_t, uint32_t>);
  static_assert(ToYmdCallable<float, int, int>);
}

TEST(Util, FromYmd) {
  using namespace std::literals;

  {
    const auto [y, m, d] = from_ymd(1901y / 1 / 1);
    ASSERT_EQ(y, 1901);
    ASSERT_EQ(m, 1);
    ASSERT_EQ(d, 1); 
  }

  {
    const auto [y, m, d] = from_ymd(2024y / 3 / 15);
    ASSERT_EQ(y, 2024);
    ASSERT_EQ(m, 3);
    ASSERT_EQ(d, 15); 
  }

  {
    const auto [y, m, d] = from_ymd(0y / 3 / 15);
    ASSERT_EQ(y, 0);
    ASSERT_EQ(m, 3);
    ASSERT_EQ(d, 15); 
  }
}

TEST(Util, OperatorAdd) {
  using namespace std::literals;
  using namespace util::ymd_operator;
  constexpr auto ymd = to_ymd(1901, 1, 1);

  ASSERT_EQ(ymd + std::chrono::days { -365 }, 1900y / 1 / 1);
  ASSERT_EQ(ymd + std::chrono::days { -1 }, 1900y / 12 / 31);
  ASSERT_EQ(ymd + std::chrono::days { 0 }, 1901y / 1 / 1);
  ASSERT_EQ(ymd + std::chrono::days { 1 }, 1901y / 1 / 2);
  ASSERT_EQ(ymd + std::chrono::days { 365 }, 1902y / 1 / 1);

  ASSERT_EQ(ymd + (-365), 1900y / 1 / 1);
  ASSERT_EQ(ymd + (-1), 1900y / 12 / 31);
  ASSERT_EQ(ymd + 0, 1901y / 1 / 1);
  ASSERT_EQ(ymd + 1, 1901y / 1 / 2);
  ASSERT_EQ(ymd + 365, 1902y / 1 / 1);

  ASSERT_EQ(-365 + ymd, 1900y / 1 / 1);
  ASSERT_EQ(-1 + ymd, 1900y / 12 / 31);
  ASSERT_EQ(0 + ymd, 1901y / 1 / 1);
  ASSERT_EQ(1 + ymd, 1901y / 1 / 2);
  ASSERT_EQ(365 + ymd, 1902y / 1 / 1);
}

TEST(Util, OperatorSub) {
  using namespace std::literals;
  using namespace util::ymd_operator;
  
  constexpr auto ymd = to_ymd(1901, 1, 1);

  ASSERT_EQ(ymd - std::chrono::days { 365 }, 1900y / 1 / 1);
  ASSERT_EQ(ymd - std::chrono::days { 1 }, 1900y / 12 / 31);
  ASSERT_EQ(ymd - std::chrono::days { 0 }, 1901y / 1 / 1);
  ASSERT_EQ(ymd - std::chrono::days { -1 }, 1901y / 1 / 2);
  ASSERT_EQ(ymd - std::chrono::days { -365 }, 1902y / 1 / 1);

  ASSERT_EQ(ymd - 365, 1900y / 1 / 1);
  ASSERT_EQ(ymd - 1, 1900y / 12 / 31);
  ASSERT_EQ(ymd - 0, 1901y / 1 / 1);
  ASSERT_EQ(ymd - (-1), 1901y / 1 / 2);
  ASSERT_EQ(ymd - (-365), 1902y / 1 / 1);
}

/*! @brief Whether `util::ymd_operator::operator+(ymd, t)` is viable. Qualified because ADL from
           `year_month_day` reaches only `std::chrono`, and the file's `using namespace util;`
           does not reach into the nested namespace. */
template <typename T>
concept YmdShiftable = requires (std::chrono::year_month_day ymd, T t) {
  util::ymd_operator::operator+(ymd, t);
};

TEST(Util, DaysConvertibleContract) {
  using namespace std::chrono;

  static_assert(DaysConvertible<days>);
  static_assert(DaysConvertible<int>);
  static_assert(DaysConvertible<weeks>);
  static_assert(YmdShiftable<days>);
  static_assert(YmdShiftable<int>);

  // #83: a count of days, not a calendar field -- `days { day { 1 } }` has no viable constructor.
  static_assert(not DaysConvertible<day>);
  static_assert(not DaysConvertible<month>);
  static_assert(not DaysConvertible<double>);
  static_assert(not DaysConvertible<hours>);   // pairs with `weeks` above: exact conversions only
  enum class Season : uint8_t { Spring = 1 };
  static_assert(not DaysConvertible<Season>);

  // The operators bind `const T&`, so the concept must check a const object too -- a conversion
  // that is not const-qualified would otherwise pass here and fail in the body.
  struct NonConstOnly { operator days() { return days { 3 }; } };   // NOLINT(google-explicit-constructor)
  static_assert(not DaysConvertible<NonConstOnly>);
  static_assert(not YmdShiftable<NonConstOnly>);
  static_assert(not YmdShiftable<day>);
}

TEST(Util, GenRandomValue1) {
  // #69: full-domain draws must actually show both signs — this doubles as the mutation
  // guard for the sign branch (drop it and every draw is non-negative).
  std::size_t neg_count = 0;
  std::size_t pos_count = 0;
  for (size_t i = 0; i < 5000; i++) {
    const auto random_value = random<double>();
    ASSERT_TRUE(std::isfinite(random_value));
    if (random_value < 0.0) {
      ++neg_count;
    } else {
      ++pos_count;
    }
  }
  ASSERT_GT(neg_count, 0);
  ASSERT_GT(pos_count, 0);

  std::size_t f_neg_count = 0;
  std::size_t f_pos_count = 0;
  for (size_t i = 0; i < 1000; i++) {
    const auto random_value = random<float>();
    ASSERT_TRUE(std::isfinite(random_value));
    if (random_value < 0.0F) {
      ++f_neg_count;
    } else {
      ++f_pos_count;
    }
  }
  ASSERT_GT(f_neg_count, 0);
  ASSERT_GT(f_pos_count, 0);

  for (size_t i = 0; i < 5000; i++) {
    const auto random_value = random<uint8_t>();
    ASSERT_GE(random_value, std::numeric_limits<uint8_t>::min());
    ASSERT_LE(random_value, std::numeric_limits<uint8_t>::max());
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto random_value = random<int16_t>();
    ASSERT_GE(random_value, std::numeric_limits<int16_t>::min());
    ASSERT_LE(random_value, std::numeric_limits<int16_t>::max());
  }
}


TEST(Util, GenRandomValue2) {
  for (size_t i = 0; i < 5000; i++) {
    // Modest bounds on purpose: full-domain values would overflow the range version's
    // (max - min) to inf (#69).
    const auto random_value1 = random<float>(-1e6F, 1e6F);
    const auto random_value2 = random<float>(-1e6F, 1e6F);
    if (random_value1 == random_value2) {
      continue;
    }
    
    const auto random_value3 = random<float>(
      std::min(random_value1, random_value2), 
      std::max(random_value1, random_value2)
    );
    ASSERT_GE(random_value3, std::min(random_value1, random_value2));
    ASSERT_LE(random_value3, std::max(random_value1, random_value2));
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto random_value1 = random<uint64_t>();
    const auto random_value2 = random<uint64_t>();
    if (random_value1 == random_value2) {
      continue;
    }

    const auto random_value3 = random<uint64_t>(
      std::min(random_value1, random_value2), 
      std::max(random_value1, random_value2)
    );
    ASSERT_GE(random_value3, std::min(random_value1, random_value2));
    ASSERT_LT(random_value3, std::max(random_value1, random_value2));
  }

  for (size_t i = 0; i < 100; i++) {
    const auto gap = random<uint16_t>(1, 20);
    const auto random_value1 = std::invoke([&] {
      while (true) {
        const auto random_value = random<uint16_t>();
        if (random_value < std::numeric_limits<uint16_t>::max() - gap) {
          return random_value;
        }
      }
    });
    const auto random_value2 = random_value1 + gap;

    for (size_t j = 0; j < 100; j++) {
      const auto random_value3 = random<uint16_t>(random_value1, random_value2);
      ASSERT_GE(random_value3, random_value1);
      ASSERT_LE(random_value3, random_value2);
    }
  }
}


TEST(Util, TupleHash) {
  { // Test double, int32_t
    std::unordered_set<std::size_t> s;
    s.insert(hash::hash(0.0, 0));
    s.insert(hash::hash(0.1, 0));
    s.insert(hash::hash(0.1, 1));
    s.insert(hash::hash(0.1, 1));
    ASSERT_EQ(s.size(), 3);
  }

  { // Test int64_t, float, std::string
    std::unordered_set<std::size_t> s;
    s.insert(hash::hash(0LL, 0.0F, ""));
    s.insert(hash::hash(0LL, 0.1F, ""));
    s.insert(hash::hash(0LL, 0.1F, "a"));
    s.insert(hash::hash(0LL, 0.1F, "b"));
    s.insert(hash::hash(0LL, 0.1F, "b"));
    ASSERT_EQ(s.size(), 4);
  }

  { // Test tuple
    std::unordered_set<std::size_t> s;
    s.insert(hash::hash(std::make_tuple(0, 0.0F, "")));
    s.insert(hash::hash(std::make_tuple(0, 0.1F, "")));
    s.insert(hash::hash(std::make_tuple(0, 0.1F, "a")));
    s.insert(hash::hash(std::make_tuple(0, 0.1F, "b")));
    s.insert(hash::hash(std::make_tuple(5, 0.1F, "b")));
    ASSERT_EQ(s.size(), 5);
  }
}


TEST(Util, HashCollision) {
  constexpr auto try_count = 80000;

  std::unordered_set<std::tuple<int, double, float>, hash::TupleHash<int, double, float>> tuples;
  std::unordered_set<std::size_t> hash_values;

  for (auto _ = 0; _ < try_count; _++) {
    const auto v1 = util::random<int>();
    const auto v2 = util::random<double>();
    const auto v3 = util::random<float>();
    tuples.emplace(v1, v2, v3);
    hash_values.insert(hash::hash(v1, v2, v3));
  }

  std::println("{} unique tuples", tuples.size());
  std::println("{} unique hash values", hash_values.size());
  std::println("{} collisions", tuples.size() - hash_values.size());

  ASSERT_NEAR(tuples.size(), hash_values.size(), try_count * 0.00005);
}


/*! @brief A hashable wrapper around an exact bit pattern, so the avalanche test controls the
           mixer's input on every STL -- MSVC's integer `std::hash` is FNV-1a (verified:
           `hash(1)` != 1), which would mask a weak mixer. */
struct Bits {
  std::size_t value;
};

} // namespace util::test

namespace std {

template <>
struct hash<util::test::Bits> {
  [[nodiscard]] auto operator()(const util::test::Bits b) const noexcept -> std::size_t { return b.value; }
};

} // namespace std

namespace util::test {

TEST(Util, HashCombineAvalanche) {
  static_assert(sizeof(std::size_t) == 8, "the 64 input bits below assume a 64-bit size_t");

  constexpr std::size_t SAMPLES = 512;
  // Mean flipped output bits per flipped input bit; ideal is 32 of 64. Calibration (probes at
  // 512 samples/bit, outputs archived in the PR): the previous finalizer -- a 32-bit-constant
  // multiply -- scored as low as 7.7 on high input bits; the current one draws 27.4..33.4
  // across 100 seeds. The floor sits at input bit 7 and is structural, not noise: the
  // retained prefix's `>> 13` leaves the low 13 bits to the single golden-ratio multiply, and
  // bit 7 is the worst-placed of those against the constant's bit pattern. The window absorbs
  // that floor rather than the ideal.
  constexpr double MIN_FLIPS = 26.0;
  constexpr double MAX_FLIPS = 38.0;

  for (std::size_t bit = 0; bit < 64; ++bit) {
    const auto mask = std::size_t { 1 } << bit;
    int value_flips = 0;
    int seed_flips = 0;

    for (std::size_t k = 0; k < SAMPLES; ++k) {
      const auto seed = random<std::size_t>();
      const Bits bits { random<std::size_t>() };
      const auto base = hash::hash_combine(seed, bits);
      value_flips += std::popcount(base ^ hash::hash_combine(seed, Bits { bits.value ^ mask }));
      seed_flips += std::popcount(base ^ hash::hash_combine(seed ^ mask, bits));
    }

    const auto value_mean = static_cast<double>(value_flips) / static_cast<double>(SAMPLES);
    const auto seed_mean = static_cast<double>(seed_flips) / static_cast<double>(SAMPLES);
    EXPECT_GE(value_mean, MIN_FLIPS) << "input bit " << bit;
    EXPECT_LE(value_mean, MAX_FLIPS) << "input bit " << bit;
    EXPECT_GE(seed_mean, MIN_FLIPS) << "seed bit " << bit;
    EXPECT_LE(seed_mean, MAX_FLIPS) << "seed bit " << bit;
  }
}


/*! @brief Whether `make_cached` accepts the callable `F` — spelled as a concept so rejection
           goes through substitution (the `ToYmdCallable` lesson above). */
template <typename F>
concept MakeCachedAccepts = requires (const F& func) { util::cache::make_cached(func); };

/*! @brief Whether `make_cached` accepts a signature. */
template <typename Sig>
concept MakeCachedViable = MakeCachedAccepts<std::function<Sig>>;

TEST(Util, MakeCachedConstraints) {
  enum class Key : uint8_t { A };                      // stands in for `Jieqi` (jieqi.hpp)
  struct Value { int32_t v; };                         // stands in for `LunarYear` (algo2.hpp)
  struct Unhashable { auto operator==(const Unhashable&) const -> bool = default; };

  static_assert(MakeCachedViable<double(int32_t, Key)>);
  static_assert(MakeCachedViable<Value(int32_t)>);

  static_assert(not MakeCachedViable<void(int)>);
  static_assert(not MakeCachedViable<std::unique_ptr<int>(int)>);
  static_assert(not MakeCachedViable<int(Unhashable)>);
  static_assert(not MakeCachedViable<int32_t&(int32_t)>);   // reference return: no `const RetType*`
  static_assert(not MakeCachedViable<int(std::unique_ptr<int>)>); // hashable but not copyable

  // A generic lambda has no single `&F::operator()` — rejected at the call site.
  const auto generic = [](const auto& x) { return x; };
  static_assert(not MakeCachedAccepts<decltype(generic)>);

  // `noexcept` is part of the type since C++17 — the accepted set must not narrow on it.
  const auto noexcept_f = [](const int32_t x) noexcept { return x * 2; };
  static_assert(MakeCachedAccepts<decltype(noexcept_f)>);
  static_assert(MakeCachedAccepts<double(int32_t) noexcept>);
  static_assert(MakeCachedAccepts<double (*)(int32_t) noexcept>);

  // A `mutable` `operator()` has no const signature to extract;
  // a member-function pointer has no object to invoke on.
  const auto mutable_f = [](const int32_t x) mutable { return x * 2; };
  static_assert(not MakeCachedAccepts<decltype(mutable_f)>);

  struct Foo {
    int32_t k = 2; // used below: an unused-this member function is a clang-tidy target
    [[nodiscard]] auto bar(const int32_t x) const { return x * k; }
  };
  static_assert(not MakeCachedAccepts<decltype(&Foo::bar)>);

  // A move-only callable cannot be copied into the closure.
  struct MoveOnly {
    MoveOnly() = default;
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) = default;
    ~MoveOnly() = default;
    auto operator=(const MoveOnly&) -> MoveOnly& = delete;
    auto operator=(MoveOnly&&) -> MoveOnly& = default;
    [[nodiscard]] auto operator()(const int32_t x) const { return x * 2; }
  };
  static_assert(not MakeCachedAccepts<MoveOnly>);
}

TEST(Util, MakeCached1) {
  std::atomic<int32_t> call_count { 0 };
  const auto f = [&call_count](int a, int b) {
    ++call_count;
    return a + b;
  };
  const auto cached_f = util::cache::make_cached(f);

  std::vector<int> original_results;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      original_results.emplace_back(f(i, j));
      original_results.emplace_back(f(i, j));
      original_results.emplace_back(f(i, j));
    }
  }
  ASSERT_EQ(call_count, 300);

  std::vector<int> cached_results;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      cached_results.emplace_back(cached_f(i, j));
      cached_results.emplace_back(cached_f(i, j));
      cached_results.emplace_back(cached_f(i, j));
    }
  }

  ASSERT_EQ(original_results, cached_results);

  // #69/#117: determinism over wall-clock — each unique (i, j) is computed exactly once,
  // so the 300 cached invocations must add exactly 100 underlying calls.
  ASSERT_EQ(call_count, 400);
}


TEST(Util, MakeCached2) {
  std::atomic<int32_t> call_count { 0 };
  const auto f = [&call_count](int a, double b) {
    ++call_count;
    return a * b;
  };
  const auto cached_f = util::cache::make_cached(f);

  std::vector<double> original_results;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      original_results.emplace_back(f(i, j));
      original_results.emplace_back(f(i, j));
      original_results.emplace_back(f(i, j));
    }
  }
  ASSERT_EQ(call_count, 300);

  std::vector<double> cached_results;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      cached_results.emplace_back(cached_f(i, j));
      cached_results.emplace_back(cached_f(i, j));
      cached_results.emplace_back(cached_f(i, j));
    }
  }

  ASSERT_EQ(original_results, cached_results);

  // #69/#117: see MakeCached1 — 300 cached invocations, exactly 100 fresh computations.
  ASSERT_EQ(call_count, 400);
}


TEST(Util, MakeCachedCopyShares) {
  // #78: copies of a cached function must share one cache, not fork divergent replicas.
  std::atomic<int32_t> call_count { 0 };
  const auto f = [&call_count](int32_t a) {
    ++call_count;
    return a * 2;
  };
  const auto cached_f = util::cache::make_cached(f);

  ASSERT_EQ(cached_f(21), 42);
  ASSERT_EQ(call_count, 1);

  const auto copied_f = cached_f; // NOLINT(performance-unnecessary-copy-initialization): the copy is the point.
  ASSERT_EQ(copied_f(21), 42);
  ASSERT_EQ(call_count, 1); // Hit through the copy — no recomputation.

  ASSERT_EQ(copied_f(4), 8);
  ASSERT_EQ(call_count, 2);
  ASSERT_EQ(cached_f(4), 8);
  ASSERT_EQ(call_count, 2); // The original sees the copy's insertion.
}


TEST(Util, MakeCachedThreadSafe) {
  // #78: concurrent callers used to race on the captured `unordered_map` (UB, TSAN-verified).
  std::atomic<int32_t> call_count { 0 };
  const auto f = [&call_count](int32_t a) {
    ++call_count;
    return a * a;
  };
  const auto cached_f = util::cache::make_cached(f);

  constexpr int32_t THREAD_COUNT = 8;
  constexpr int32_t KEY_COUNT = 64;
  constexpr int32_t ROUND_COUNT = 4;

  // gtest assertions are not thread-safe everywhere — threads only record, main thread asserts.
  std::vector<std::vector<int32_t>> results(THREAD_COUNT);

  {
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);
    for (int32_t t = 0; t < THREAD_COUNT; ++t) {
      threads.emplace_back([&cached_f, &results, t] {
        auto& thread_results = results[t];
        thread_results.reserve(static_cast<size_t>(ROUND_COUNT) * KEY_COUNT);
        for (int32_t round = 0; round < ROUND_COUNT; ++round) {
          for (int32_t k = 0; k < KEY_COUNT; ++k) {
            thread_results.emplace_back(cached_f(k));
          }
        }
      });
    }
    for (auto& thread : threads) {
      thread.join();
    }
  }

  for (const auto& thread_results : results) {
    ASSERT_EQ(thread_results.size(), ROUND_COUNT * KEY_COUNT);
    for (int32_t round = 0; round < ROUND_COUNT; ++round) {
      for (int32_t k = 0; k < KEY_COUNT; ++k) {
        ASSERT_EQ(thread_results[(round * KEY_COUNT) + k], k * k);
      }
    }
  }

  // Every key computed at least once; concurrent misses on a key may compute a few extras.
  ASSERT_GE(call_count, KEY_COUNT);
  ASSERT_LE(call_count, THREAD_COUNT * KEY_COUNT);
}


// Seed-override contract: only a fully-consumed non-negative decimal numeral is honored.
// These pass on the pre-#81 `strtoull` implementation too -- a no-regression pin, not a fix.
TEST(Random, ParseSeedAcceptsPlainNumerals) {
  using util::detail::parse_seed;
  ASSERT_EQ(parse_seed("0"), 0U);
  ASSERT_EQ(parse_seed("42"), 42U);
  ASSERT_EQ(parse_seed("0042"), 42U);
  ASSERT_EQ(parse_seed("18446744073709551615"), std::numeric_limits<uint64_t>::max());
}

TEST(Random, ParseSeedRejectsEverythingElse) {
  using util::detail::parse_seed;
  ASSERT_FALSE(parse_seed(nullptr).has_value());          // Unset
  ASSERT_FALSE(parse_seed("").has_value());               // Empty
  ASSERT_FALSE(parse_seed(" 42").has_value());            // Leading space
  ASSERT_FALSE(parse_seed("+42").has_value());            // Explicit plus
  ASSERT_FALSE(parse_seed("-1").has_value());             // Leading minus
  ASSERT_FALSE(parse_seed("42abc").has_value());          // Trailing garbage
  ASSERT_FALSE(parse_seed("0x10").has_value());           // Hex prefix: the `x` after `0` is left unconsumed
  ASSERT_FALSE(parse_seed("1e3").has_value());            // Scientific notation
  ASSERT_FALSE(parse_seed("abc").has_value());            // Not a number at all
  ASSERT_FALSE(parse_seed("18446744073709551616").has_value());  // Overflows uint64
}

} // namespace util::test
