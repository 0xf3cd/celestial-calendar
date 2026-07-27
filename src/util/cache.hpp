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

#include <mutex>
#include <memory>
#include <utility>
#include <functional>
#include <type_traits>
#include <unordered_map>

#include "hash.hpp"

namespace util::cache {

using util::hash::TupleHash;

// TODO:
// 1. Add a way to clear the cache (LRU, or something).

/**
 * @brief A wrapper that caches the result of a function.
 * @param func The function to cache. Must be pure — same arguments ⇒ same result.
 * @return The cached function. Thread-safe; copies share one cache (#78).
 */
template <typename RetType, typename... Args>
inline auto make_cached(const std::function<RetType(Args...)>& func) -> std::function<RetType(Args...)> {
  // Cache and mutex live behind a `shared_ptr`, so copying the returned
  // `std::function` shares one cache instead of forking divergent replicas (#78).
  struct State {
    std::mutex mtx;
    std::unordered_map<std::tuple<std::decay_t<Args>...>, RetType, TupleHash<std::decay_t<Args>...>> cache;
  };

  return [state = std::make_shared<State>(), func = func](Args... args) -> RetType {
    auto key = std::make_tuple(args...);

    {
      const std::lock_guard lock { state->mtx };
      const auto found = state->cache.find(key);
      if (found != state->cache.end()) {
        return found->second;
      }
    }

    // Compute outside the lock: misses on different keys don't serialize, and `func`
    // may itself call another cached function without holding two locks at once.
    // Concurrent misses on the same key both compute; `try_emplace` keeps the first.
    auto result = func(args...);

    const std::lock_guard lock { state->mtx };
    return state->cache.try_emplace(std::move(key), std::move(result)).first->second;
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
