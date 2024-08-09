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

#include <utility>
#include <concepts>
#include <functional>
#include <type_traits>
#include <unordered_map>

namespace util::cache {

#pragma region Hash

template <typename T>
inline auto hash_combine(std::size_t seed, T&& v) -> std::size_t {
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  // Don't lint this code, because clang-tidy compalins about calculating hash for `std::string`.
  return seed ^ (std::hash<std::decay_t<T>>{}(std::forward<T>(v)) * 0x9e3779b9);
  // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
}

template <typename T>
inline auto hash(T&& v) -> std::size_t {
  return std::hash<std::decay_t<T>>{}(std::forward<T>(v));
}

template <typename T, typename... Rest>
inline auto hash(T&& v, Rest&&... rest) -> std::size_t {
  std::size_t seed = hash(std::forward<T>(v));
  (..., (seed = hash_combine(seed, std::forward<Rest>(rest))));
  return seed;
}

/** @brief A concept which ensures the type is tuple-like. */
template <typename T>
concept IsTuple = requires {
  typename std::tuple_size<T>::type;
};

template <IsTuple T>
inline auto hash(T&& t) -> std::size_t {
  return std::apply([](auto&&... args) { return hash(std::forward<decltype(args)>(args)...); }, std::forward<T>(t));
}


template <typename... Args>
struct TupleHash {
  auto operator()(const std::tuple<Args...>& t) const -> std::size_t {
    return std::apply([](const auto&... elems) {
      return hash(elems...);
    }, t);
  }
};

/**
 * @brief A wrapper that caches the result of a function.
 * @param func The function to cache.
 * @return The cached function.
 */
template <typename RetType, typename... Args>
inline auto make_cached(const std::function<RetType(Args...)>& func) -> std::function<RetType(Args...)> {
  std::unordered_map<std::tuple<std::decay_t<Args>...>, RetType, TupleHash<std::decay_t<Args>...>> cache;

  return [cache = std::move(cache), func = func](Args... args) mutable {
    // Create a tuple from the arguments
    auto key = std::make_tuple(std::forward<Args>(args)...);

    // Check if the result is already in the cache
    if (cache.contains(key)) {
      return cache.at(key);
    }
    
    // Compute the result and cache it
    auto result = func(std::forward<Args>(args)...);
    cache[key] = result;
    return result;
  };
}

} // namespace util::cache
