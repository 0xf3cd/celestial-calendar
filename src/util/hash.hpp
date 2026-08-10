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

#pragma once

#include <tuple>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <functional>
#include <type_traits>

namespace util::hash {

template <typename T>
[[nodiscard]] inline auto hash_combine(std::size_t seed, T&& v) -> std::size_t {
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  // Don't lint this code, because clang-tidy complains about calculating hash for `std::string`.
  std::uint64_t v_hash = std::hash<std::decay_t<T>>{}(std::forward<T>(v));
  // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

  // Mix in 64 bits regardless of size_t's width (#163): a 32-bit accumulator would UB
  // the >> 32 finalizer on wasm32; on 64-bit this is bit-identical to the old size_t mix.
  // Only cache keys depend on it, so wasm/native key divergence is unobservable.
  v_hash ^= static_cast<std::uint64_t>(seed) * 0xc4ceb9fe1a85ec53;
  v_hash ^= (v_hash >> 13) * 0xff51afd7ed558ccd;

  // A multiply only carries bits upward, so on its own the high half of the input never
  // reaches the low bits -- the xorshifts either side of the golden-ratio multiply bring
  // it down.
  v_hash ^= v_hash >> 32;
  v_hash *= 0x9e3779b97f4a7c15;
  v_hash ^= v_hash >> 32;

  return static_cast<std::size_t>(v_hash);
}

template <typename T>
[[nodiscard]] inline auto hash(T&& v) -> std::size_t {
  return std::hash<std::decay_t<T>>{}(std::forward<T>(v));
}

template <typename T, typename... Rest>
[[nodiscard]] inline auto hash(T&& v, Rest&&... rest) -> std::size_t {
  std::size_t seed = hash(std::forward<T>(v));
  (..., (seed = hash_combine(seed, std::forward<Rest>(rest))));
  return seed;
}

/** @brief A concept which ensures the type is tuple-like. */
template <typename T>
// TODO: Use `std::tuple_like` once every CI leg has it (./linter.py --features). (#81)
concept IsTuple = requires {
  typename std::tuple_size<T>::type;
};

template <IsTuple T>
[[nodiscard]] inline auto hash(T&& t) -> std::size_t {
  return std::apply([](auto&&... args) { return hash(std::forward<decltype(args)>(args)...); }, std::forward<T>(t));
}


template <typename... Args>
struct TupleHash {
  [[nodiscard]] auto operator()(const std::tuple<Args...>& t) const -> std::size_t {
    return std::apply([](const auto&... elems) {
      return hash(elems...);
    }, t);
  }
};

} // namespace util::hash
