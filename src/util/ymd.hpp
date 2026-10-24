/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
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
#include <format>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <limits>

namespace util {

/*! @brief An integral year of at most 64 value bits, a floating-point year, or a chrono year. */
template <typename T>
concept YearConvertible = std::same_as<std::remove_cv_t<T>, std::chrono::year>
  or std::floating_point<T>
  or (std::integral<T> and std::numeric_limits<T>::digits <= 64);

/*!
 * @brief Converts an integer year to `std::chrono::year`, rejecting what narrowing would alias.
 * @param year The original integer year.
 * @return The same year as a `std::chrono::year`.
 * @throw std::invalid_argument If `year` is outside [`std::chrono::year::min()`,
 *        `std::chrono::year::max()`]; the diagnostic names the original integer.
 * @note `std::chrono::year` holds an unspecified value outside ±32767; the check precedes the
 *       narrowing conversion.
 */
[[nodiscard]] constexpr auto checked_year(const int32_t year) -> std::chrono::year {
  if (year < static_cast<int32_t>(std::chrono::year::min()) or
      year > static_cast<int32_t>(std::chrono::year::max())) {
    throw std::invalid_argument {
      std::format("Argument `year` cannot be represented by std::chrono::year: {}", year)
    };
  }
  return std::chrono::year { year };
}

/*! @brief A type `std::chrono::days` can be built from — what the operators below shift by. */
template <typename T>
concept DaysConvertible = requires (const T& t) {
  // The check is the operators' own expression, on the same const object they bind (#83): with a
  // non-const `t` a conversion that is not const-qualified passes here and fails in the body.
  { std::chrono::days { t } } -> std::same_as<std::chrono::days>;
};

/*!
 * @brief Converts the input year, month, and date to a `std::chrono::year_month_day`.
 * @throw std::invalid_argument if the original year is not a representable whole year,
 *        or month/day cannot be represented unchanged by their chrono fields.
 * @note Calendar validity is checked by the consumer: lunar labels also use positional month 13.
 */
[[nodiscard]] constexpr auto to_ymd(
  const YearConvertible auto year,
  const uint32_t month,
  const uint32_t day
) -> std::chrono::year_month_day {
  using Year = decltype(year);
  constexpr auto FIRST_YEAR = static_cast<int32_t>(std::chrono::year::min());
  constexpr auto LAST_YEAR = static_cast<int32_t>(std::chrono::year::max());

  if constexpr (std::is_floating_point_v<Year>) {
    const bool within_years = year >= FIRST_YEAR and year <= LAST_YEAR;
    const bool whole_year = within_years and static_cast<std::remove_cv_t<Year>>(static_cast<int32_t>(year)) == year;
    if (not whole_year) {
      throw std::invalid_argument { std::format("Argument `year` must be a representable whole year, got {}", year) };
    }
  } else if constexpr (std::is_integral_v<Year>) {
    using Wide = std::conditional_t<std::is_signed_v<Year>, int64_t, uint64_t>;
    const auto original = static_cast<Wide>(year);
    if (std::cmp_less(original, FIRST_YEAR) or std::cmp_greater(original, LAST_YEAR)) {
      throw std::invalid_argument { std::format("Argument `year` is outside [{}, {}], got {}", FIRST_YEAR, LAST_YEAR, original) };
    }
  }

  const auto checked = checked_year(static_cast<int32_t>(year));
  if (month > 255 or day > 255) {
    throw std::invalid_argument { std::format("Month/day cannot be represented unchanged: {}-{}", month, day) };
  }

  return std::chrono::year_month_day { checked / std::chrono::month { month } / std::chrono::day { day } };
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
