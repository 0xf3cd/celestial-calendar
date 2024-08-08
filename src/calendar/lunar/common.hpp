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
#include <vector>

namespace calendar::lunar::common {

using std::chrono::year_month_day;

/** 
 * @struct LunarYearInfo 
 * @brief  Information of the lunar year. 阴历年信息。
 */
struct LunarYearInfo {
  /*! @brief The date of the first day of the lunar year in gregorian calendar. 
             本阴历年第一天对应的公历日期。 */
  year_month_day date_of_first_day;

  /*! @brief The month of the leap month (1 <= leap_month <= 12). If 0, there is no leap month.
             闰月的月份 (1-12)，如果为 0 则没有闰月。 */
  uint8_t leap_month;

  /*! @brief The number of days in all months in the lunar year.
             There are 12 elements if there is no leap year, otherwise there are 13 elements.
             本阴历年每个月的天数。
             如果没有闰月，那么有 12 个元素；如果有闰月，那么有 13 个元素。 */
  std::vector<uint32_t> month_lengths;
};


/** @brief A concept which ensures the required functions for date conversion are implemented. */
template <typename T>
concept IsConverter = requires {
  { T::lunar_to_gregorian(std::declval<const year_month_day&>()) } -> std::same_as<std::optional<year_month_day>>;
  { T::gregorian_to_lunar(std::declval<const year_month_day&>()) } -> std::same_as<std::optional<year_month_day>>;
  { T::is_valid_lunar(std::declval<const year_month_day&>()) } -> std::same_as<bool>;
  { T::is_valid_gregorian(std::declval<const year_month_day&>()) } -> std::same_as<bool>;
};

} // namespace calendar::lunar::common
