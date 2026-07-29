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
