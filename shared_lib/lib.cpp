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

#include "lib.hpp"

// This cpp file is holding the functions to control the global configuration, such as log verbosity level.

extern "C" {

/** 
 * @brief Set the verbosity level of log printing. 
 * @param new_verbosity The new verbosity level.
 * @returns `true` if the verbosity level is changed to the specified value, `false` otherwise.
 */
bool set_log_verbosity(const lib::Verbosity new_verbosity) {
  const auto current_verbosity = lib::set_verbosity(new_verbosity);
  return current_verbosity == new_verbosity;
}

}
