/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "celestial.h"
#include "julian_day.hpp"
#include "ymd.hpp"

namespace astro::julian_day::test {

namespace {

/** @brief Reconstruct the returned civil instant without applying a time-scale conversion. */
auto civil_datetime(const CelestialUTCTime& value) -> calendar::Datetime {
  return calendar::Datetime { util::to_ymd(value.year, value.month, value.day), value.fraction };
}

/** @brief Signed civil elapsed seconds, including a change of date. */
auto elapsed_seconds(
  const calendar::Datetime& result,
  const calendar::Datetime& input
) -> double {
  const auto days = std::chrono::sys_days { result.ymd } - std::chrono::sys_days { input.ymd };
  return (static_cast<double>(days.count()) * 86400.0) + ((result.fraction() - input.fraction()) * 86400.0);
}

} // namespace


TEST(UtcConversion, CBoundaryMatchesCpp) {
  // Parity fixtures, not an independent astronomical oracle. SOFA-backed goldens remain in
  // leap_second_test.cpp; these rows pin the public boundary, including noninvertible instants.
  const std::array inputs {
    calendar::Datetime { util::to_ymd(1, 1, 1), 0.0 },
    calendar::Datetime { util::to_ymd(401, 6, 1), 0.56789 },
    calendar::Datetime { util::to_ymd(1900, 1, 1), 0.5 },
    calendar::Datetime { util::to_ymd(1971, 12, 31), 1.0 - (0.001 / 86400.0) },
    calendar::Datetime { util::to_ymd(1972, 1, 1), 0.0 },
    calendar::Datetime { util::to_ymd(1972, 7, 1), 0.0 },
    calendar::Datetime { util::to_ymd(1973, 1, 1), 0.0 },
    calendar::Datetime { util::to_ymd(2017, 1, 1), 0.0 },
    calendar::Datetime { util::to_ymd(2026, 10, 28), 0.5 },
    calendar::Datetime { util::to_ymd(2100, 1, 1), 0.5 },
    calendar::Datetime { util::to_ymd(32767, 12, 31), 0.0 },
  };
  for (const auto& input : inputs) {
    const auto [y, m, d] = util::from_ymd(input.ymd);
    const auto forward = celestial_utc_to_jde(y, m, d, input.fraction());
    ASSERT_TRUE(forward.valid) << celestial_last_error();
    EXPECT_EQ(forward.value, utc_to_jde(input));
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    if (forward.value < 1867522.5) { continue; }

    const auto inverse = celestial_jde_to_utc(forward.value);
    ASSERT_TRUE(inverse.valid) << celestial_last_error();
    const auto expected = jde_to_utc(forward.value);
    EXPECT_EQ(civil_datetime(inverse).ymd, expected.ymd);
    EXPECT_EQ(inverse.fraction, expected.fraction());
    EXPECT_STREQ(celestial_last_error(), "");
  }
}


TEST(UtcConversion, OrdinaryRoundTrips) {
  // 100 us covers modern JDE quantization (~40 us/ulp); the upper year needs 200 us (~161 us/ulp).
  for (const int32_t year : { 1900, 1972, 2026, 2100, 32767 }) {
    const calendar::Datetime input { util::to_ymd(year, 6, 1), 0.56789 };
    const auto forward = celestial_utc_to_jde(year, 6, 1, input.fraction());
    ASSERT_TRUE(forward.valid);
    const auto result = celestial_jde_to_utc(forward.value);
    ASSERT_TRUE(result.valid);
    EXPECT_NEAR(elapsed_seconds(civil_datetime(result), input), 0.0, year == 32767 ? 2e-4 : 1e-4);
  }
}


TEST(UtcConversion, HistoricalSubstituteAndFixedFutureOffset) {
  const calendar::Datetime historical { util::to_ymd(1900, 1, 1), 0.5 };
  EXPECT_EQ(celestial_utc_to_jde(1900, 1, 1, 0.5).value, ut1_to_jde(historical));
  const double historical_jde = ut1_to_jde(historical);
  EXPECT_EQ(civil_datetime(celestial_jde_to_utc(historical_jde)), jde_to_ut1(historical_jde));

  // Policy check: the existing leap table holds TT-UTC = 37 + 32.184 s after its final row.
  for (const int32_t year : { 2017, 2026, 2100, 32767 }) {
    const calendar::Datetime civil { util::to_ymd(year, 1, 1), 0.5 };
    const auto result = celestial_utc_to_jde(year, 1, 1, 0.5);
    ASSERT_TRUE(result.valid);
    EXPECT_NEAR((result.value - ut1_to_jd(civil)) * 86400.0, 69.184, 2e-4);
  }
}


TEST(UtcConversion, ModelSeamIsNotMonotonic) {
  const auto before = celestial_utc_to_jde(1971, 12, 31, 1.0 - (0.001 / 86400.0));
  const auto after = celestial_utc_to_jde(1972, 1, 1, 0.0);
  ASSERT_TRUE(before.valid and after.valid);
  // Algo5's historical offset exceeds the first modern offset by about 68 ms.
  EXPECT_NEAR((after.value - before.value) * 86400.0, -0.067, 1e-4);
  for (const auto& input : std::array {
    calendar::Datetime { util::to_ymd(1971, 12, 31), 1.0 - (0.1 / 86400.0) },
    calendar::Datetime { util::to_ymd(1972, 1, 1), 0.1 / 86400.0 },
  }) {
    const auto [y, m, d] = util::from_ymd(input.ymd);
    const auto result = celestial_jde_to_utc(celestial_utc_to_jde(y, m, d, input.fraction()).value);
    ASSERT_TRUE(result.valid);
    EXPECT_NEAR(elapsed_seconds(civil_datetime(result), input), 0.0, 1e-4);
  }
}


TEST(UtcConversion, LeapStepEdgesAndInsertedSecond) {
  for (const auto ymd : std::array {
    util::to_ymd(1972, 7, 1), util::to_ymd(1973, 1, 1), util::to_ymd(2017, 1, 1),
  }) {
    const calendar::Datetime midnight { ymd, 0.0 };
    const auto [y, m, d] = util::from_ymd(ymd);
    const auto forward = celestial_utc_to_jde(y, m, d, 0.0);
    const auto inverse = celestial_jde_to_utc(forward.value);
    ASSERT_TRUE(forward.valid and inverse.valid);
    EXPECT_EQ(inverse.fraction, jde_to_utc(forward.value).fraction());
    const double midnight_gap = elapsed_seconds(civil_datetime(inverse), midnight);
    // An ulp at midnight may select the adjacent, noninvertible inserted-second interval.
    EXPECT_TRUE(std::abs(midnight_gap) < 1e-4 or std::abs(midnight_gap - 1.0) < 1e-4);

    const std::chrono::year_month_day previous { std::chrono::sys_days { ymd } - std::chrono::days { 1 } };
    for (const auto& input : std::array {
      calendar::Datetime { previous, 1.0 - (0.001 / 86400.0) },
      calendar::Datetime { ymd, 0.001 / 86400.0 },
    }) {
      const auto [iy, im, id] = util::from_ymd(input.ymd);
      const auto result = celestial_jde_to_utc(celestial_utc_to_jde(iy, im, id, input.fraction()).value);
      ASSERT_TRUE(result.valid);
      EXPECT_NEAR(elapsed_seconds(civil_datetime(result), input), 0.0, 1e-4);
    }
  }

  const auto before = celestial_utc_to_jde(2016, 12, 31, 86399.0 / 86400.0);
  const auto after = celestial_utc_to_jde(2017, 1, 1, 0.0);
  ASSERT_TRUE(before.valid and after.valid);
  EXPECT_NEAR((after.value - before.value) * 86400.0, 2.0, 1e-4);
  const double inserted_jde = before.value + (1.5 / 86400.0);
  const auto collapsed = celestial_jde_to_utc(inserted_jde);
  ASSERT_TRUE(collapsed.valid);
  EXPECT_EQ(collapsed.year, 2017);
  EXPECT_EQ(collapsed.month, 1U);
  EXPECT_EQ(collapsed.day, 1U);
  EXPECT_NEAR(collapsed.fraction * 86400.0, 0.5, 1e-4);
  EXPECT_NEAR((celestial_utc_to_jde(2017, 1, 1, collapsed.fraction).value - inserted_jde) * 86400.0, 1.0, 1e-4);
}


TEST(UtcConversion, AsymmetricDomains) {
  const auto first = celestial_utc_to_jde(1, 1, 1, 0.0);
  ASSERT_TRUE(first.valid);
  EXPECT_LT(first.value, 1867522.5);
  EXPECT_FALSE(celestial_jde_to_utc(first.value).valid);
  const auto lower = celestial_jde_to_utc(1867522.5);
  ASSERT_TRUE(lower.valid);
  EXPECT_EQ(lower.year, 400);
  EXPECT_EQ(lower.month, 12U);
  EXPECT_EQ(lower.day, 31U);
  EXPECT_TRUE(celestial_jde_to_utc(std::nextafter(1867522.5, HUGE_VAL)).valid);
  EXPECT_FALSE(celestial_jde_to_utc(std::nextafter(1867522.5, -HUGE_VAL)).valid);
  EXPECT_TRUE(celestial_jde_to_utc(std::nextafter(13689325.5, -HUGE_VAL)).valid);
  EXPECT_FALSE(celestial_jde_to_utc(13689325.5).valid);
  EXPECT_FALSE(celestial_jde_to_utc(std::nextafter(13689325.5, HUGE_VAL)).valid);
  EXPECT_TRUE(celestial_utc_to_jde(32767, 12, 31, 0.0).valid);
  EXPECT_FALSE(celestial_utc_to_jde(32767, 12, 31, 0.9999999).valid);
}


TEST(UtcConversion, InvalidInputsAndErrorRecovery) {
  for (const int32_t year : { 0, -4000, -32768, 32768, INT32_MIN, INT32_MAX }) {
    EXPECT_FALSE(celestial_utc_to_jde(year, 1, 1, 0.0).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), std::to_string(year).c_str()), nullptr);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  for (const auto& [month, day] : std::array {
    std::pair { 2U, 30U }, std::pair { 13U, 1U }, std::pair { 257U, 1U }, std::pair { 1U, 257U },
  }) {
    EXPECT_FALSE(celestial_utc_to_jde(2024, month, day, 0.0).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  for (const double fraction : { -1e-300, 1.0, HUGE_VAL, -HUGE_VAL, std::numeric_limits<double>::quiet_NaN() }) {
    EXPECT_FALSE(celestial_utc_to_jde(2024, 1, 1, fraction).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_STRNE(celestial_last_error(), "");
  }
  ASSERT_TRUE(celestial_utc_to_jde(2024, 1, 1, -0.0).valid);
  EXPECT_STREQ(celestial_last_error(), "");
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  EXPECT_EQ(celestial_utc_to_jde(2024, 1, 1, -0.0).value, celestial_utc_to_jde(2024, 1, 1, 0.0).value);
  EXPECT_TRUE(celestial_utc_to_jde(2024, 1, 1, std::nextafter(1.0, 0.0)).valid);
  for (const double jde_tt : { HUGE_VAL, -HUGE_VAL, std::numeric_limits<double>::quiet_NaN() }) {
    EXPECT_FALSE(celestial_jde_to_utc(jde_tt).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_STRNE(celestial_last_error(), "");
  }
  EXPECT_TRUE(celestial_jde_to_utc(2451545.0).valid);
  EXPECT_STREQ(celestial_last_error(), "");
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
}

} // namespace astro::julian_day::test
