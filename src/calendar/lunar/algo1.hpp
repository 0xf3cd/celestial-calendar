/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre* @gmail.com
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
#include <format>
#include <ranges>

#include <numeric>
#include <cassert>
#include <optional>

#include <array>
#include <vector>

#include "util.hpp"
#include "common.hpp"


namespace calendar::lunar::algo1::data {

// This file contains encoded binary data for each lunar year between 1901 and 2099.
// This is intended to be used in algo1.

using calendar::lunar::common::LunarYear;

/** @brief The first supported lunar year. */
constexpr int32_t START_YEAR = 1901;

/** @brief The last supported lunar year. */
constexpr int32_t END_YEAR = 2099;

/** @brief The encoded binary data for each lunar year. Info for a year is stored in a uint32_t. */
constexpr std::array<uint32_t, 199> LUNAR_DATA = {
  0x620752, 0x4c0ea5, 0x38b64a, 0x5c064b, 0x440a9b, 0x309556, 0x56056a, 0x400b59, 0x2a5752, 0x500752, 
  0x3adb25, 0x600b25, 0x480a4b, 0x32b4ab, 0x5802ad, 0x42056b, 0x2c4b69, 0x520da9, 0x3efd92, 0x640e92, 
  0x4c0d25, 0x36ba4d, 0x5c0a56, 0x4602b6, 0x2e95b5, 0x5606d4, 0x400ea9, 0x2c5e92, 0x500e92, 0x3acd26, 
  0x5e052b, 0x480a57, 0x32b2b6, 0x580b5a, 0x4406d4, 0x2e6ec9, 0x520749, 0x3cf693, 0x620a93, 0x4c052b, 
  0x34ca5b, 0x5a0aad, 0x46056a, 0x309b55, 0x560ba4, 0x400b49, 0x2a5a93, 0x500a95, 0x38f52d, 0x5e0536, 
  0x480aad, 0x34b5aa, 0x5805b2, 0x420da5, 0x2e7d4a, 0x540d4a, 0x3d0a95, 0x600a97, 0x4c0556, 0x36cab5, 
  0x5a0ad5, 0x4606d2, 0x308ea5, 0x560ea5, 0x40064a, 0x286c97, 0x4e0a9b, 0x3af55a, 0x5e056a, 0x480b69, 
  0x34b752, 0x5a0b52, 0x420b25, 0x2c964b, 0x520a4b, 0x3d14ab, 0x6002ad, 0x4a056d, 0x36cb69, 0x5c0da9, 
  0x460d92, 0x309d25, 0x560d25, 0x415a4d, 0x640a56, 0x4e02b6, 0x38c5b5, 0x5e06d5, 0x480ea9, 0x34be92, 
  0x5a0e92, 0x440d26, 0x2c6a56, 0x500a57, 0x3d14d6, 0x62035a, 0x4a06d5, 0x36b6c9, 0x5c0749, 0x460693, 
  0x2e952b, 0x54052b, 0x3e0a5b, 0x2a555a, 0x4e056a, 0x38fb55, 0x600ba4, 0x4a0b49, 0x32ba93, 0x580a95, 
  0x42052d, 0x2c8aad, 0x500ab5, 0x3d35aa, 0x6205d2, 0x4c0da5, 0x36dd4a, 0x5c0d4a, 0x460c95, 0x30952e, 
  0x540556, 0x3e0ab5, 0x2a55b2, 0x5006d2, 0x38cea5, 0x5e0725, 0x48064b, 0x32ac97, 0x560cab, 0x42055a, 
  0x2c6ad6, 0x520b69, 0x3d7752, 0x620b52, 0x4c0b25, 0x36da4b, 0x5a0a4b, 0x4404ab, 0x2ea55b, 0x5405ad, 
  0x3e0b6a, 0x2a5b52, 0x500d92, 0x3afd25, 0x5e0d25, 0x480a55, 0x32b4ad, 0x5804b6, 0x4005b5, 0x2c6daa, 
  0x520ec9, 0x3f1e92, 0x620e92, 0x4c0d26, 0x36ca56, 0x5a0a57, 0x440556, 0x2e86d5, 0x540755, 0x400749, 
  0x286e93, 0x4e0693, 0x38f52b, 0x5e052b, 0x460a5b, 0x32b55a, 0x58056a, 0x420b65, 0x2c974a, 0x520b4a, 
  0x3d1a95, 0x620a95, 0x4a052d, 0x34caad, 0x5a0ab5, 0x4605aa, 0x2e8ba5, 0x540da5, 0x400d4a, 0x2a7c95, 
  0x4e0c96, 0x38f94e, 0x5e0556, 0x480ab5, 0x32b5b2, 0x5806d2, 0x420ea5, 0x2e8e4a, 0x50068b, 0x3b0c97, 
  0x6004ab, 0x4a055b, 0x34cad6, 0x5a0b6a, 0x460752, 0x309725, 0x540b45, 0x3e0a8b, 0x28549b,
};

/**
 * @brief Parse the encoded lunar year information for the given year. 
          返回给定年份的阴历年信息。
 * @param year The Lunar year. 阴历年份。
 * @return The lunar year information. 阴历年信息。
 */
inline auto parse_lunar_year(int32_t year) -> LunarYear {
  // Validate the input year.
  if (year < START_YEAR or year > END_YEAR) {
    throw std::out_of_range { 
      std::vformat("year must be between {:d} and {:d}.", std::make_format_args(START_YEAR, END_YEAR)) 
    };
  }

  const uint32_t bin_data       = LUNAR_DATA[year - START_YEAR]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
  const uint32_t days_offset    = bin_data >> 17;
  const uint8_t  leap_month     = (bin_data >> 13) & 0xf;
  const uint16_t month_len_info = bin_data & 0x1fff;

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
 * @brief Same function as `calc_lunar_year_info`, but cached. 
          与 `calc_lunar_year_info` 功能相同，但使用缓存。
 * @attention The input year should be in the range of [START_YEAR, END_YEAR].
 * @param year The Lunar year. 阴历年份。
 * @return The lunar year information. 阴历年信息。
 */
const inline auto get_lunar_year = util::cache::cache_func(parse_lunar_year);

} // namespace calendar::lunar::algo1::data



namespace calendar::lunar::algo1 {

using std::chrono::year_month_day;
using std::chrono::sys_days;
using namespace calendar::lunar::algo1::data;

/** @brief The first supported lunar date. */
const inline year_month_day FIRST_LUNAR_DATE = util::to_ymd(START_YEAR, 1, 1);

/** @brief The last supported lunar date. */
const inline year_month_day LAST_LUNAR_DATE = std::invoke([] {
  const LunarYear& info = get_lunar_year(END_YEAR);
  return util::to_ymd(END_YEAR, info.month_lengths.size(), info.month_lengths.back());
});

/** @brief The first supported gregorian date. */
const inline year_month_day FIRST_GREGORIAN_DATE = std::invoke([] {
  const LunarYear& info = get_lunar_year(START_YEAR);
  return info.date_of_first_day;
});

/** @brief The last supported gregorian date. */
const inline year_month_day LAST_GREGORIAN_DATE = std::invoke([] {
  const LunarYear& info = get_lunar_year(END_YEAR);
  const auto& ml = get_lunar_year(END_YEAR).month_lengths;
  const uint32_t days_count = std::reduce(cbegin(ml), cend(ml));

  using namespace util::ymd_operator;
  return (days_count - 1) + info.date_of_first_day;
});


struct Converter {

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
    if (date < FIRST_GREGORIAN_DATE or date > LAST_GREGORIAN_DATE) {
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
    if (lunar_date < FIRST_LUNAR_DATE or lunar_date > LAST_LUNAR_DATE) {
      return false;
    }

    const auto [y, m, d] = util::from_ymd(lunar_date);
    const auto& info = get_lunar_year(y);
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
   * @attention The input date should be in the range of [FIRST_GREGORIAN_DATE, LAST_GREGORIAN_DATE].
              输入的日期需要在所支持的范围内。
  * @attention `std::nullopt` is returned if the input date is invalid. No exception is thrown.
              输入的日期无效时返回 `std::nullopt`。不会抛出异常。
  */
  static auto gregorian_to_lunar(const year_month_day& gregorian_date) -> std::optional<year_month_day> {
    if (not is_valid_gregorian(gregorian_date)) {
      return std::nullopt;
    }

    const auto find_lunar_date = [&](const int32_t lunar_y) -> year_month_day {
      const auto& info = get_lunar_year(lunar_y);
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
    if (g_year <= END_YEAR) {
      using namespace util::ymd_operator;
      const auto& info = get_lunar_year(g_year);
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
   * @attention The input date should be in the range of [FIRST_LUNAR_DATE, LAST_LUNAR_DATE].
              输入的日期需要在所支持的范围内。
  * @attention `std::nullopt` is returned if the input date is invalid. No exception is thrown.
              输入的日期无效时返回 `std::nullopt`。不会抛出异常。
  */
  static auto lunar_to_gregorian(const year_month_day& lunar_date) -> std::optional<year_month_day> {
    if (not is_valid_lunar(lunar_date)) {
      return std::nullopt;
    }

    const auto [y, m, d] = util::from_ymd(lunar_date);
    const auto& info = get_lunar_year(y);
    const auto& ml = info.month_lengths;

    const uint32_t past_days_count = d + std::reduce(cbegin(ml), cbegin(ml) + m - 1);

    using namespace util::ymd_operator;
    return info.date_of_first_day + (past_days_count - 1);
  }

}; // struct Converter

static_assert(calendar::lunar::common::IsConverter<Converter>);

} // namespace calendar::lunar::algo1
