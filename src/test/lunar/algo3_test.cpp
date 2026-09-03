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

#include <gtest/gtest.h>
#include "lunar/algo1.hpp"
#include "lunar/algo2.hpp"
#include "lunar/algo3.hpp"

namespace calendar::lunar::algo3::test {

// Retained material boundary (V01): checks over the copied HKO slice remain under the HKO source
// terms and outside the project MIT grant; upstream permission was not obtained and is not claimed.

using namespace calendar::lunar::common;

// External oracle for algo3 is the ytliu0 golden (algo3_ytliu0_golden_test.cpp);
// neither test below is one.

// [1901, 2099]: algo3's baked slice is a byte-for-byte copy of algo1's HKO table.
// Both sides decode via the shared `parse_lunar_year`, so this is a dual-table
// drift check — it catches "only one of the two tables was edited", not an
// independent correctness proof. Full window (199 years): decode is free.
TEST(LunarAlgo3, HkoTableCopiesAgree) {
  for (int32_t year = algo1::START_YEAR; year <= algo1::END_YEAR; ++year) {
    const auto expected = algo1::calc_lunar_year(year);
    const auto actual = algo3::calc_lunar_year(year);

    ASSERT_EQ(expected.date_of_first_day, actual.date_of_first_day) << "year=" << year;
    ASSERT_EQ(expected.leap_month, actual.leap_month) << "year=" << year;
    ASSERT_EQ(expected.month_lengths, actual.month_lengths) << "year=" << year;
  }
}

// [1600, 1900] ∪ [2100, 2199]: all 401 baked values must equal a current live-algo2
// regeneration. This is a full drift gate, not an external oracle (that is the ytliu0 golden).
TEST(LunarAlgo3, BakedMatchesLiveAlgo2) {
  int32_t checked = 0;
  for (int32_t year = START_YEAR; year <= END_YEAR; ++year) {
    if (year >= algo1::START_YEAR and year <= algo1::END_YEAR) {
      continue;
    }
    const auto expected = algo2::calc_lunar_year(year);
    const auto actual = algo3::calc_lunar_year(year);

    ASSERT_EQ(expected.date_of_first_day, actual.date_of_first_day) << "year=" << year;
    ASSERT_EQ(expected.leap_month, actual.leap_month) << "year=" << year;
    ASSERT_EQ(expected.month_lengths, actual.month_lengths) << "year=" << year;
    ++checked;
  }
  ASSERT_EQ(401, checked);
}

// Structural pin: `calc_lunar_year` must decode `LUNAR_DATA`, not bypass to live
// algo2. Years chosen from the known HKO divergences (baked ≡ algo1 ≠ live algo2),
// so a pure live bypass fails without first dirtying the table. On re-bakeable years
// baked already equals live, so this pin would be silent there.
TEST(LunarAlgo3, CalcReadsBakedTable) {
  for (const int32_t year : { 1914, 1920, 2097 }) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index) — year ∈ {1914,1920,2097}, all in [START_YEAR, END_YEAR]; bounds guaranteed by fixed literal set
    const auto from_table = parse_lunar_year(year, LUNAR_DATA[year - START_YEAR]);
    const auto from_calc = calc_lunar_year(year);
    ASSERT_EQ(from_table.date_of_first_day, from_calc.date_of_first_day) << "year=" << year;
    ASSERT_EQ(from_table.leap_month, from_calc.leap_month) << "year=" << year;
    ASSERT_EQ(from_table.month_lengths, from_calc.month_lengths) << "year=" << year;
  }
}

} // namespace calendar::lunar::algo3::test
