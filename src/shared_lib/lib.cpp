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

#include "lib.hpp"
#include "celestial.h"

// This cpp file is holding the functions to control the global configuration, such as log verbosity level.
// #67: every export catches all exceptions and degrades to `valid = false` / `0`; contract: see celestial.h.

namespace lib {

// #97 pilot: the calling thread's last-error message. Declared in lib.hpp and defined here so
// that exactly one of it exists -- see the note there for what an `inline thread_local` cost
// us on Mach-O once the inlines went hidden.
namespace {
thread_local std::string LAST_ERROR; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

auto clear_last_error() -> void {
  LAST_ERROR.clear();
}

auto set_last_error(const std::string& message) noexcept -> void {
  try {
    LAST_ERROR = message;
  } catch (...) { // NOLINT(bugprone-empty-catch) — nowhere left to report; swallowing is the contract.
  }
}

auto last_error_message() -> const char* {
  return LAST_ERROR.c_str();
}

} // namespace lib


extern "C" {

auto set_log_verbosity(const uint8_t new_value) -> bool {
  try {
    return lib::set_verbosity(static_cast<lib::Verbosity>(new_value));
  } catch (...) {
    return false;
  }
}


// Contract: see celestial.h.
auto last_error() -> const char* {
  return lib::last_error_message();
}

}
