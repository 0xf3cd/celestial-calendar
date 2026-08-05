/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
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
#include "lunar/algo1.hpp"
#include "lunar/algo3.hpp"
#include "lunar/common.hpp"

namespace calendar::lunar::common::test {

// Provenance of the 2023 pins: lunar 2023 carries a leap 2nd month (闰二月) — HKO data
// (https://www.hko.gov.hk/sc/gts/time/conversion.htm) baked into algo1's table, which covers
// 1901-2100; algo3's baked entry for 2023 must agree.
TEST(LunarCommon, Leap2023DataAgreement) {
  EXPECT_EQ(algo1::calc_lunar_year(2023).leap_month, 2);
  EXPECT_EQ(algo3::calc_lunar_year(2023).leap_month, 2);
}

TEST(LunarCommon, MonthPositionLeapYear) {
  const auto info2023 = algo1::calc_lunar_year(2023);
  ASSERT_EQ(info2023.leap_month, 2);
  ASSERT_EQ(info2023.month_lengths.size(), 13);

  EXPECT_EQ(month_position(info2023, 1, false), 1);
  EXPECT_EQ(month_position(info2023, 2, false), 2);
  EXPECT_EQ(month_position(info2023, 2, true), 3);  // 闰二月 = 位置 3
  EXPECT_EQ(month_position(info2023, 3, false), 4); // 传统三月 = 位置 4
  EXPECT_EQ(month_position(info2023, 12, false), 13);
}

TEST(LunarCommon, MonthPositionRejectsBadInput) {
  const auto info2023 = algo1::calc_lunar_year(2023);

  const auto info2024 = algo1::calc_lunar_year(2024);
  ASSERT_EQ(info2024.leap_month, 0); // 2024 has no leap month.
  ASSERT_EQ(info2024.month_lengths.size(), 12);

  EXPECT_EQ(month_position(info2023, 0, false), std::nullopt);
  EXPECT_EQ(month_position(info2023, 13, false), std::nullopt);
  EXPECT_EQ(month_position(info2023, 5, true), std::nullopt); // 2023's leap month is the 2nd, not the 5th.
  EXPECT_EQ(month_position(info2024, 2, true), std::nullopt); // no leap month in 2024 at all.
}

TEST(LunarCommon, MonthAtPosition) {
  const auto info2023 = algo1::calc_lunar_year(2023);

  EXPECT_EQ(month_at_position(info2023, 1), (TraditionalMonth { 1, false }));
  EXPECT_EQ(month_at_position(info2023, 2), (TraditionalMonth { 2, false }));
  EXPECT_EQ(month_at_position(info2023, 3), (TraditionalMonth { 2, true }));
  EXPECT_EQ(month_at_position(info2023, 4), (TraditionalMonth { 3, false }));
  EXPECT_EQ(month_at_position(info2023, 13), (TraditionalMonth { 12, false }));

  EXPECT_EQ(month_at_position(info2023, 0), std::nullopt);
  EXPECT_EQ(month_at_position(info2023, 14), std::nullopt);

  const auto info2024 = algo1::calc_lunar_year(2024);
  EXPECT_EQ(month_at_position(info2024, 12), (TraditionalMonth { 12, false }));
  EXPECT_EQ(month_at_position(info2024, 13), std::nullopt);
}

TEST(LunarCommon, MonthTranslationRoundTrip) {
  // Conservation: position → traditional → position is the identity on every valid
  // position of every year algo1 covers.
  for (int32_t year = algo1::START_YEAR; year <= algo1::END_YEAR; ++year) {
    const auto info = algo1::calc_lunar_year(year);
    for (uint32_t pos = 1; pos <= info.month_lengths.size(); ++pos) {
      const auto tm = month_at_position(info, static_cast<uint8_t>(pos));
      ASSERT_TRUE(tm.has_value());
      // NOLINTNEXTLINE(bugprone-unchecked-optional-access) -- guarded by the ASSERT_TRUE above.
      EXPECT_EQ(month_position(info, tm->month, tm->is_leap), pos);
    }
  }
}

} // namespace calendar::lunar::common::test
