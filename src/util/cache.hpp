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
#include <functional>
#include <type_traits>
#include <unordered_map>

#include "hash.hpp"

namespace util::cache {

using util::hash::TupleHash;

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


/** @brief Checks if a type can be converted to `std::function`. */
template <typename T>
concept FunctionConvertible = requires(T t) {
  { std::function { t } };
};


/**
 * @brief A wrapper that caches the result of a function.
 * @param func The function to cache.
 * @return The cached function.
 */
inline auto cache_func(const FunctionConvertible auto& func) {
  return make_cached(std::function(func));
}

} // namespace util::cache
