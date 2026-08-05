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


namespace detail {

/** @brief The single call signature of `F` as `R(Args...)`; the primary template has no `type`.
 *         The `void_t` route keeps every failure in the immediate (substitution) context —
 *         a generic lambda has no unique `&F::operator()` and dies here cleanly, at the call site. */
template <typename F, typename = void>
struct SignatureOf {};

template <typename R, typename... Args>
struct SignatureOf<R(Args...), void> { using type = R(Args...); };

template <typename R, typename... Args>
struct SignatureOf<R(Args...) noexcept, void> { using type = R(Args...); };

template <typename R, typename... Args>
struct SignatureOf<R (*)(Args...), void> { using type = R(Args...); };

template <typename R, typename... Args>
struct SignatureOf<R (*)(Args...) noexcept, void> { using type = R(Args...); };

template <typename R, typename C, typename... Args>
struct SignatureOf<R (C::*)(Args...) const, void> { using type = R(Args...); };

template <typename R, typename C, typename... Args>
struct SignatureOf<R (C::*)(Args...) const noexcept, void> { using type = R(Args...); };

template <typename F>
struct SignatureOf<F, std::void_t<decltype(&F::operator())>>
  : SignatureOf<decltype(&F::operator())> {};

template <typename F>
using SignatureOfT = typename SignatureOf<F>::type;


/** @brief Whether a signature `R(Args...)` is cacheable — states what the `unordered_map`
 *         below enforces anyway, so misuse is rejected at the call site instead of inside
 *         its instantiation (`copy_constructible` also rules out `void`). */
template <typename Sig>
struct CacheableSignature : std::false_type {};

template <typename R, typename... Args>
struct CacheableSignature<R(Args...)>
  : std::bool_constant<std::copy_constructible<R> and (... and CacheKeyElement<std::decay_t<Args>>)> {};


/** @brief Whether `F` invokes as `R(Args...)` with a convertible result — this is what
 *         rejects a pointer-to-member-function (`&Foo::bar`): it has a signature,
 *         but no object to call it on. */
template <typename Sig, typename F>
struct CallableForSignature : std::false_type {};

// Constrained on `invocable`: only then is `invoke_result_t` formed at all.
template <typename R, typename... Args, typename F>
  requires std::invocable<const F&, Args...>
struct CallableForSignature<R(Args...), F>
  : std::bool_constant<std::convertible_to<std::invoke_result_t<const F&, Args...>, R>> {};


/** @brief The whole entry contract of `make_cached`, spelled so every rejection happens
 *         in the immediate context — a constraint failure, never a hard error deeper in. */
template <typename F>
concept CacheableCallable = std::copy_constructible<std::decay_t<F>>
    and CacheableSignature<SignatureOfT<F>>::value
    and CallableForSignature<SignatureOfT<F>, F>::value;


/** @brief Expands a signature `R(Args...)` back into a parameter pack and builds the closure. */
template <typename Sig>
struct ExpandSignature;

template <typename RetType, typename... Args>
struct ExpandSignature<RetType(Args...)> {
  template <typename F>
  [[nodiscard]] static auto from(const F& func) {
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
      // Copy outside the lock — sound only under the never-erase invariant (see `make_cached`).
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
};

} // namespace detail


/**
 * @brief A wrapper that caches the result of a function.
 * @param func The function to cache. Must be pure — same arguments ⇒ same result — and have
 *        a single, non-generic, const call signature: a generic lambda is rejected at the
 *        call site, and so is a `mutable` or ref-qualified `operator()` — a non-const call
 *        is exactly where impurity would live.
 * @return The cached closure (copies share one cache, #78; not assignable). Thread-safe.
 * @note The cache **never erases and never evicts** — an entry lives as long as its shared
 *       state. The hit path's out-of-lock copy depends on that: references into an
 *       `unordered_map` survive rehash, only erase would invalidate them. No LRU and no
 *       `clear`, by the same invariant. The key space is bounded only by the caller's contract.
 */
template <typename F>
  requires detail::CacheableCallable<F>
[[nodiscard]] inline auto make_cached(const F& func) {
  return detail::ExpandSignature<detail::SignatureOfT<F>>::from(func);
}

} // namespace util::cache
