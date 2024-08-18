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

#include <numeric>
#include <optional>

#include "common.hpp"


namespace calendar::lunar::converter {

using std::chrono::year_month_day;
using std::chrono::sys_days;
using namespace calendar::lunar::common;

/**
 * @struct Converter
 * @brief  Converts between gregorian date and lunar date. 
 *         将公历日期和阴历日期进行转换。
 * @tparam algo The algorithm to use. 使用的算法。
 */
template <common::Algo algo>
struct Converter {

  using AlgoMetadata = common::AlgoMetadata<algo>;

  /** 
   * @fn Checks if the input gregorian date is valid and within the supported range. 
         检查输入的公历日期是否有效，且在支持的范围内。 
   * @param date The date. 公历日期。
   * @return `true` if valid, otherwise `false`. 如果有效，返回 `true`，否则返回 `false`。
   */
  static auto is_valid_gregorian(const year_month_day& date) -> bool {
    if (not date.ok()) {
      return false;
    }
    if (date < AlgoMetadata::bounds.first_gregorian_date or 
        date > AlgoMetadata::bounds.last_gregorian_date) {
      return false;
    }
    return true;
  }

  /** 
   * @fn Checks if the input lunar date is valid and within the supported range. 
         检查输入的阴历日期是否有效，且在支持的范围内。 
   * @param lunar_date The lunar date. 阴历日期。
   * @return `true` if valid, otherwise `false`. 如果有效，返回 `true`，否则返回 `false`。
   */
  static auto is_valid_lunar(const year_month_day& lunar_date) -> bool {
    if (lunar_date < AlgoMetadata::bounds.first_lunar_date or 
        lunar_date > AlgoMetadata::bounds.last_lunar_date) {
      return false;
    }

    const auto [y, m, d] = util::from_ymd(lunar_date);
    const auto& info = AlgoMetadata::get_info_for_year(y);
    const auto& ml = info.month_lengths;
    
    if (m < 1 or m > ml.size()) {
      return false;
    }
    if (d < 1 or d > ml[m - 1]) {
      return false;
    }

    return true;
  }

  /** 
   * @fn gregorian_to_lunar 
   * @brief Converts gregorian date to lunar date. 将公历日期转换为阴历日期。 
   * @param gregorian_date The gregorian date. 公历日期。
   * @return The optional lunar date. 阴历日期（optional）。
   * @attention The input date should be in the supported range.
                输入的日期需要在所支持的范围内。
  * @attention `std::nullopt` is returned if the input date is invalid. No exception is thrown.
               输入的日期无效时返回 `std::nullopt`。不会抛出异常。
  */
  static auto gregorian_to_lunar(const year_month_day& gregorian_date) -> std::optional<year_month_day> {
    if (not is_valid_gregorian(gregorian_date)) {
      return std::nullopt;
    }

    const auto find_lunar_date = [&](const int32_t lunar_y) -> year_month_day {
      const auto& info = AlgoMetadata::get_info_for_year(lunar_y);
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
    const auto& [g_year, _, __] = util::from_ymd(gregorian_date);
    if (g_year <= AlgoMetadata::bounds.end_lunar_year) {
      using namespace util::ymd_operator;
      const auto& info = AlgoMetadata::get_info_for_year(g_year);
      const auto& ml = info.month_lengths;
      const uint32_t lunar_year_days_count = std::reduce(cbegin(ml), cend(ml));

      // Calculate the gregorian date of the last day in the lunar year.
      const year_month_day last_lunar_day = info.date_of_first_day + (lunar_year_days_count - 1);
      if (gregorian_date >= info.date_of_first_day and gregorian_date <= last_lunar_day) { // Yeah! We found the lunar year.
        return find_lunar_date(g_year);
      }
    }

    // Otherwise, the lunar date falls into the previous year.
    return find_lunar_date(g_year - 1);
  }

  /** 
   * @fn lunar_to_gregorian
   * @brief Converts lunar date to gregorian date. 将阴历日期转换为公历日期。 
   * @param lunar_date The lunar date. 阴历日期。
   * @return The optional gregorian date. 公历日期（optional）。
   * @attention The input date should be in the supported range.
                输入的日期需要在所支持的范围内。
  * @attention `std::nullopt` is returned if the input date is invalid. No exception is thrown.
               输入的日期无效时返回 `std::nullopt`。不会抛出异常。
  */
  static auto lunar_to_gregorian(const year_month_day& lunar_date) -> std::optional<year_month_day> {
    if (not is_valid_lunar(lunar_date)) {
      return std::nullopt;
    }

    const auto [y, m, d] = util::from_ymd(lunar_date);
    const auto& info = AlgoMetadata::get_info_for_year(y);
    const auto& ml = info.month_lengths;

    const uint32_t past_days_count = d + std::reduce(cbegin(ml), cbegin(ml) + m - 1);

    using namespace util::ymd_operator;
    return info.date_of_first_day + (past_days_count - 1);
  }
};

} // namespace calendar::lunar::converter
