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
constexpr auto parse_lunar_year(int32_t year, uint32_t encoded) -> LunarYear { // NOLINT(bugprone-easily-swappable-parameters)
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


/** @brief The bounds of the algorithm. */
struct AlgoBounds {
  int32_t start_lunar_year;
  int32_t end_lunar_year;
  year_month_day first_lunar_date;
  year_month_day last_lunar_date;
  year_month_day first_gregorian_date;
  year_month_day last_gregorian_date;
};


/** @brief Calculate the bounds of the luanr algorithm. */
inline auto calc_bounds(
  const int32_t start_lunar_year, 
  const int32_t end_lunar_year, 
  std::function<LunarYear(int32_t)> algo_f
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
 * @param bounds The bounds of the algorithm, i.e. the supported range of lunar and Gregorian dates.
 *               算法支持的阴历和公历日期的范围。
 */
template <Algo algo>
struct AlgoMetadata;

} // namespace calendar::lunar::common
