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

#include <array>
#include <format>

#include "util.hpp"
#include "common.hpp"


namespace calendar::lunar::algo1 {

using namespace calendar::lunar::common;

/** @brief The first supported lunar year. */
constexpr int32_t START_YEAR = 1901;

/** @brief The last supported lunar year. */
constexpr int32_t END_YEAR = 2099;

/** 
 * @brief The encoded binary data for each lunar year. Info for a year is stored in a uint32_t. 
 * @ref https://www.hko.gov.hk/sc/gts/time/conversion.htm
 * @details Data collected from Hong Kong Observatory.
 */
constexpr std::array<uint32_t, (END_YEAR - START_YEAR + 1)> LUNAR_DATA = {
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
 * @brief Calculate the lunar year information for the given year.
 * @attention The input year should be in the range of [START_YEAR, END_YEAR].
 * @param year The Lunar year. 阴历年份。
 * @return The lunar year information. 阴历年信息。
 */
inline auto calc_lunar_year_info(int32_t year) -> LunarYear {
  if (year < START_YEAR or year > END_YEAR) {
    throw std::out_of_range {
      std::format("year {} is out of range [{}, {}]", year, START_YEAR, END_YEAR)
    };
  }

  return parse_lunar_year(year, LUNAR_DATA[year - START_YEAR]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

/**
 * @brief Same function as `calc_lunar_year_info`, but cached. 
          与 `calc_lunar_year_info` 功能相同，但使用缓存。
 * @attention The input year should be in the range of [START_YEAR, END_YEAR].
 * @param year The Lunar year. 阴历年份。
 * @return The lunar year information. 阴历年信息。
 */
const inline auto get_info_for_year = util::cache::cache_func(calc_lunar_year_info);

/** @brief The bounds of the algorithm, i.e. the supported range of lunar and Gregorian dates. */
const inline auto bounds = calc_bounds(START_YEAR, END_YEAR, get_info_for_year);

} // namespace calendar::lunar::algo1


namespace calendar::lunar::common {

/** @brief Specialize `AlgoMetadata` for `Algo::ALGO_1`. */
template <>
struct AlgoMetadata<Algo::ALGO_1> {
  static const inline auto get_info_for_year = algo1::get_info_for_year;
  static const inline auto bounds = algo1::bounds;
};

} // namespace calendar::lunar::common
