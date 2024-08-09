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

#include <queue>
#include "common.hpp"
#include "jieqi.hpp"
#include "moon_phase.hpp"
#include "julian_day.hpp"

namespace calendar::lunar::algo2 {

using namespace calendar::jieqi;
using calendar::lunar::common::LunarYearInfo;


/**
 * @brief A generator that generates some metadata of lunar months, including Jieqi and length of the month.
 */
// TODO: Use `std::generator` instead, when supported.
struct LunarMonthGenerator {
private:
  astro::moon_phase::new_moon::RootGenerator _root_gen;

  std::queue<double> _jdes;
  std::queue<Jieqi> _jieqi;

public:
 /** @note Generated lunar and jieqi information starts after the given JDE. */
  LunarMonthGenerator(const double start_jde) 
    : _root_gen(start_jde) 
  {
    
  }
};


/**
 * @brief Calculate the lunar year information for the given year. 
          计算给定年份的阴历年信息。
 * @param year The Lunar year. 阴历年份。
 * @return The lunar year information. 阴历年信息。
 */
inline auto calc_lunar_year_info(uint32_t year) -> LunarYearInfo {
  const auto winter_solstice_last_year = jieqi_ut1_moment(year - 1, Jieqi::冬至);
  const auto winter_solstice_this_year = jieqi_ut1_moment(year, Jieqi::冬至);



  return {};
}

} // namespace calendar::lunar::algo2
