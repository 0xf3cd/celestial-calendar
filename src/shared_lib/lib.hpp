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

#include <print>
#include <atomic>
#include <format>
#include <string>


namespace lib {

/** @enum Verbosity represents the verbosity level of log printing. */
enum class Verbosity : uint8_t {
  NONE  = 0,
  INFO  = 1,
  DEBUG = 2,

  COUNT,
};


// #67: written via `set_log_verbosity` and read on every log path — atomic, or the two race.
inline std::atomic<Verbosity> GLOBAL_VERBOSITY = Verbosity::DEBUG; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)


/** @brief Set the verbosity level of log printing. */
inline auto set_verbosity(const Verbosity new_verbosity) -> Verbosity {
  if (new_verbosity < Verbosity::COUNT) {
    GLOBAL_VERBOSITY = new_verbosity;
  }
  return GLOBAL_VERBOSITY;
}


// #67: logging runs inside the C-ABI catch handlers, so it must never throw — `vformat`
// throws on an ill-formed format string, `println` on a failed stream ([print.fun]) — and
// an escape there would cross the `extern "C"` boundary and terminate the host.
template <typename... Args>
inline void log_noexcept(const std::string& format_str, Args&&... args) { // NOLINT(cppcoreguidelines-missing-std-forward)
  try {
    // TODO: Currently std::forward<Args>(args)... is not supported on some platforms. Forward args when available.
    const std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
    std::println("{}", formatted_message);
  } catch (...) { // NOLINT(bugprone-empty-catch) — nowhere left to report; swallowing is the contract.
  }
}

/** @brief Log a message, at the `INFO` verbosity level. */
template <typename... Args>
inline void info(const std::string& format_str, Args&&... args) { // NOLINT(cppcoreguidelines-missing-std-forward)
  if (GLOBAL_VERBOSITY >= Verbosity::INFO) {
    log_noexcept(format_str, args...);
  }
}

/** @brief Log a message, at the `DEBUG` verbosity level. */
template <typename... Args>
inline void debug(const std::string& format_str, Args&&... args) { // NOLINT(cppcoreguidelines-missing-std-forward)
  if (GLOBAL_VERBOSITY >= Verbosity::DEBUG) {
    log_noexcept(format_str, args...);
  }
}


// #97 pilot: a thread-local last-error channel, so an FFI caller that got `valid = false`
// can learn *why* (the log goes to the library's stdout, which hosts may never see).
// Only the Julian Day exports fill it for now — pilot, not a full rollout.
inline thread_local std::string LAST_ERROR; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/** @brief Clear the calling thread's last-error message. */
inline auto clear_last_error() -> void {
  LAST_ERROR.clear();
}

/** @brief Record the calling thread's last-error message. */
inline auto set_last_error(const std::string& message) -> void {
  LAST_ERROR = message;
}

/** @brief Read the calling thread's last-error message (empty if none). */
inline auto last_error_message() -> const char* {
  return LAST_ERROR.c_str();
}


} // namespace lib
