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

#include <gtest/gtest.h>
#include <algorithm>
#include <vector>
#include "lunar/algo1.hpp"
#include "lunar/algo2.hpp"
#include "lunar/algo3.hpp"
#include "random.hpp"

namespace calendar::lunar::algo3::test {

using namespace calendar::lunar::common;

TEST(LunarAlgo3, Correctness) {
  const auto in_algo1_range = [](const int32_t year) -> bool {
    return algo1::START_YEAR <= year and year <= algo1::END_YEAR;
  };

  const auto expected_info = [&](const int32_t year) -> LunarYear {
    if (in_algo1_range(year)) {
      return algo1::calc_lunar_year(year);
    }
    return algo2::calc_lunar_year(year);
  };

  std::vector<int32_t> years = std::views::iota(algo3::START_YEAR, algo3::END_YEAR + 1)
                             | std::ranges::to<std::vector>();

  std::shuffle(years.begin(), years.end(), util::detail::engine()); // Seeded, reproducible (#69).
  years.resize(32);

  for (const auto year : years) {
    const auto expected = expected_info(year);
    const auto actual = algo3::calc_lunar_year(year);

    ASSERT_EQ(expected.date_of_first_day, actual.date_of_first_day);
    ASSERT_EQ(expected.leap_month, actual.leap_month);
    ASSERT_EQ(expected.month_lengths, actual.month_lengths);
  }

  // #64: entries re-baked under algo5's ΔT — checked deterministically, since the random
  // sample above only covers them ~15% of the time.
  for (const int32_t year : { 2133, 2165, 2172 }) {
    const auto expected = expected_info(year);
    const auto actual = algo3::calc_lunar_year(year);

    ASSERT_EQ(expected.date_of_first_day, actual.date_of_first_day);
    ASSERT_EQ(expected.leap_month, actual.leap_month);
    ASSERT_EQ(expected.month_lengths, actual.month_lengths);
  }
}

} // namespace calendar::lunar::algo3::test
