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

#include <optional>

#include "common.hpp"
#include "jieqi.hpp"
#include "moon_phase.hpp"
#include "julian_day.hpp"


namespace calendar::lunar::algo2 {

using namespace calendar::jieqi;
using calendar::lunar::common::LunarYearInfo;


/**
 * @brief A generator that generates some metadata of lunar months, including Jieqi and length of the month.
 * @note Generated lunar and jieqi information starts after the given JDE.
 */
// TODO: Use `std::generator` instead, when supported.
struct LunarMonthIterator {
private:
  astro::moon_phase::new_moon::RootGenerator _new_moon_gen;
  calendar::jieqi::JieqiGenerator _jieqi_gen;

  std::optional<double> _next_new_moon;
  std::optional<JieqiGenerator::JieqiPair> _next_jieqi;

  auto next_new_moon() -> double {
    if (_next_new_moon.has_value()) {
      const double jde = *_next_new_moon;
      _next_new_moon = std::nullopt;
      return jde;
    }

    return _new_moon_gen.next();
  }

  auto put_back_new_moon(const double jde) -> void {
    assert(!_next_new_moon.has_value());
    _next_new_moon = jde;
  }

  auto next_jieqi() -> JieqiGenerator::JieqiPair {
    if (_next_jieqi.has_value()) {
      const auto jieqi = *_next_jieqi;
      _next_jieqi = std::nullopt;
      return jieqi;
    }

    return _jieqi_gen.next();
  }

  auto put_back_jieqi(const JieqiGenerator::JieqiPair jieqi) -> void {
    assert(!_next_jieqi.has_value());
    _next_jieqi = jieqi;
  }

public:
  explicit LunarMonthIterator(const double start_jde) 
    : _new_moon_gen(start_jde),
      _jieqi_gen(start_jde),
      _next_new_moon(_new_moon_gen.next()),
      _next_jieqi(_jieqi_gen.next())
  {}

  struct LunarMonth {
    // Start of the month, inclusive.
    double start_jde;
    calendar::Datetime start_moment;

    // End of the month, exclusive.
    double end_jde;
    calendar::Datetime end_moment;

    // Jieqis that fall in this lunar month.
    std::vector<JieqiGenerator::JieqiPair> contained_jieqis;
  };

  auto next() -> LunarMonth {
    // Get the bounds of the next lunar month.
    const auto start_jde = next_new_moon();
    const auto end_jde = next_new_moon();
    put_back_new_moon(end_jde);

    // Get the Jieqis that fall in this lunar month.
    std::vector<JieqiGenerator::JieqiPair> jieqis;
    while (true) {
      const auto jieqi = next_jieqi();

      if (jieqi.jde >= end_jde) {
        put_back_jieqi(jieqi);
        break;
      }

      if (jieqi.jde < start_jde) {
        continue;
      }

      jieqis.push_back(jieqi);
    }

    return {
      .start_jde = start_jde,
      .start_moment = astro::julian_day::jde_to_ut1(start_jde),
      .end_jde = end_jde,
      .end_moment = astro::julian_day::jde_to_ut1(end_jde),
      .contained_jieqis = jieqis
    };
  };
};


// /**
//  * @brief Calculate the lunar year information for the given year. 
//           计算给定年份的阴历年信息。
//  * @param year The Lunar year. 阴历年份。
//  * @return The lunar year information. 阴历年信息。
//  */
// inline auto calc_lunar_year_info(uint32_t year) -> LunarYearInfo {
//   const auto winter_solstice_last_year = jieqi_ut1_moment(year - 1, Jieqi::冬至);
//   const auto winter_solstice_this_year = jieqi_ut1_moment(year, Jieqi::冬至);
//   return {};
// }

} // namespace calendar::lunar::algo2
