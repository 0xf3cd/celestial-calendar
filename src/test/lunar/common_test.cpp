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

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "lunar/algo1.hpp"
#include "lunar/algo3.hpp"
#include "lunar/common.hpp"

namespace calendar::lunar::common::test {

// Provenance of the 2023 pins: lunar 2023 carries a leap 2nd month (闰二月) — HKO data
// (https://www.hko.gov.hk/sc/gts/time/conversion.htm, published for 1901-2100) baked into
// algo1's table (1901-2099); algo3's baked entry for 2023 must agree.
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
  EXPECT_EQ(month_position(info2023, 2, true), 3);  // leap 2nd month (闰二月) = position 3
  EXPECT_EQ(month_position(info2023, 3, false), 4); // traditional 3rd month = position 4
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

// Direct pin of the LUNAR_DATA bitmap decoder (#87): days_offset = encoded >> 17,
// leap_month = (encoded >> 13) & 0xf, month lengths from the low 13 bits (bit set = 30
// days, clear = 29; 13 months iff leap_month != 0).
// Provenance: `encoded` is copied verbatim from algo1::LUNAR_DATA[year - 1901] (HKO
// 1901-2100 conversion table, https://www.hko.gov.hk/sc/gts/time/conversion.htm); the
// expected fields were hand-decoded from the bitmap and cross-checked against ytliu0's
// calendarData.js (pinned commit d6aae82b63b79a6f8659ea3e064024b7d8ac3077, md5
// 6c9649f384d178918d9cb4618f7d3e98; decode via statistics/algo3_ytliu0_golden.py
// `extract_ytliu0_year`) — all four years agree. Public anchors: lunar 2023 starts
// 2023-01-22 with a leap 2nd month (闰二月, the `TraditionalMonth` docstring instance),
// lunar 2024 starts 2024-02-10 with no leap month.
// Integer civil-day fields: exact equality (EXPECT_EQ per field).
TEST(LunarCommon, ParseLunarYear) {
  struct ParseCase {
    int32_t year;
    uint32_t encoded;
    std::chrono::year_month_day first_day;
    uint8_t leap_month;
    std::vector<uint32_t> month_lengths;
  };

  const std::vector<ParseCase> cases {
    { .year = 1901, .encoded = 0x620752, .first_day = std::chrono::year { 1901 } / 2 / 19, .leap_month = 0, .month_lengths = { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 } },              // START_YEAR (table start)
    { .year = 2023, .encoded = 0x2a55b2, .first_day = std::chrono::year { 2023 } / 1 / 22, .leap_month = 2, .month_lengths = { 29, 30, 29, 29, 30, 30, 29, 30, 30, 29, 30, 29, 30 } },  // leap 2nd month (闰二月)
    { .year = 2024, .encoded = 0x5006d2, .first_day = std::chrono::year { 2024 } / 2 / 10, .leap_month = 0, .month_lengths = { 29, 30, 29, 29, 30, 29, 30, 30, 29, 30, 30, 29 } },              // no leap month
    { .year = 2099, .encoded = 0x28549b, .first_day = std::chrono::year { 2099 } / 1 / 21, .leap_month = 2, .month_lengths = { 30, 30, 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30 } },  // END_YEAR (table end)
  };

  for (const auto& c : cases) {
    const auto info = parse_lunar_year(c.year, c.encoded);
    EXPECT_EQ(info.date_of_first_day, c.first_day) << "year=" << c.year;
    EXPECT_EQ(info.leap_month, c.leap_month) << "year=" << c.year;
    EXPECT_EQ(info.month_lengths, c.month_lengths) << "year=" << c.year;
  }
}

TEST(LunarCommon, CalcBounds) {
  // calc_bounds threads the algo through its declared window (#87): the lunar dates come
  // from the window arguments, the Gregorian dates from `algo_f` at the two ends.
  const auto b = calc_bounds(algo1::START_YEAR, algo1::END_YEAR, algo1::calc_lunar_year);

  EXPECT_EQ(b.start_lunar_year, algo1::START_YEAR);
  EXPECT_EQ(b.end_lunar_year, algo1::END_YEAR);
  EXPECT_EQ(b.first_lunar_date, std::chrono::year { 1901 } / 1 / 1);
  // Lunar 2099 has a leap 2nd month (13 months), so its last day is 2099/13/30.
  EXPECT_EQ(b.last_lunar_date, std::chrono::year { 2099 } / 13 / 30);
  // Lunar 1901 starts 1901-02-19 (HKO); lunar 2099 ends the day before lunar 2100's first
  // day (2100-02-09, ytliu0 pinned commit above), i.e. 2100-02-08.
  EXPECT_EQ(b.first_gregorian_date, std::chrono::year { 1901 } / 2 / 19);
  EXPECT_EQ(b.last_gregorian_date, std::chrono::year { 2100 } / 2 / 8);

  // No guard of its own: an out-of-window year falls through to the algo's out_of_range.
  ASSERT_THROW(std::ignore = calc_bounds(1900, 2099, algo1::calc_lunar_year), std::out_of_range);
  ASSERT_THROW(std::ignore = calc_bounds(1901, 2100, algo1::calc_lunar_year), std::out_of_range);
}

} // namespace calendar::lunar::common::test
