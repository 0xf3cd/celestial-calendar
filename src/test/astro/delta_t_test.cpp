/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

#include <tuple>
#include <numeric>
#include "util.hpp"
#include "datetime.hpp"
#include "delta_t.hpp"
#include "delta_t_test_helper.hpp"

namespace astro::delta_t_test {

using namespace astro::delta_t;

TEST(DeltaT, Algo1) {
  ASSERT_THROW(std::ignore = algo1::compute(-4001), std::out_of_range);

  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo1::compute(500.0), 5710.0, 5.0);
  ASSERT_NEAR(algo1::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo1::compute(2008.0),  66.0, 0.1);

  // #64: -500.5 belongs to [-4000, -500); truncation toward zero used to pick the wrong segment.
  ASSERT_NEAR(algo1::compute(-500.5), 17211.124799999998, 1e-6);

  // #64: the post-2005 blend used to jump by ~1.09 s at 2015 and ~0.28 s at 2115.
  ASSERT_NEAR(algo1::compute(2014.9999), algo1::compute(2015.0001), 0.01);
  ASSERT_NEAR(algo1::compute(2114.9999), algo1::compute(2115.0001), 0.01);
}

TEST(DeltaT, Algo2) {
  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo2::compute(500.0), 5710.0, 1.0);
  ASSERT_NEAR(algo2::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo2::compute(2008.0),  66.0, 0.15);
}

TEST(DeltaT, Algo3) {
  ASSERT_THROW(std::ignore = algo3::compute(3000.1), std::out_of_range);

  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo3::compute(500.0), 5710.0, 1.0);
  ASSERT_NEAR(algo3::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo3::compute(2008.0),  66.0, 0.5);
}

TEST(DeltaT, Algo4) {
  ASSERT_THROW(std::ignore = algo4::compute(2035.1), std::out_of_range);

  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo4::compute(500.0), 5710.0, 1.0);
  ASSERT_NEAR(algo4::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo4::compute(2008.0),  66.0, 0.6);
}

TEST(DeltaT, Algo5) {
  // No upper bound — far-future years extrapolate instead of throwing. The noexcept
  // contract is pinned at compile time; the value checks smoke-test the extrapolation.
  static_assert(noexcept(algo5::compute(3000.0)));
  ASSERT_GT(algo5::compute(2035.1), 0.0);
  ASSERT_GT(algo5::compute(3000.0), 0.0);

  // Below 2005 algo5 delegates to algo2 — identical code path, exactly equal.
  ASSERT_EQ(algo5::compute(500.0),    algo2::compute(500.0));
  ASSERT_EQ(algo5::compute(1950.0),   algo2::compute(1950.0));
  ASSERT_EQ(algo5::compute(2004.999), algo2::compute(2004.999));

  // Goldens from the training script (AstroTime-Analysis DeltaT/algo5.py);
  // 1e-9 so a mistyped coefficient can't hide.
  ASSERT_NEAR(algo5::compute(2005.0), 64.69707499997821, 1e-9);
  ASSERT_NEAR(algo5::compute(2020.0), 69.3387465593114,  1e-9);
  ASSERT_NEAR(algo5::compute(algo5::LAST_OBSERVATION_YEAR), 69.15162999997847, 1e-9);
  ASSERT_NEAR(algo5::compute(2030.0), 69.38127113857495, 1e-9);
  ASSERT_NEAR(algo5::compute(2100.0), 86.90240401769715, 1e-9);

  // Continuity at both internal boundaries (the 2005 seam vs algo2 measures +0.027 s).
  ASSERT_NEAR(algo5::compute(2004.99999), algo5::compute(2005.00001), 0.05);
  ASSERT_NEAR(algo5::compute(algo5::LAST_OBSERVATION_YEAR - 1e-6),
              algo5::compute(algo5::LAST_OBSERVATION_YEAR + 1e-6), 1e-3);
}

TEST(DeltaT, NonFiniteYears) {
  // #86: the throwing algos reject every non-finite year; the noexcept ones (algo2, algo5,
  // the dispatcher) propagate it — IEEE, not UB.
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  ASSERT_THROW(std::ignore = algo1::compute(nan), std::out_of_range);
  ASSERT_THROW(std::ignore = algo3::compute(nan), std::out_of_range);
  ASSERT_THROW(std::ignore = algo4::compute(nan), std::out_of_range);
  ASSERT_THROW(std::ignore = algo1::compute(inf),  std::out_of_range);
  ASSERT_THROW(std::ignore = algo1::compute(-inf), std::out_of_range);
  ASSERT_THROW(std::ignore = algo3::compute(inf),  std::out_of_range);
  ASSERT_THROW(std::ignore = algo3::compute(-inf), std::out_of_range);
  ASSERT_THROW(std::ignore = algo4::compute(inf),  std::out_of_range);
  ASSERT_THROW(std::ignore = algo4::compute(-inf), std::out_of_range);
  // The noexcept ones propagate: non-finite in, non-finite out.
  ASSERT_TRUE(std::isnan(algo2::compute(nan)));
  ASSERT_TRUE(std::isnan(algo5::compute(nan)));
  ASSERT_TRUE(std::isnan(compute(nan)));
  ASSERT_FALSE(std::isfinite(algo2::compute(inf)));
  ASSERT_FALSE(std::isfinite(algo5::compute(inf)));
  ASSERT_FALSE(std::isfinite(compute(-inf)));
  ASSERT_NO_THROW(std::ignore = algo1::compute(1e300));
}


TEST(DeltaT, DefaultDispatch) {
  // The default `compute` is algo5 for all years.
  ASSERT_EQ(compute(1950.0), algo5::compute(1950.0));
  ASSERT_EQ(compute(2020.0), algo5::compute(2020.0));
  ASSERT_EQ(compute(2040.0), algo5::compute(2040.0));

  // #64 regression: the old algo4/algo2 dispatch jumped by ~9.5 s at 2035.0.
  ASSERT_NEAR(compute(2034.9999), compute(2035.0001), 0.01);
}

#pragma region Delta T Algorithms Statistics

TEST(DeltaT, Statistics) {
  using namespace std::ranges;

  {
    std::println("ΔT results:");

    const auto head_line = make_line(std::vector { "year", "expected" }, algo_info::DELTA_T_ALGO_NAMES);
    const auto devider = std::string(head_line.size(), '-');

    std::println("{}", devider);
    std::println("{}", head_line);
    std::println("{}", devider);

    for (const auto& [year, expected_delta_t] : dataset::test::ACCURATE_DELTA_T_TABLE) {
      const auto datapoint_line = make_line(
        std::vector { pad(year), pad(expected_delta_t) }, 
        operation::evaluate(year)
      );

      std::println("{}", datapoint_line);
    }

    std::println("{}", devider);
    std::println("");
  }

  {
    std::println("ΔT differences from observations:");

    const auto head_line = make_line(std::vector { "year" }, algo_info::DELTA_T_ALGO_NAMES);
    const auto devider = std::string(head_line.size(), '-');

    std::println("{}", devider);
    std::println("{}", head_line);
    std::println("{}", devider);

    for (const auto& [year, expected_delta_t] : dataset::test::ACCURATE_DELTA_T_TABLE) {
      std::println("{}", make_line(
        std::vector { year },
        operation::calc_diff(year, expected_delta_t)
      ));
    }

    std::println("{}", devider);
    std::println("");
  }

  // #64: assert the default dispatch against observations (this test used to only print).
  // 0.15 keeps margin above this table's 0.097 s maximum residual. The 2015-2026
  // rows share algo5's Bulletin A fit input and are not independent accuracy evidence.
  for (const auto& [year, expected_delta_t] : dataset::test::ACCURATE_DELTA_T_TABLE) {
    ASSERT_NEAR(compute(year), expected_delta_t, 0.15)
      << "default delta_t::compute drifted from observed ΔT at year " << year;
  }
}

TEST(DeltaT, Ut1TtConversion) {
  // Test the range between year 1900 and 2100.
  // Mainly focus on the correctness and consistency of bidirectional conversions.
  // #64: was capped at 2034, hiding the old 2035.0 dispatch seam. ΔT at 2100 is 86.9 s,
  // still under the 120-second bound below.
  for (uint32_t i = 0; i < 10000; i++) {
    const int32_t year = util::random(1900, 2100);
    const int32_t month = util::random(1, 12);
    const int32_t day = util::random(1, 28);

    const auto ymd = util::to_ymd(year, month, day);

    // Fraction range [0.1, 0.9], avoiding the beginning and end of the day.
    const double fraction = util::random(0.1, 0.9);

    { // UT1 -> TT -> UT1
      const calendar::Datetime ut1_dt { ymd, fraction };

      const calendar::Datetime tt_dt = ut1_to_tt(ut1_dt);
      const calendar::Datetime ut1_dt2 = tt_to_ut1(tt_dt);

      // The difference between UT1 and TT should be less than 120 seconds.
      ASSERT_EQ(tt_dt.ymd, ut1_dt.ymd);
      ASSERT_NEAR(tt_dt.fraction(), ut1_dt.fraction(), 120.0 / 86400.0); // Delta T expected to be < 120 seconds.

      // The restored UT1 should be the same as the original UT1.
      ASSERT_EQ(ut1_dt2.ymd, ut1_dt.ymd);
      ASSERT_NEAR(ut1_dt2.fraction(), ut1_dt.fraction(), 1e-10);
    }

    { // TT -> UT1 -> TT
      const calendar::Datetime tt_dt { ymd, fraction };

      const calendar::Datetime ut1_dt = tt_to_ut1(tt_dt);
      const calendar::Datetime tt_dt2 = ut1_to_tt(ut1_dt);

      // The difference between TT and UT1 should be less than 120 seconds.
      ASSERT_EQ(tt_dt.ymd, ut1_dt.ymd);
      ASSERT_NEAR(tt_dt.fraction(), ut1_dt.fraction(), 120.0 / 86400.0); // Delta T expected to be < 120 seconds.

      // The restored TT should be the same as the original TT.
      ASSERT_EQ(tt_dt2.ymd, tt_dt.ymd);
      ASSERT_NEAR(tt_dt2.fraction(), tt_dt.fraction(), 1e-10);
    }
  }

  // Test range [0, 1900].
  for (uint32_t i = 0; i < 10000; i++) {
    using namespace util::ymd_operator;

    const int32_t year = util::random(0, 1900);
    const int32_t month = util::random(1, 12);
    const int32_t day = util::random(1, 28);

    const auto ymd = util::to_ymd(year, month, day);
    const double fraction = util::random(0.0, 0.9999);

    { // UT1 -> TT -> UT1
      const calendar::Datetime ut1_dt { ymd, fraction };
      const calendar::Datetime tt_dt = ut1_to_tt(ut1_dt);
      const calendar::Datetime ut1_dt2 = tt_to_ut1(tt_dt);

      // Estimate the delta T.
      const double year_fraction = (ut1_dt.ymd - util::to_ymd(year, 1, 1)) / 365.0;
      const double est_delta_t = compute(year + year_fraction);

      const double actual_days_diff = (tt_dt.ymd - ut1_dt.ymd)                // Integral part
                                    + (tt_dt.fraction() - ut1_dt.fraction()); // Fractional part
      
      using namespace std::chrono;
      const double actual_delta_t = actual_days_diff * calendar::in_a_day<seconds>();

      ASSERT_NEAR(actual_delta_t, est_delta_t, 0.1);

      // The restored UT1 should be the same as the original UT1.
      ASSERT_EQ(ut1_dt2.ymd, ut1_dt.ymd);
      ASSERT_NEAR(ut1_dt2.fraction(), ut1_dt.fraction(), 1e-5);
    }

    { // TT -> UT1 -> TT
      const calendar::Datetime tt_dt { ymd, fraction };
      const calendar::Datetime ut1_dt = tt_to_ut1(tt_dt);
      const calendar::Datetime tt_dt2 = ut1_to_tt(ut1_dt);

      // Estimate the delta T.
      const double year_fraction = (tt_dt.ymd - util::to_ymd(year, 1, 1)) / 365.0;
      const double est_delta_t = compute(year + year_fraction);

      const double actual_days_diff = (tt_dt.ymd - ut1_dt.ymd)                // Integral part
                                    + (tt_dt.fraction() - ut1_dt.fraction()); // Fractional part

      using namespace std::chrono;
      const double actual_delta_t = actual_days_diff * calendar::in_a_day<seconds>();

      ASSERT_NEAR(actual_delta_t, est_delta_t, 0.1);

      // The restored TT should be the same as the original TT.
      ASSERT_EQ(tt_dt2.ymd, tt_dt.ymd);
      ASSERT_NEAR(tt_dt2.fraction(), tt_dt.fraction(), 1e-5);
    }
  }

}

TEST(DeltaT, DefaultDatetimeUsesAlgo5AcrossChronoYears) {
  // #321: the default datetime overload equals Algo 5 at every representable year.
  for (const int32_t year : { -32767, -32766, -1, 0, 1900, 2000, 2024, 2026, 32766, 32767 }) {
    for (const auto ymd : std::array {
      util::to_ymd(year, 1, 1), util::to_ymd(year, 2, 28), util::to_ymd(year, 3, 1), util::to_ymd(year, 12, 31)
    }) {
      for (const int64_t elapsed_ns : { 0LL, 1LL, 43200'000'000'000LL, 86399'999'999'999LL }) {
        const calendar::Datetime dt { ymd, std::chrono::hh_mm_ss { std::chrono::nanoseconds { elapsed_ns } } };
        const auto past_days = (std::chrono::sys_days { ymd }
                               - std::chrono::sys_days { util::to_ymd(year, 1, 1) }).count();
        const double year_days = ymd.year().is_leap() ? 366.0 : 365.0;
        const double expected = algo5::compute(year + ((static_cast<double>(past_days) + dt.fraction()) / year_days));

        ASSERT_EQ(compute(dt), expected)
          << "at " << year << '-' << dt.month() << '-' << dt.day() << ", elapsed ns " << elapsed_ns;
        ASSERT_EQ(compute(dt), compute(Model::ALGO5, dt))
          << "at " << year << '-' << dt.month() << '-' << dt.day() << ", elapsed ns " << elapsed_ns;
      }
    }
  }

  for (const int32_t year : { 0, 2000, 2024 }) {
    const calendar::Datetime leap_day { util::to_ymd(year, 2, 29), 0.123456789 };
    ASSERT_EQ(compute(leap_day), algo5::compute(year + ((59.0 + leap_day.fraction()) / 366.0))) << "at year " << year;
    ASSERT_EQ(compute(leap_day), compute(Model::ALGO5, leap_day)) << "at year " << year;
  }
}

TEST(DeltaT, ConversionsAtChronoYearEdges) {
  const calendar::Datetime upper_early { util::to_ymd(32767, 1, 1), 0.5 };
  const calendar::Datetime lower_early { util::to_ymd(-32767, 1, 1), 0.5 };
  const calendar::Datetime upper_late { util::to_ymd(32767, 12, 31), 0.5 };
  const calendar::Datetime lower_late { util::to_ymd(-32767, 12, 31), 0.5 };

  for (const auto& dt : { upper_early, lower_early, lower_late }) {
    const auto tt = ut1_to_tt(dt);
    ASSERT_TRUE(tt.ok());
    ASSERT_EQ(tt, calendar::add_seconds(dt, compute(Model::ALGO5, dt)));
    EXPECT_EQ(tt.year(), dt == lower_late ? -32766 : dt.year());
  }
  for (const auto& dt : { upper_early, upper_late, lower_late }) {
    const auto ut1 = tt_to_ut1(dt);
    ASSERT_TRUE(ut1.ok());
    ASSERT_EQ(ut1, calendar::add_seconds(dt, -compute(Model::ALGO5, dt)));
    EXPECT_EQ(ut1.year(), dt == upper_early ? 32766 : dt.year());
  }
  ASSERT_GT(compute(upper_late), 0.0);
  ASSERT_GT(compute(lower_early), 0.0);
  try {
    std::ignore = ut1_to_tt(upper_late);
    FAIL() << "Expected a shift beyond the upper chrono year";
  } catch (const std::invalid_argument& error) {
    EXPECT_NE(std::string_view { error.what() }.find("beyond the representable years"), std::string_view::npos);
  }
  try {
    std::ignore = tt_to_ut1(lower_early);
    FAIL() << "Expected a shift beyond the lower chrono year";
  } catch (const std::invalid_argument& error) {
    EXPECT_NE(std::string_view { error.what() }.find("beyond the representable years"), std::string_view::npos);
  }
}

TEST(DeltaT, DefaultDatetimeRejectsMutatedInvalidFields) {
  const calendar::Datetime valid { util::to_ymd(2026, 1, 1), 0.5 };
  for (const auto ymd : std::array {
    util::to_ymd(2026, 2, 30), util::to_ymd(1900, 2, 29), util::to_ymd(2026, 0, 1),
    util::to_ymd(2026, 13, 1), util::to_ymd(2026, 1, 0), util::to_ymd(2026, 1, 32), util::to_ymd(-32768, 1, 1)
  }) {
    auto invalid = valid;
    invalid.ymd = ymd;
    ASSERT_FALSE(invalid.ok());
    ASSERT_THROW(std::ignore = compute(invalid), std::invalid_argument);
    ASSERT_THROW(std::ignore = compute(Model::ALGO5, invalid), std::invalid_argument);
    ASSERT_THROW(std::ignore = ut1_to_tt(invalid), std::invalid_argument);
    ASSERT_THROW(std::ignore = tt_to_ut1(invalid), std::invalid_argument);
  }

  for (const int64_t elapsed_ns : { -1LL, -43200'000'000'000LL, 86400'000'000'000LL, 86400'000'000'001LL }) {
    auto invalid = valid;
    invalid.time_of_day = std::chrono::hh_mm_ss { std::chrono::nanoseconds { elapsed_ns } };
    ASSERT_FALSE(invalid.ok());
    ASSERT_THROW(std::ignore = compute(invalid), std::invalid_argument);
    ASSERT_THROW(std::ignore = compute(Model::ALGO5, invalid), std::invalid_argument);
    ASSERT_THROW(std::ignore = ut1_to_tt(invalid), std::invalid_argument);
    ASSERT_THROW(std::ignore = tt_to_ut1(invalid), std::invalid_argument);
  }

  ASSERT_EQ(compute(valid), compute(Model::ALGO5, valid));
}

} // namespace astro::delta_t_test
