/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#include "lib.hpp"
#include "celestial.h"

// This cpp file is holding the functions to control the global configuration, such as log verbosity level.
// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

namespace lib {

// #97: the calling thread's last-error message. Declared in lib.hpp and defined here so
// that exactly one of it exists -- see the note there for what an `inline thread_local` cost
// us on Mach-O once the inlines went hidden.
namespace {
thread_local std::string LAST_ERROR; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

auto clear_last_error() noexcept -> void {
  LAST_ERROR.clear();
}

auto set_last_error(const std::string_view message) noexcept -> void {
  try {
    LAST_ERROR = message;
  } catch (...) { // NOLINT(bugprone-empty-catch) — nowhere left to report; swallowing is the contract.
  }
}

auto set_unknown_error(const std::string_view operation) noexcept -> void {
  try {
    LAST_ERROR = "Unknown error in ";
    LAST_ERROR += operation;
    LAST_ERROR += '.';
  } catch (...) { // NOLINT(bugprone-empty-catch) — nowhere left to report; swallowing is the contract.
  }
}

auto last_error_message() noexcept -> const char* {
  return LAST_ERROR.c_str();
}

} // namespace lib


extern "C" {

auto set_log_verbosity(const uint8_t new_value) -> bool {
  return lib::wrap_export("set_log_verbosity", [=] {
    if (not lib::set_verbosity(static_cast<lib::Verbosity>(new_value))) {
      throw std::invalid_argument {
        std::format("Argument `new_value` must be in [0, 2], got {}", new_value)
      };
    }
    return true;
  });
}


// Contract: see celestial.h.
auto last_error() -> const char* {
  return lib::last_error_message();
}

}
