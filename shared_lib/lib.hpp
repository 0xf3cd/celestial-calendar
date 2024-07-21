/*
 * CelestialCalendar: 
 *   A C++23-style library for date conversions and astronomical calculations for various calendars,
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
#include <format>


namespace lib {

/** @enum Verbosity represents the verbosity level of log printing. */
enum class Verbosity : uint8_t {
  NONE  = 0,
  INFO  = 1,
  DEBUG = 2,

  COUNT,
};


inline Verbosity GLOBAL_VERBOSITY = Verbosity::DEBUG;


/** @brief Set the verbosity level of log printing. */
inline Verbosity set_verbosity(const Verbosity new_verbosity) {
  if (new_verbosity < Verbosity::COUNT) {
    GLOBAL_VERBOSITY = new_verbosity;
  }
  return GLOBAL_VERBOSITY;
}


/** @brief Log a message, at the `INFO` verbosity level. */
template <typename... Args>
inline void info(const std::string& format_str, Args&&... args) {
  if (GLOBAL_VERBOSITY >= Verbosity::INFO) {
    const std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
    std::println("{}", formatted_message);
  }
}

/** @brief Log a message, at the `DEBUG` verbosity level. */
template <typename... Args>
inline void debug(const std::string& format_str, Args&&... args) {
  if (GLOBAL_VERBOSITY >= Verbosity::DEBUG) {
    const std::string formatted_message = std::vformat(format_str, std::make_format_args(args...));
    std::println("{}", formatted_message);
  }
}


} // namespace lib
