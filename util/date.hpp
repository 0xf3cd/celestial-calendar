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

#include <chrono>

namespace util {

/*! @brief A type that can be converted to `std::chrono::year`. */
template <typename T>
concept YearConvertible = std::convertible_to<T, int32_t>;


/*! @brief Converts the input year, month, and date to a `std::chrono::year_month_day`. */
constexpr std::chrono::year_month_day to_ymd(
  const YearConvertible auto year, 
  const uint32_t month, 
  const uint32_t day
) {
  const std::chrono::year __year { static_cast<int32_t>(year) };
  return std::chrono::year_month_day { __year / month / day };
}


/*! @brief Converts the input `std::chrono::year_month_day` to a year, month, and date. */
constexpr std::tuple<int32_t, uint32_t, uint32_t> from_ymd(const std::chrono::year_month_day& ymd) {
  const uint32_t y = static_cast<int32_t>(ymd.year());
  const uint32_t m = static_cast<uint32_t>(ymd.month());
  const uint32_t d = static_cast<uint32_t>(ymd.day());
  return { y, m, d, };
}


/*! @brief A type that can be converted to `std::chrono::days`. */
template <typename T>
concept DayConvertible = requires (T t) {
  { std::chrono::days { t } } -> std::same_as<std::chrono::days>;
};


constexpr std::chrono::year_month_day operator+(
  const std::chrono::year_month_day& ymd, 
  const DayConvertible auto& days
) {
  const auto time_point = std::chrono::sys_days { ymd } + std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}


constexpr std::chrono::year_month_day operator+(
  const DayConvertible auto& days,
  const std::chrono::year_month_day& ymd
) {
  const auto time_point = std::chrono::sys_days { ymd } + std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}


constexpr std::chrono::year_month_day operator-(
  const std::chrono::year_month_day& ymd, 
  const DayConvertible auto& days
) {
  const auto time_point = std::chrono::sys_days { ymd } - std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}

} // namespace util
