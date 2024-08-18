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

#include <cassert>

#include "lib.hpp"

#include "lunar/algo1.hpp"
#include "lunar/algo2.hpp"


extern "C" {

struct SupportedLunarYearRange {
  bool valid;
  int32_t start;
  int32_t end;
};

/**
 * @brief Get the supported lunar year range for the algorithm.
 * @param algo The algorithm. Expected to be 1 or 2.
 * @return The supported lunar year range.
 */
auto get_supported_lunar_year_range(const uint8_t algo) -> SupportedLunarYearRange {
  if (algo == 1) {
    return {
      .valid = true,
      .start = calendar::lunar::algo1::START_YEAR,
      .end   = calendar::lunar::algo1::END_YEAR,
    };
  }

  if (algo == 2) {
    return {
      .valid = true,
      .start = calendar::lunar::algo2::START_YEAR,
      .end   = calendar::lunar::algo2::END_YEAR,
    };
  }

  return {};
}

struct LunarYearInfo { // Avoid name conflict with `calendar::lunar::common::LunarYear`.
  bool valid;

  // The following 3 fields indicate the date of the first day of the lunar year in gregorian calendar.
  int32_t year;
  uint8_t month;
  uint8_t day;

  // `leap_month` indicates the month of the leap month (1 <= leap_month <= 12).
  // If 0, there is no leap month.
  uint8_t leap_month;
  
  // Use least 12 or 13 bits to indicate the length of each month in the lunar year.
  // '1' means 30 days, '0' means 29 days.
  // If `leap_month` is 0, there are 12 months; otherwise there are 13 months.
  uint16_t month_len;
};


/**
 * @brief Get the lunar year information for the given year.
 * @param algo The algorithm. Expected to be 1 or 2.
 * @param year The lunar year.
 * @return The lunar year information.
 */
auto get_lunar_year_info(const uint8_t algo, const int32_t year) -> LunarYearInfo { // NOLINT(bugprone-easily-swappable-parameters)
  using namespace std::views;
  
  try {
    if (algo != 1 and algo != 2) {
      throw std::runtime_error {
        std::format("Unsupported algorithm: {}", algo)
      };
    }

    const auto raw = std::invoke([=] {
      if (algo == 1) {
        return calendar::lunar::algo1::get_info_for_year(year);
      }
      return calendar::lunar::algo2::get_info_for_year(year);
    });

    const auto [y, m, d] = util::from_ymd(raw.date_of_first_day);

    uint16_t month_len = 0;
    for (const auto& [i, days] : zip(iota(0), raw.month_lengths)) { // TODO: Use `views::enumerate` when supported.
      assert(days == 29 or days == 30);
      const uint16_t bit = (days == 29) ? 0 : 1;
      month_len |= (bit << i);
    }

    return {
      .valid      = true,
      .year       = y,
      .month      = static_cast<uint8_t>(m),
      .day        = static_cast<uint8_t>(d),
      .leap_month = raw.leap_month,
      .month_len  = month_len,
    };

  } catch (const std::exception& e) {
    lib::info("Exception raised during execution of get_lunar_year_info_algo1, year = {}", year);
    lib::info(e.what());
    return {};
  }
}

}
