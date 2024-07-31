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

#include <chrono>
#include <numeric>
#include <cassert>
#include <optional>

#include "ymd.hpp"
#include "lunardata.hpp"


namespace calendar::lunar {

using std::chrono::year_month_day;
using std::chrono::sys_days;
using namespace calendar::lunardata;

/*! @brief The first supported lunar date. */
const inline year_month_day FIRST_LUNAR_DATE = util::to_ymd(START_YEAR, 1, 1);

/*! @brief The last supported lunar date. */
const inline year_month_day LAST_LUNAR_DATE = std::invoke([] {
  const LunarYearInfo& info = LUNARDATA_CACHE.get(END_YEAR);
  return util::to_ymd(END_YEAR, info.month_lengths.size(), info.month_lengths.back());
});

/*! @brief The first supported gregorian date. */
const inline year_month_day FIRST_GREGORIAN_DATE = std::invoke([] {
  const LunarYearInfo& info = LUNARDATA_CACHE.get(START_YEAR);
  return info.date_of_first_day;
});

/*! @brief The last supported gregorian date. */
const inline year_month_day LAST_GREGORIAN_DATE = std::invoke([] {
  const LunarYearInfo& info = LUNARDATA_CACHE.get(END_YEAR);
  const auto& ml = LUNARDATA_CACHE.get(END_YEAR).month_lengths;
  const uint32_t days_count = std::reduce(ml.begin(), ml.end());

  using namespace util::ymd_operator;
  return (days_count - 1) + info.date_of_first_day;
});


/*! @fn Checks if the input gregorian date is valid and within the supported range. 
        检查输入的公历日期是否有效，且在支持的范围内。 */
inline auto is_valid_gregorian(const year_month_day& date) -> bool {
  if (!date.ok()) {
    return false;
  }
  if (date < FIRST_GREGORIAN_DATE || date > LAST_GREGORIAN_DATE) {
    return false;
  }
  return true;
}


/*! @fn Checks if the input lunar date is valid and within the supported range. 
        检查输入的阴历日期是否有效，且在支持的范围内。 */
inline auto is_valid_lunar(const year_month_day& lunar_date) -> bool {
  if (lunar_date < FIRST_LUNAR_DATE || lunar_date > LAST_LUNAR_DATE) {
    return false;
  }

  const auto [y, m, d] = util::from_ymd(lunar_date);
  const auto& info = LUNARDATA_CACHE.get(y);
  const auto& ml = info.month_lengths;
  
  if (m < 1 || m > ml.size()) {
    return false;
  }
  if (d < 1 || d > ml[m - 1]) {
    return false;
  }

  return true;
}


/*! 
 @fn gregorian_to_lunar 
 @brief Converts gregorian date to lunar date. 将公历日期转换为阴历日期。 
 @param gregorian_date The gregorian date. 公历日期。
 @return The optional lunar date. 阴历日期（optional）。
 @attention The input date should be in the range of [FIRST_GREGORIAN_DATE, LAST_GREGORIAN_DATE].
            输入的日期需要在所支持的范围内。
 @attention `std::nullopt` is returned if the input date is invalid. No exception is thrown.
            输入的日期无效时返回 `std::nullopt`。不会抛出异常。
 */
inline auto gregorian_to_lunar(const year_month_day& gregorian_date) -> std::optional<year_month_day> {
  if (!is_valid_gregorian(gregorian_date)) {
    return std::nullopt;
  }

  const auto& [g_y, g_m, g_d] = util::from_ymd(gregorian_date);

  const auto find_lunar_date = [&](const int32_t lunar_y) -> year_month_day {
    const auto& info = LUNARDATA_CACHE.get(lunar_y);
    const auto& ml = info.month_lengths;

    // Calculate how many days have past in the lunar year.
    const uint32_t past_days_count = (sys_days { gregorian_date } - sys_days { info.date_of_first_day }).count();

    uint32_t lunar_m_idx = 0;
    uint32_t rest_days_count = past_days_count;
    while (rest_days_count >= ml[lunar_m_idx]) {
      rest_days_count -= ml[lunar_m_idx];
      ++lunar_m_idx;
    }

    const uint32_t lunar_m = lunar_m_idx + 1;
    assert(1 <= lunar_m and lunar_m <= 13);

    const uint32_t lunar_d = rest_days_count + 1;
    assert(1 <= lunar_d and lunar_d <= 30);

    return util::to_ymd(lunar_y, lunar_m, lunar_d);
  }; // find_lunar_date
  
  // The lunar year can either be the same as the gregorian_date year, or the previous year.
  // Example: a gregorian_date date in gregorian_date year 2024 can be in lunar year 2023 or 2024.

  // First, check if lunar date and gregorian_date date are in the same year.
  if (g_y <= END_YEAR) {
    using namespace util::ymd_operator;
    const auto& info = LUNARDATA_CACHE.get(g_y);
    const auto& ml = info.month_lengths;
    const uint32_t lunar_year_days_count = std::reduce(ml.begin(), ml.end());

    // Calculate the gregorian date of the last day in the lunar year.
    const year_month_day last_lunar_day = info.date_of_first_day + (lunar_year_days_count - 1);
    if (gregorian_date >= info.date_of_first_day and gregorian_date <= last_lunar_day) { // Yeah! We found the lunar year.
      return find_lunar_date(g_y);
    }
  }

  // Otherwise, the lunar date falls into the previous year.
  return find_lunar_date(g_y - 1);
}


/*! 
 @fn lunar_to_gregorian
 @brief Converts lunar date to gregorian date. 将阴历日期转换为公历日期。 
 @param lunar_date The lunar date. 阴历日期。
 @return The optional gregorian date. 公历日期（optional）。
 @attention The input date should be in the range of [FIRST_LUNAR_DATE, LAST_LUNAR_DATE].
            输入的日期需要在所支持的范围内。
 @attention `std::nullopt` is returned if the input date is invalid. No exception is thrown.
            输入的日期无效时返回 `std::nullopt`。不会抛出异常。
 */
inline auto lunar_to_gregorian(const year_month_day& lunar_date) -> std::optional<year_month_day> {
  if (!is_valid_lunar(lunar_date)) {
    return std::nullopt;
  }

  const auto [y, m, d] = util::from_ymd(lunar_date);
  const auto& info = LUNARDATA_CACHE.get(y);
  const auto& ml = info.month_lengths;

  const uint32_t past_days_count = d + std::reduce(ml.begin(), ml.begin() + m - 1);

  using namespace util::ymd_operator;
  return info.date_of_first_day + (past_days_count - 1);
}

} // namespace calendar::lunar
