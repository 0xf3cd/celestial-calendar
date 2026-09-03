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

// In this file, we test that algo1 and algo2 generate the same lunar month data —
// and, for the four known divergence years, that each side still matches its own
// absolute pin (#70 §1 / D-Q).
//
// ## Known divergences 1914 / 1915 / 1916 / 1920 (Provenance)
//
// algo1 = HKO hard-coded table (T{Y}c.txt); algo2 = live VSOP87/ELP2000 + algo5 ΔT.
// Four years disagree by one day at one lunation boundary. Direction is uniform:
// the almanac (algo1) places 初一 one civil day earlier than pure calculation (algo2).
// Mechanism (ytliu0 廖育棟, ChineseCalendar computation page, accessed 2026-08-05):
// from 1914 the published almanac switched to Beijing local mean time.
// Pins only these four measured years — no Qing closed rule claimed; 1906 falsifies
// pure time-zone extrapolation outside that era.
//
// Three-layer absolute pins: notebook cell output in statistics/lunar_calendar.ipynb;
// HKO T1914c / T1916c / T1920c.txt; ytliu0 page (mechanism + 初一 rows).
// Near-midnight scan: python3 statistics/algo3_ytliu0_golden.py scan-near-midnight

namespace calendar::lunar::test {

// Retained material boundary (V02): the HKO divergence anchors remain under their source terms and
// outside the project MIT grant; upstream permission was not obtained and is not claimed.

auto pick_random_years() -> std::vector<int32_t> {
  using namespace std::ranges;

  const auto filter_year = [](int32_t year) {
    // 1914/1915/1916/1920 → pinned in KnownDivergences below (#70 / D-Q).
    // 2057/2097 → still #64 (near-midnight syzygy under algo5 ΔT); excluded here and
    // NOT pinned in this PR (called out in the #70 close-out comment).
    return year != 1914 and year != 1915 and year != 1916 and year != 1920
       and year != 2057 and year != 2097;
  };

  // algo1's window is the narrower of the two, so it bounds the comparison.
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


// Absolute pins on both sides. Equal fields are pinned too so a future "agreement
// regression" (one side drifting toward the other) also fails loudly.
TEST(LunarAlgoDiff, KnownDivergences) {
  using std::chrono::year;
  using std::vector;
  using uint = uint32_t;

  // --- 1914: month_lengths[9]/[10] swap; first_day and leap_month(5) agree ---
  // Root event 1914-11-18 00:01:49 UTC+8 (~109 s). HKO T1914c: 十月 starts 11-17.
  {
    const auto a1 = algo1::calc_lunar_year(1914);
    const auto a2 = algo2::get_info_for_year(1914);

    EXPECT_EQ(a1.date_of_first_day, year { 1914 } / 1 / 26);
    EXPECT_EQ(a2.date_of_first_day, year { 1914 } / 1 / 26);
    EXPECT_EQ(a1.leap_month, 5);
    EXPECT_EQ(a2.leap_month, 5);
    EXPECT_EQ(a1.month_lengths,
              (vector<uint> { 30, 30, 29, 30, 29, 30, 29, 30, 29, 29, 30, 29, 30 }));
    EXPECT_EQ(a2.month_lengths,
              (vector<uint> { 30, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29, 29, 30 }));
  }

  // --- 1915: only month_lengths[11] 29 vs 30 ---
  // Not an independent event: upper bound of the 1916-02-04 new moon on the previous
  // lunar year's last month. HKO T1916c: 十二月 starts 1916-01-05, 正月 02-03 ⇒ 29 days.
  {
    const auto a1 = algo1::calc_lunar_year(1915);
    const auto a2 = algo2::get_info_for_year(1915);

    EXPECT_EQ(a1.date_of_first_day, year { 1915 } / 2 / 14);
    EXPECT_EQ(a2.date_of_first_day, year { 1915 } / 2 / 14);
    EXPECT_EQ(a1.leap_month, 0);
    EXPECT_EQ(a2.leap_month, 0);
    EXPECT_EQ(a1.month_lengths,
              (vector<uint> { 30, 29, 30, 30, 29, 30, 29, 30, 29, 30, 29, 29 }));
    EXPECT_EQ(a2.month_lengths,
              (vector<uint> { 30, 29, 30, 30, 29, 30, 29, 30, 29, 30, 29, 30 }));
  }

  // --- 1916: date_of_first_day + month_lengths[0] ---
  // Root event 1916-02-04 00:05:13 UTC+8 (~313 s). HKO T1916c: 正月 starts 02-03.
  {
    const auto a1 = algo1::calc_lunar_year(1916);
    const auto a2 = algo2::get_info_for_year(1916);

    EXPECT_EQ(a1.date_of_first_day, year { 1916 } / 2 / 3);
    EXPECT_EQ(a2.date_of_first_day, year { 1916 } / 2 / 4);
    EXPECT_EQ(a1.leap_month, 0);
    EXPECT_EQ(a2.leap_month, 0);
    EXPECT_EQ(a1.month_lengths,
              (vector<uint> { 30, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29 }));
    EXPECT_EQ(a2.month_lengths,
              (vector<uint> { 29, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29 }));
  }

  // --- 1920: month_lengths[8]/[9] swap; first_day and leap_month agree ---
  // Root event 1920-11-11 00:04:48 UTC+8 (~288 s). HKO T1920c: 十月 starts 11-10.
  {
    const auto a1 = algo1::calc_lunar_year(1920);
    const auto a2 = algo2::get_info_for_year(1920);

    EXPECT_EQ(a1.date_of_first_day, year { 1920 } / 2 / 20);
    EXPECT_EQ(a2.date_of_first_day, year { 1920 } / 2 / 20);
    EXPECT_EQ(a1.leap_month, 0);
    EXPECT_EQ(a2.leap_month, 0);
    EXPECT_EQ(a1.month_lengths,
              (vector<uint> { 29, 30, 29, 29, 30, 29, 29, 30, 29, 30, 30, 30 }));
    EXPECT_EQ(a2.month_lengths,
              (vector<uint> { 29, 30, 29, 29, 30, 29, 29, 30, 30, 29, 30, 30 }));
  }
}

} // namespace calendar::lunar::test
