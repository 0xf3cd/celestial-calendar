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

#include <atomic>
#include <cstdint>
#include <functional>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// Internal helpers for the C-ABI layer (`lib*.cpp`): an atomic verbosity knob, never-throwing
// logging safe to call inside catch handlers, and a thread-local last-error channel.

namespace lib {

/** @enum Verbosity represents the verbosity level of log printing. */
enum class Verbosity : uint8_t {
  NONE  = 0,
  INFO  = 1,
  DEBUG = 2,

  COUNT = 3,
};


/**
 * @brief The global verbosity level of log printing.
 * @note #67: written via `set_log_verbosity` and read on every log path — atomic, or the two race.
 */
inline std::atomic<Verbosity> GLOBAL_VERBOSITY = Verbosity::NONE; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// Default NONE (D-F): logging is opt-in — the library must not claim the host's stdout unasked.


/** @brief Set the verbosity level of log printing.
 *  @return `true` if the level was stored, `false` if `new_verbosity` is out of range.
 */
[[nodiscard]] inline auto set_verbosity(const Verbosity new_verbosity) -> bool {
  if (new_verbosity >= Verbosity::COUNT) {
    return false;
  }
  GLOBAL_VERBOSITY = new_verbosity;
  return true;
}


/**
 * @brief Log a message, never throwing.
 * @note #67: logging runs inside the C-ABI catch handlers, so it must never throw — `vformat`
 *       throws on an ill-formed format string, `println` on a failed stream ([print.fun]) — and
 *       an escape there would cross the `extern "C"` boundary and terminate the host.
 */
template <typename... Args>
inline void log_noexcept(const std::string_view format_str, Args&&... args) noexcept { // NOLINT(cppcoreguidelines-missing-std-forward)
  try {
    // TODO: Currently std::forward<Args>(args)... is not supported on some platforms. Forward args when available.
    const std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
    std::println("{}", formatted_message);
  } catch (...) { // NOLINT(bugprone-empty-catch) — nowhere left to report; swallowing is the contract.
  }
}

/** @brief Log a message, at the `INFO` verbosity level. */
template <typename... Args>
inline void info(const std::string_view format_str, Args&&... args) noexcept { // NOLINT(cppcoreguidelines-missing-std-forward)
  if (GLOBAL_VERBOSITY >= Verbosity::INFO) {
    log_noexcept(format_str, args...);
  }
}

/** @brief Log a message, at the `DEBUG` verbosity level. */
template <typename... Args>
inline void debug(const std::string_view format_str, Args&&... args) noexcept { // NOLINT(cppcoreguidelines-missing-std-forward)
  if (GLOBAL_VERBOSITY >= Verbosity::DEBUG) {
    log_noexcept(format_str, args...);
  }
}


/**
 * @brief The calling thread's last-error message.
 * @note #97: a thread-local last-error channel, so an FFI caller that got `valid = false`
 *       can learn *why* (the log goes to the library's stdout, which hosts may never see).
 *       The recording set is listed once on `last_error` in `celestial.h`.
 */
/**
 * @note The storage and these three bodies live in `lib.cpp`, deliberately not here. As an
 *       `inline thread_local` in the header it had vague linkage, and under
 *       `VISIBILITY_INLINES_HIDDEN` Mach-O does not coalesce a hidden TLS initialisation
 *       routine across translation units -- the five `lib*.cpp` each kept one and the dylib
 *       failed to link (2026-08-06, the macOS leg's first run after the visibility work).
 *       One definition in one TU sidesteps the question instead of asking the linker to
 *       merge something we have just told it to hide.
 */

/** @brief Clear the calling thread's last-error message. */
auto clear_last_error() noexcept -> void;

/**
 * @brief Record the calling thread's last-error message.
 * @note `noexcept`: the string assignment can throw `bad_alloc`; on failure the previous
 *       message is kept.
 */
auto set_last_error(std::string_view message) noexcept -> void;

/** @brief Record an export-specific fallback for a non-standard exception. */
auto set_unknown_error(std::string_view operation) noexcept -> void;

/** @brief Read the calling thread's last-error message (empty if none). */
[[nodiscard]] auto last_error_message() noexcept -> const char*;

/** @brief Translate all exceptions from a C export to its zero-initialized failure sentinel. */
template <typename Function>
[[nodiscard]] inline auto wrap_export(const std::string_view operation, Function&& function) noexcept
  -> std::invoke_result_t<Function> {
  using Result = std::invoke_result_t<Function>;

  clear_last_error();
  try {
    return std::invoke(std::forward<Function>(function));
  } catch (const std::exception& e) {
    set_last_error(e.what());
    info("Error in {}: {}", operation, e.what());
  } catch (...) {
    set_unknown_error(operation);
  }
  return Result {};
}


} // namespace lib
