/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024-2026 Ningqi Wang (0xf3cd)
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

#pragma once


#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <print>
#include <random>

// #69: test draws come from one shared thread_local engine, so every run is reproducible.
// The seed defaults to 42 and can be overridden via the CELESTIAL_TEST_SEED env var; the
// effective seed is printed once per process on first use. Note: only the engine sequence
// is standardized — std::uniform_real_distribution's transform is implementation-defined,
// so float draws can differ across STL vendors.
namespace util::detail {

inline constexpr uint64_t DEFAULT_SEED = 42;

inline auto random_seed() -> uint64_t {
  // Only a fully-consumed, non-negative decimal numeral counts; anything else (unset,
  // non-numeric, zero, trailing junk, negative) falls back to the default.
  const char* const env = std::getenv("CELESTIAL_TEST_SEED");
  if (env == nullptr or *env == '-') {
    return DEFAULT_SEED;
  }
  char* end = nullptr;
  const auto parsed = std::strtoull(env, &end, 10);
  if (end == env or *end != '\0' or parsed == 0) {
    return DEFAULT_SEED;
  }
  return parsed;
}

inline void print_random_seed_once() {
  static std::once_flag flag;
  std::call_once(flag, [] {
    std::println(stderr, "[ util::random ] seed = {} (override: CELESTIAL_TEST_SEED)", random_seed());
  });
}

// One engine per thread, all sharing the same seed — per-thread sequences are deterministic
// and identical, which keeps multi-threaded tests reproducible without locking.
inline auto engine() -> std::mt19937_64& {
  static thread_local auto eng = [] {
    print_random_seed_once();
    // Two-step (not brace-init): clang-tidy's narrowing check misresolves the ctor's
    // parameter type here and fires on a cast that is identity in practice.
    auto e = std::mt19937_64 {};
    e.seed(static_cast<std::mt19937_64::result_type>(random_seed()));
    return e;
  }();
  return eng;
}

} // namespace util::detail

namespace util {

/*!
 * @fn random
 * @brief Generate a random value of type T.
 * @return a random value of type T.
 * @note For floating-point T the semantics are full-domain (#69): a magnitude in
 *       [0, max] with a random sign. (Drawing from [lowest(), max()] directly instead
 *       would overflow the distribution's range.)
 */
template <typename T>
  requires std::integral<T> || std::floating_point<T>
inline auto random() -> T {
  auto& gen = detail::engine();

  if constexpr (std::integral<T>) {
    std::uniform_int_distribution<T> dist { std::numeric_limits<T>::min(), std::numeric_limits<T>::max() };
    return dist(gen);
  } else {
    static_assert(std::floating_point<T>);
    std::uniform_real_distribution<T> dist { T { 0 }, std::numeric_limits<T>::max() };
    std::bernoulli_distribution sign;
    return sign(gen) ? dist(gen) : -dist(gen);
  }
}

/*!
 * @fn random
 * @brief Generate a random value of type T within the specified range [min, max].
 * @param min the lower bound of the range, inclusive.
 * @param max the upper bound of the range, inclusive.
 * @return a random value of type T.
 */
template <typename T>
  requires std::integral<T> || std::floating_point<T>
inline auto random(const T& min, const T& max) -> T {
  assert(min < max);
  auto& gen = detail::engine();

  if constexpr (std::integral<T>) {
    std::uniform_int_distribution<T> dist { min, max };
    return dist(gen);
  } else {
    static_assert(std::floating_point<T>);
    std::uniform_real_distribution<T> dist { min, max };
    return dist(gen);
  }
}

// Specializations for `uint8_t` since clang on Windows doesn't support it
// because this is not part of the C++ standard (for std::uniform_int_distribution).

template <>
inline auto random() -> uint8_t {
  auto& gen = detail::engine();
  std::uniform_int_distribution<uint32_t> dist { 0, 255 };
  return static_cast<uint8_t>(dist(gen));
}

template <>
inline auto random(const uint8_t& min, const uint8_t& max) -> uint8_t {
  assert(min < max);
  auto& gen = detail::engine();
  std::uniform_int_distribution<uint32_t> dist { min, max };
  return static_cast<uint8_t>(dist(gen));
}

} // namespace util
