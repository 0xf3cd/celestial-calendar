#include <gtest/gtest.h>
#include <numeric>
#include "util.hpp"
#include "datetime.hpp"
#include "delta_t.hpp"
#include "delta_t_test_helper.hpp"

namespace astro::delta_t_test {

using namespace astro::delta_t;

TEST(DeltaT, Algo1) {
  ASSERT_THROW(algo1::compute(-4001), std::out_of_range);

  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo1::compute(500.0), 5710.0, 5.0);
  ASSERT_NEAR(algo1::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo1::compute(2008.0),  66.0, 0.1);
}

TEST(DeltaT, Algo2) {
  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo2::compute(500.0), 5710.0, 1.0);
  ASSERT_NEAR(algo2::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo2::compute(2008.0),  66.0, 0.15);
}

TEST(DeltaT, Algo3) {
  ASSERT_THROW(algo3::compute(3000.1), std::out_of_range);

  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo3::compute(500.0), 5710.0, 1.0);
  ASSERT_NEAR(algo3::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo3::compute(2008.0),  66.0, 0.5);
}

TEST(DeltaT, Algo4) {
  ASSERT_THROW(algo4::compute(2035.1), std::out_of_range);

  // Following data points are not very accurate.
  // Use them just to ensure the function is invokable.
  ASSERT_NEAR(algo4::compute(500.0), 5710.0, 1.0);
  ASSERT_NEAR(algo4::compute(1950.0),  29.0, 0.1);
  ASSERT_NEAR(algo4::compute(2008.0),  66.0, 0.6);
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
}

TEST(DeltaT, Ut1TtConversion) {
  // Test the range between year 1900 and 2034.
  // Mainly focus on the correctness and consistency of bidirectional conversions.
  for (uint32_t i = 0; i < 10000; i++) {
    const int32_t year = util::random(1900, 2034);
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

} // namespace astro::delta_t_test
