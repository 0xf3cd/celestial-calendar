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

// The former single TEST(LunarAlgo3, Correctness) covered two windows with two
// different meanings of "expected". Split so each name says what it proves.
// External oracle for algo3 is the ytliu0 golden (algo3_ytliu0_golden_test.cpp),
// not either of the tests below.

// [1901, 2099]: algo3's baked slice is a byte-for-byte copy of algo1's HKO table
// (driver probe: 199 entries, identical encodings). Both sides decode via the
// shared `parse_lunar_year`, so this is a dual-table drift check — it catches
// "only one of the two tables was edited", not an independent correctness proof.
// Decoding itself has no independent discriminating power here.
TEST(LunarAlgo3, HkoTableCopiesAgree) {
  std::vector<int32_t> years = std::views::iota(algo1::START_YEAR, algo1::END_YEAR + 1)
                             | std::ranges::to<std::vector>();

  std::shuffle(years.begin(), years.end(), util::detail::engine()); // Seeded (#69).
  years.resize(32);

  for (const auto year : years) {
    const auto expected = algo1::calc_lunar_year(year);
    const auto actual = algo3::calc_lunar_year(year);

    ASSERT_EQ(expected.date_of_first_day, actual.date_of_first_day) << "year=" << year;
    ASSERT_EQ(expected.leap_month, actual.leap_month) << "year=" << year;
    ASSERT_EQ(expected.month_lengths, actual.month_lengths) << "year=" << year;
  }
}

// [1600, 1900] ∪ [2100, 2199]: algo3's baked value must still equal *today's*
// live `algo2::calc_lunar_year` recompute. This HAS real discriminating power —
// #64's 2133/2165/2172 first surfaced as drift against live algo2 under a new
// default ΔT. It is NOT an external oracle (that is the ytliu0 golden).
TEST(LunarAlgo3, BakedMatchesLiveAlgo2) {
  const auto outside_hko = [](const int32_t year) -> bool {
    return year < algo1::START_YEAR or year > algo1::END_YEAR;
  };

  std::vector<int32_t> years = std::views::iota(algo3::START_YEAR, algo3::END_YEAR + 1)
                             | std::views::filter(outside_hko)
                             | std::ranges::to<std::vector>();

  std::shuffle(years.begin(), years.end(), util::detail::engine()); // Seeded (#69).
  years.resize(32);

  for (const auto year : years) {
    const auto expected = algo2::calc_lunar_year(year);
    const auto actual = algo3::calc_lunar_year(year);

    ASSERT_EQ(expected.date_of_first_day, actual.date_of_first_day) << "year=" << year;
    ASSERT_EQ(expected.leap_month, actual.leap_month) << "year=" << year;
    ASSERT_EQ(expected.month_lengths, actual.month_lengths) << "year=" << year;
  }

  // #64: entries re-baked under algo5's ΔT — checked deterministically, since the
  // random sample above only covers them ~15% of the time. Belongs on this test
  // (baked vs live algo2), not on the HKO dual-table check.
  for (const int32_t year : { 2133, 2165, 2172 }) {
    const auto expected = algo2::calc_lunar_year(year);
    const auto actual = algo3::calc_lunar_year(year);

    ASSERT_EQ(expected.date_of_first_day, actual.date_of_first_day) << "year=" << year;
    ASSERT_EQ(expected.leap_month, actual.leap_month) << "year=" << year;
    ASSERT_EQ(expected.month_lengths, actual.month_lengths) << "year=" << year;
  }
}

} // namespace calendar::lunar::algo3::test
