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
#include <tuple>
#include <memory>
#include <cstddef>
#include <utility>
#include <concepts>
#include <functional>
#include <type_traits>
#include <unordered_map>

#include "hash.hpp"

namespace util::cache {

using util::hash::TupleHash;

/**
 * @brief What one element of a cache key must support: equality, hashing, and being copyable —
 *        the key is copied into a tuple and again into the map. Element-wise on purpose:
 *        `TupleHash`'s `operator()` takes any tuple and fails inside its body, so
 *        constraining *it* would constrain nothing.
 */
// Spelled as three library concepts rather than one `requires { std::hash<T> {}(v) }`: the latter
// makes g++-14 ICE in `finish_compound_literal` when reached through a nested requires-expression.
template <typename T>
concept CacheKeyElement = std::equality_comparable<T>
                      and std::default_initializable<std::hash<T>>
                      and std::invocable<std::hash<T>&, const T&>
                      and std::copy_constructible<T>;

/**
 * @brief A wrapper that caches the result of a function.
 * @param func The function to cache. Must be pure — same arguments ⇒ same result — and have
 *        a single, non-generic call signature (deduced through `std::function`).
 * @return The cached closure (copies share one cache, #78; not assignable). Thread-safe.
 * @note The constraints state what the `unordered_map` below already enforces, so misuse is
 *       rejected here instead of inside its instantiation. `copy_constructible` also rules out
 *       `void`; `is_object_v` rules out reference returns — the hit path forms `const RetType*`.
 * @note The cache **never erases and never evicts** — an entry lives as long as its shared
 *       state. The hit path's out-of-lock copy depends on that: references into an
 *       `unordered_map` survive rehash, only erase would invalidate them. No LRU and no
 *       `clear`, by the same invariant. The key space is bounded only by the caller's contract.
 */
template <typename RetType, typename... Args>
  requires std::is_object_v<RetType>
       and std::copy_constructible<RetType>
       and (... and CacheKeyElement<std::decay_t<Args>>)
[[nodiscard]] inline auto make_cached(const std::function<RetType(Args...)>& func) {
  // Cache and mutex live behind a `shared_ptr`, so copying the returned
  // closure shares one cache instead of forking divergent replicas (#78).
  struct State {
    std::mutex mtx;
    std::unordered_map<std::tuple<std::decay_t<Args>...>, RetType, TupleHash<std::decay_t<Args>...>> cache;
  };

  return [state = std::make_shared<State>(), func = func](Args... args) -> RetType {
    auto key = std::make_tuple(args...);

    const RetType* hit = nullptr;
    {
      const std::lock_guard lock { state->mtx };
      if (const auto found = state->cache.find(key); found != state->cache.end()) {
        hit = &found->second;
      }
    }
    // Copy outside the lock — sound only under the never-erase invariant (see above).
    if (hit != nullptr) {
      return *hit;
    }

    // Compute outside the lock: misses on different keys don't serialize, and `func`
    // may itself call another cached function without holding two locks at once.
    // Concurrent misses on the same key both compute; `try_emplace` keeps the first.
    // Forward only here — the key above was built from copies, so nothing is moved twice.
    auto result = func(std::forward<Args>(args)...);

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
 * @return The cached closure.
 */
[[nodiscard]] inline auto cache_func(const FunctionConvertible auto& func) {
  return make_cached(std::function(func));
}

} // namespace util::cache
