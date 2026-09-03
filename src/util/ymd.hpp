/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
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
concept DaysConvertible = requires (const T& t) {
  // The check is the operators' own expression, on the same const object they bind (#83): with a
  // non-const `t` a conversion that is not const-qualified passes here and fails in the body.
  { std::chrono::days { t } } -> std::same_as<std::chrono::days>;
};

/*!
 * @brief Converts the input year, month, and date to a `std::chrono::year_month_day`.
 * @note Month and day are plain `uint32_t` while `year` keeps a concept — a deliberate asymmetry,
 *       not an oversight. Both are counts that become `std::chrono::month` / `day`, whose
 *       constructors take `unsigned`; the parameter type is that contract, so a constraint there
 *       could only restate it (#83).
 */
[[nodiscard]] constexpr auto to_ymd(
  const YearConvertible auto year,
  const uint32_t month,
  const uint32_t day
) -> std::chrono::year_month_day {
  const std::chrono::year _year { static_cast<int32_t>(year) };
  return std::chrono::year_month_day { _year / std::chrono::month { month } / std::chrono::day { day } };
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
