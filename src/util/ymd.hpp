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

#include <tuple>
#include <chrono>
#include <cstdint>
#include <concepts>

namespace util {

/*! @brief A type that can be converted to `std::chrono::year`. */
template <typename T>
concept YearConvertible = requires (T t) {
  { std::chrono::year { static_cast<int32_t>(t) } } -> std::same_as<std::chrono::year>;
};

/*! @brief A type `std::chrono::days` can be built from — what the operators below shift by. */
template <typename T>
concept DaysConvertible = requires (T t) {
  // The check is the operators' own expression, not a paraphrase of it. The paraphrase
  // `days { static_cast<uint32_t>(t) }` admitted `std::chrono::day`, which the body cannot use (#83).
  { std::chrono::days { t } } -> std::same_as<std::chrono::days>;
};

/*!
 * @brief Converts the input year, month, and date to a `std::chrono::year_month_day`.
 * @note Month and day are plain `uint32_t`: every caller passes an integer, and the concrete type
 *       already rejects the rest at the call site — a constraint there could only paraphrase it (#83).
 */
[[nodiscard]] constexpr auto to_ymd(
  const YearConvertible auto year,
  const uint32_t month,
  const uint32_t day
) -> std::chrono::year_month_day {
  const std::chrono::year _year { static_cast<int32_t>(year) };
  return std::chrono::year_month_day { _year / month / day };
}


/*! @brief Converts the input `std::chrono::year_month_day` to a year, month, and date. */
[[nodiscard]] constexpr auto from_ymd(const std::chrono::year_month_day& ymd) -> std::tuple<int32_t, uint32_t, uint32_t> {
  const int32_t y = static_cast<int32_t>(ymd.year());
  const uint32_t m = static_cast<uint32_t>(ymd.month());
  const uint32_t d = static_cast<uint32_t>(ymd.day());
  return { y, m, d, };
}

} // namespace util

namespace util::ymd_operator {

using util::DaysConvertible;

[[nodiscard]] constexpr auto operator+(
  const std::chrono::year_month_day& ymd, 
  const DaysConvertible auto& days
) -> std::chrono::year_month_day {
  const auto time_point = std::chrono::sys_days { ymd } + std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}


[[nodiscard]] constexpr auto operator+(
  const DaysConvertible auto& days,
  const std::chrono::year_month_day& ymd
) -> std::chrono::year_month_day {
  const auto time_point = std::chrono::sys_days { ymd } + std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}


[[nodiscard]] constexpr auto operator-(
  const std::chrono::year_month_day& ymd, 
  const DaysConvertible auto& days
) -> std::chrono::year_month_day {
  const auto time_point = std::chrono::sys_days { ymd } - std::chrono::days { days };
  return std::chrono::year_month_day { time_point };
}


[[nodiscard]] constexpr auto operator-(
  const std::chrono::year_month_day& ymd1, 
  const std::chrono::year_month_day& ymd2
) -> int32_t {
  const auto diff_days = std::chrono::sys_days { ymd1 } - std::chrono::sys_days { ymd2 };
  return static_cast<int32_t>(diff_days.count());
}

} // namespace util::ymd_operator
