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

#include <tuple>
#include <vector>
#include "random.hpp"
#include "lunar/algo1.hpp"

namespace calendar::lunar::algo1::test {

// Retained material boundary (V01): HKO-derived expected rows remain under their source terms and
// outside the project MIT grant; upstream permission was not obtained and is not claimed.

using namespace calendar::lunar::common;
using namespace calendar::lunar::algo1;

TEST(LunarAlgo1, ArraySize) {
  EXPECT_EQ(199, LUNAR_DATA.size());
  EXPECT_EQ(END_YEAR - START_YEAR + 1, LUNAR_DATA.size());
}

TEST(LunarAlgo1, LunarYear) {
  ASSERT_THROW(std::ignore = calc_lunar_year(START_YEAR - 1), std::out_of_range);
  ASSERT_THROW(std::ignore = calc_lunar_year(END_YEAR + 1), std::out_of_range);

  const auto check_month_lengths = [](const auto& l1, const auto& l2) -> bool {
    if (l1.size() != l2.size()) {
      return false;
    }
    for (size_t i = 0; i < l1.size(); ++i) {
      if (l1[i] != l2[i]) {
        return false;
      }
    }
    return true;
  };

  auto info = calc_lunar_year(1901);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 1901 } / 2 / 19);
  EXPECT_EQ(info.leap_month, 0);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 }
  ));

  info = calc_lunar_year(1903);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 1903 } / 1 / 29);
  EXPECT_EQ(info.leap_month, 5);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 29, 30, 29, 30, 29, 29, 30, 29, 29, 30, 30, 29, 30 }
  ));

  info = calc_lunar_year(2099);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 2099 } / 1 / 21);
  EXPECT_EQ(info.leap_month, 2);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 30, 30, 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30 }
  ));
}

TEST(LunarAlgo1, Copy) {
  for (auto _ = 0; _ < 100; ++_) {
    auto info = calc_lunar_year(util::random(START_YEAR, END_YEAR));
    auto info2 = info;

    EXPECT_NE(&info, &info2);
    EXPECT_NE(&info.month_lengths, &info2.month_lengths);

    EXPECT_EQ(info.date_of_first_day, info2.date_of_first_day);
    EXPECT_EQ(info.leap_month, info2.leap_month);
    EXPECT_EQ(info.month_lengths, info2.month_lengths);

    info.month_lengths.emplace_back(29);
    EXPECT_NE(info.month_lengths, info2.month_lengths);
  }
}

TEST(LunarAlgo1, MetadataConsistency) {
  // #75: the memo wrapper is gone — `AlgoMetadata` forwards straight to `calc_lunar_year`.
  for (auto _ = 0; _ < 100; ++_) {
    const auto year = util::random(START_YEAR, END_YEAR);
    const auto info = calc_lunar_year(year);
    const auto info2 = AlgoMetadata<Algo::ALGO_1>::get_info_for_year(year);
    EXPECT_EQ(info.date_of_first_day, info2.date_of_first_day);
    EXPECT_EQ(info.leap_month, info2.leap_month);
    EXPECT_EQ(info.month_lengths, info2.month_lengths);
  }
}

} // namespace calendar::lunar::algo1::test
