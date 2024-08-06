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
#include "delta_t.hpp"

extern "C" {

struct DeltaT {
  bool   valid; // Indicates if the result is valid.
  double value; // The value of delta T.
};

/** @brief Compute delta T of a given moment using algorithm 1. */
auto delta_t_algo1(double year) -> DeltaT {
  try {
    return {
      .valid = true,
      .value = astro::delta_t::algo1::compute(year),
    };
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of delta_t_algo1");
    lib::debug("delta_t_algo1: year = {}, error = {}", year, e.what());

    return {};
  }
}

/** @brief Compute delta T of a given moment using algorithm 2. */
auto delta_t_algo2(double year) -> DeltaT {
  try {
    return {
      .valid = true,
      .value = astro::delta_t::algo2::compute(year),
    };
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of delta_t_algo2");
    lib::debug("delta_t_algo2: year = {}, error = {}", year, e.what());

    return {};
  }
}

/** @brief Compute delta T of a given moment using algorithm 3. */
auto delta_t_algo3(double year) -> DeltaT {
  try {
    return {
      .valid = true,
      .value = astro::delta_t::algo3::compute(year),
    };
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of delta_t_algo3");
    lib::debug("delta_t_algo3: year = {}, error = {}", year, e.what());

    return {};
  }
}

/** @brief Compute delta T of a given moment using algorithm 4. */
auto delta_t_algo4(double year) -> DeltaT {
  try {
    return {
      .valid = true,
      .value = astro::delta_t::algo4::compute(year),
    };
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of delta_t_algo4");
    lib::debug("delta_t_algo4: year = {}, error = {}", year, e.what());

    return {};
  }
}

/** @brief Compute delta T of a given moment, using the best algorithm. */
auto delta_t(double year) -> DeltaT {
  try {
    return {
      .valid = true,
      .value = astro::delta_t::compute(year),
    };
  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of delta_t");
    lib::debug("delta_t: year = {}, error = {}", year, e.what());

    return {};
  }
}   

}
