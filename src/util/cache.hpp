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
 * @brief What one element of a cache key must support. Element-wise on purpose: `TupleHash`'s
 *        `operator()` takes any tuple and fails inside its body, so constraining *it* would
 *        constrain nothing.
 */
// Spelled as three library concepts rather than one `requires { std::hash<T> {}(v) }`: the latter
// makes g++-14 ICE in `finish_compound_literal` when reached through a nested requires-expression.
template <typename T>
concept CacheKeyElement = std::equality_comparable<T>
                      and std::default_initializable<std::hash<T>>
                      and std::invocable<std::hash<T>&, const T&>;


/** @brief The single call signature of `F` as `R(Args...)`; primary template has no `type`.
 *         The `void_t` route keeps every failure in the immediate (substitution) context —
 *         a generic lambda has no unique `&F::operator()` and dies here cleanly, which is
 *         what the deleted `std::function` CTAD used to do for the same reason. */
template <typename F, typename = void>
struct signature_of {};

template <typename R, typename... Args>
struct signature_of<R(Args...), void> { using type = R(Args...); };

template <typename R, typename... Args>
struct signature_of<R (*)(Args...), void> { using type = R(Args...); };

template <typename R, typename C, typename... Args>
struct signature_of<R (C::*)(Args...) const, void> { using type = R(Args...); };

template <typename F>
struct signature_of<F, std::void_t<decltype(&F::operator())>>
  : signature_of<decltype(&F::operator())> {};

template <typename F>
using signature_of_t = typename signature_of<F>::type;


/** @brief Whether `F` has a single, non-generic call signature. */
template <typename F>
concept SingleSignatureCallable = requires { typename signature_of_t<F>; };


/** @brief Whether a signature `R(Args...)` is cacheable — the key elements and the return
 *         type state what the `unordered_map` below would enforce anyway. */
template <typename Sig>
inline constexpr bool cacheable_signature_v = false;

template <typename R, typename... Args>
inline constexpr bool cacheable_signature_v<R(Args...)> =
    std::copy_constructible<R> and (... and CacheKeyElement<std::decay_t<Args>>);


/** @brief Whether `F` can be invoked as the signature `R(Args...)`, with a convertible result. */
template <typename Sig, typename F>
inline constexpr bool callable_for_signature_v = false;

template <typename R, typename... Args, typename F>
inline constexpr bool callable_for_signature_v<R(Args...), F> =
    std::invocable<const F&, Args...>
    and std::convertible_to<std::invoke_result_t<const F&, Args...>, R>;


namespace detail {

template <typename RetType, typename... Args, typename F>
[[nodiscard]] inline auto make_cached_impl(const F& func) {
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
    // The copy happens outside the lock. Sound because the cache never erases: references
    // into an `unordered_map` survive rehash, only erase invalidates them — and nothing here
    // can erase. Contended hits pay the (heap-backed, for `LunarYear`) copy without holding
    // the mutex: ~2x faster with 4 workers (bench_cache's copy pair).
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

template <typename Sig>
struct expand_signature;

template <typename RetType, typename... Args>
struct expand_signature<RetType(Args...)> {
  template <typename F>
  [[nodiscard]] static auto from(const F& func) {
    return make_cached_impl<RetType, Args...>(func);
  }
};

} // namespace detail


/**
 * @brief A wrapper that caches the result of a function.
 * @param func The function to cache. Must be pure — same arguments ⇒ same result — and have
 *        a single non-generic call signature; a generic lambda is rejected at the call site.
 * @return The cached closure. Thread-safe; copies share one cache (#78).
 * @note The constraints state what the `unordered_map` below already enforces, so misuse is
 *       rejected here instead of inside its instantiation. `copy_constructible` also rules out
 *       `void`, which has nothing to cache.
 * @note The cache **never erases and never evicts** — an entry lives as long as its shared
 *       state. Two things depend on that, and would break silently if an erase ever appeared:
 *       the hit path copies the mapped value after releasing the mutex (see the body), and the
 *       key space stays bounded only by the caller's contract (both call sites guard their
 *       domain upstream: `calc_jieqi_jde` via `find_roots`, `calc_lunar_year` via START/END_YEAR).
 *       No LRU, no `clear`: LRU has no consumer and contradicts the out-of-lock copy; `clear`'s
 *       one consumer (the cold bench) just builds a fresh wrapper per round instead.
 */
template <typename F>
  requires SingleSignatureCallable<F>
       and cacheable_signature_v<signature_of_t<F>>
       and callable_for_signature_v<signature_of_t<F>, F>
[[nodiscard]] inline auto make_cached(const F& func) {
  return detail::expand_signature<signature_of_t<F>>::from(func);
}

} // namespace util::cache
