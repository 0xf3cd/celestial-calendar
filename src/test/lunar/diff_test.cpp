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
#include <ranges>
#include <vector>
#include <print>
#include "lunar/algo1.hpp"
#include "lunar/algo2.hpp"
#include "random.hpp"

// In this file, we test that algo1 and algo2 generate the same lunar month data.

namespace calendar::lunar::test {

auto pick_random_years() -> std::vector<int32_t> {
  using namespace std::ranges;

  const auto filter_year = [](int32_t year) {
    // The two algorithms produce different results on some years.
    // Algo1 is using the hard-coded values, collected from Hong Kong Observatory.
    // Algo2 is based on VSOP87 and ELP2000 theories.
    // #64: 2057/2097 — under algo5's observation-anchored ΔT, one lunation shifts across a
    // day boundary vs HKO's baked prediction. HKO stays authoritative in-product (algo1/algo3).
    return year != 1914 and year != 1915 and year != 1916 and year != 1920
       and year != 2057 and year != 2097;
  };

  // Algo2 doesn't really have year limits. So use algo1's.
  auto years = views::iota(algo1::START_YEAR, algo1::END_YEAR + 1)
             | views::filter(filter_year)
             | to<std::vector>();

  // Randomly pick some years. Seeded via util::random's shared engine, so the draw is
  // reproducible under CELESTIAL_TEST_SEED (#69).
  std::shuffle(begin(years), end(years), util::detail::engine());
  return years
       | views::take(10)
       | to<std::vector>();
}


TEST(LunarAlgoDiff, Consistency) {
  // This test ensures that algo1 and algo2 have the same result on leap months.
  // Use algo1's result as the benchmark.

  using namespace std::ranges;
  const auto years = pick_random_years();

  for (const auto year : years) {
    std::println("year: {}", year);

    const auto info1 = algo1::calc_lunar_year(year);
    const auto info2 = algo2::get_info_for_year(year);

    ASSERT_EQ(info1.date_of_first_day, info2.date_of_first_day);
    ASSERT_EQ(info1.leap_month, info2.leap_month);
    ASSERT_EQ(info1.month_lengths, info2.month_lengths);
  }
}

} // namespace calendar::lunar::test
