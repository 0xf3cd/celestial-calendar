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
#include <ranges>
#include <numeric>
#include <cstdint>
#include <concepts>
#include <optional>
#include <functional>
#include <type_traits>

#include "ymd.hpp"

namespace calendar::lunar::common {

using std::chrono::year_month_day;

/** 
 * @struct LunarYear 
 * @brief  Information of the lunar year. 阴历年信息。
 * @note   Lunar months are defined in UTC+8 time zone. 阴历月的划分是基于 UTC+8 时区的（如北京时间、香港时间、台北时间）。
 */
struct LunarYear {
  /*! @brief The date of the first day of the lunar year in gregorian calendar. 
             本阴历年第一天对应的公历日期。 */
  year_month_day date_of_first_day {};

  /*! @brief The month of the leap month (1 <= leap_month <= 12). If 0, there is no leap month.
             闰月的月份 (1-12)，如果为 0 则没有闰月。 */
  uint8_t leap_month {};

  /*! @brief The number of days in all months in the lunar year.
             There are 12 elements if there is no leap year, otherwise there are 13 elements.
             本阴历年每个月的天数。
             如果没有闰月，那么有 12 个元素；如果有闰月，那么有 13 个元素。 */
  std::vector<uint32_t> month_lengths;
};


/**
 * @brief Parse the encoded lunar year information for the given year. 
          返回给定年份的阴历年信息。
 * @param year The lunar year. 阴历年份。
 * @param encoded The encoded lunar year information. 阴历年信息的编码。
 * @return The lunar year information. 阴历年信息。
 */
[[nodiscard]] inline auto parse_lunar_year(int32_t year, uint32_t encoded) -> LunarYear { // NOLINT(bugprone-easily-swappable-parameters)
  const uint32_t days_offset    = encoded >> 17;
  const uint8_t  leap_month     = (encoded >> 13) & 0xf;
  const uint16_t month_len_info = encoded & 0x1fff;

  const std::chrono::year_month_day first_day = std::invoke([&] {
    using namespace util::ymd_operator;
    return util::to_ymd(year, 1, 1) + days_offset;
  });

  const std::vector<uint32_t> month_lengths = std::invoke([&] {
    using namespace std::ranges;

    const bool leap = (leap_month != 0);
    const auto months = views::iota(0, leap ? 13 : 12);

    const auto lengths = months | views::transform([&](auto m) -> uint32_t {
      const bool big = (month_len_info >> m) & 0x1;
      return big ? 30 : 29;
    });

    return lengths | to<std::vector>();
  });

  return {
    .date_of_first_day = first_day,
    .leap_month        = leap_month,
    .month_lengths     = month_lengths,
  };  
}


/**
 * @brief A lunar month in traditional numbering: month number (1-12) plus a leap flag.
 *        传统编号的阴历月：月份 (1-12) 加闰月标记。
 * @note  `leap_month` and the positional indexing of `month_lengths` speak different month
 *        languages — a leap month has no traditional number of its own (it takes its predecessor's
 *        number with the leap flag), but occupies its own position. E.g. lunar 2023 (leap 2nd
 *        month): position 3 is the leap 2nd month, and the traditional 3rd month is position 4.
 *        `leap_month` 是传统编号，而 `month_lengths` 按位置序号索引——闰月没有自己的传统编号
 *        （承前一个月的编号加闰标记），但独占一个位置。例：2023 年闰二月，位置 3 = 闰二月，
 *        传统三月 = 位置 4。
 */
struct TraditionalMonth {
  uint8_t month;
  bool is_leap;

  [[nodiscard]] auto operator==(const TraditionalMonth& other) const -> bool = default;
};

/**
 * @brief Translate a traditionally-numbered lunar month to its 1-based position in
 *        `info.month_lengths`. 把传统编号的阴历月翻译成 `info.month_lengths` 里的位置序号（从 1 起）。
 * @param info The lunar year. 阴历年信息。
 * @param month The traditional month number (1-12). 传统编号 (1-12)。
 * @param is_leap Whether the month is the leap month. 是否闰月。
 * @return The 1-based position; `std::nullopt` if `month` is out of range, or if `is_leap`
 *         does not match the year's actual leap month (including leap-less years).
 *         位置序号（从 1 起）；`month` 越界，或 `is_leap` 与该年实际闰月不符（含无闰月年）时返回 `std::nullopt`。
 */
[[nodiscard]] inline auto month_position(const LunarYear& info, const uint8_t month, const bool is_leap) -> std::optional<uint8_t> {
  if (month < 1 or month > 12) {
    return std::nullopt;
  }
  if (is_leap) {
    if (month != info.leap_month) {
      return std::nullopt;
    }
    return static_cast<uint8_t>(month + 1);
  }
  const bool after_leap = (info.leap_month != 0 and month > info.leap_month);
  return static_cast<uint8_t>(month + (after_leap ? 1 : 0));
}

/**
 * @brief Translate a 1-based position in `info.month_lengths` back to traditional numbering.
 *        把 `info.month_lengths` 里的位置序号（从 1 起）翻译回传统编号。
 * @param info The lunar year. 阴历年信息。
 * @param position The 1-based position. 位置序号（从 1 起）。
 * @return The traditionally-numbered month; `std::nullopt` if `position` is out of range.
 *         传统编号的月；`position` 越界时返回 `std::nullopt`。
 */
[[nodiscard]] inline auto month_at_position(const LunarYear& info, const uint8_t position) -> std::optional<TraditionalMonth> {
  // `month_lengths.size()` is 12 or 13 by construction, so the cast cannot lose anything.
  if (position < 1 or position > static_cast<uint8_t>(info.month_lengths.size())) {
    return std::nullopt;
  }
  if (info.leap_month == 0 or position <= info.leap_month) {
    return TraditionalMonth { .month = position, .is_leap = false };
  }
  if (position == info.leap_month + 1) {
    return TraditionalMonth { .month = info.leap_month, .is_leap = true };
  }
  return TraditionalMonth { .month = static_cast<uint8_t>(position - 1), .is_leap = false };
}


/** @brief The bounds of the algorithm. */
struct AlgoBounds {
  int32_t start_lunar_year;
  int32_t end_lunar_year;
  year_month_day first_lunar_date;
  year_month_day last_lunar_date;
  year_month_day first_gregorian_date;
  year_month_day last_gregorian_date;
};


/** @brief Calculate the bounds of the lunar algorithm. */
template <typename Func>
requires std::invocable<const Func&, int32_t>
     and std::convertible_to<std::invoke_result_t<const Func&, int32_t>, LunarYear>
[[nodiscard]] inline auto calc_bounds(
  const int32_t start_lunar_year,
  const int32_t end_lunar_year,
  const Func& algo_f
) -> AlgoBounds {
  const auto first_lunar_date = util::to_ymd(start_lunar_year, 1, 1);

  const auto last_lunar_date = std::invoke([&] {
    const LunarYear& info = algo_f(end_lunar_year);
    return util::to_ymd(end_lunar_year, info.month_lengths.size(), info.month_lengths.back());
  });

  const auto first_gregorian_date = std::invoke([&] {
    const LunarYear& info = algo_f(start_lunar_year);
    return info.date_of_first_day;
  });

  const auto last_gregorian_date = std::invoke([&] {
    const LunarYear& info = algo_f(end_lunar_year);
    const auto& ml = info.month_lengths;
    const uint32_t days_count = std::reduce(cbegin(ml), cend(ml));

    using namespace util::ymd_operator;
    return (days_count - 1) + info.date_of_first_day;
  });

  return {
    .start_lunar_year     = start_lunar_year,
    .end_lunar_year       = end_lunar_year,
    .first_lunar_date     = first_lunar_date,
    .last_lunar_date      = last_lunar_date,
    .first_gregorian_date = first_gregorian_date,
    .last_gregorian_date  = last_gregorian_date,
  };
}


/** @brief Algorithm version. */
enum class Algo : uint8_t { ALGO_1, ALGO_2, ALGO_3 };

/**
 * @struct The type trait for the lunar algorithm,
 *         expected specializations in every algorithm implementation.
 * @param get_info_for_year The function to get the lunar year information for the given year.
 *                          用于使用该算法获取给定年份的阴历年信息的函数。
 *                          Either a plain forwarding function (algo1/3) or a reference binding
 *                          to the algorithm's cached callable (algo2, #78) — same call syntax.
 * @param bounds The bounds of the algorithm, i.e. the supported range of lunar and Gregorian dates.
 *               算法支持的阴历和公历日期的范围。
 *               Returned through an accessor function — an eagerly-bound member would
 *               initialize at image load (defeating algo2's lazy init, #67).
 */
template <Algo algo>
struct AlgoMetadata;

} // namespace calendar::lunar::common
