#include <gtest/gtest.h>
#include <algorithm>
#include "lunar/algo2.hpp"


namespace calendar::lunar::algo2::test {

using namespace calendar::lunar::algo2;
using namespace calendar::lunar::common;
using astro::julian_day::ut1_to_jde;
using astro::julian_day::jde_to_ut1;

TEST(LunarAlgo2, LunarMonthGenerator) {
  const double random_jde = astro::julian_day::J2000 + util::random(-365250.0, 365250.0);

  LunarMonthGenerator lunar_gen { random_jde };

  std::vector<LunarMonth> lunar_months;
  std::generate_n(std::back_inserter(lunar_months), 200, [&]() { return lunar_gen.next(); });

  // TODO: Use `std::views::pairwise` when supported.
  const auto lunar_month_pairs = std::views::zip(lunar_months, lunar_months | std::views::drop(1));

  std::vector<JieqiGenerator::JieqiPair> jieqi_pairs;
  for (const auto &[a, b] : lunar_month_pairs) {
    ASSERT_EQ(a.end_moment_utc8, b.start_moment_utc8);

    for (const auto jq_pair : a.contained_jieqis) {
      // Raw `.jde` is in UT1, add 8 hours to make it in UTC+8.
      const auto jq_moment_utc8 = jde_to_ut1(jq_pair.jde + 8.0 / 24.0);
      const auto jq_date = jq_moment_utc8.ymd;

      ASSERT_GE(jq_date, a.start_moment_utc8.ymd);
      ASSERT_LT(jq_date, a.end_moment_utc8.ymd);

      jieqi_pairs.push_back(jq_pair);
    }
  }

  // Ensure the Jieqis are in order.
  const auto jieqi_jdes = jieqi_pairs | std::views::transform([](const auto& jq) { return jq.jde; });
  ASSERT_TRUE(std::is_sorted(cbegin(jieqi_jdes), cend(jieqi_jdes)));

  // Ensure the first Jieqi in `jieqi_pairs` is reasonable.
  ASSERT_FALSE(jieqi_pairs.empty());
  const double first_jieqi_jde = jieqi_pairs[0].jde;
  ASSERT_GT(first_jieqi_jde, random_jde);
  ASSERT_LT(first_jieqi_jde, random_jde + 45.0);

  // Ensure the Jieqis are consecutive and correct.
  std::vector<JieqiGenerator::JieqiPair> expected_jieqi_pairs;
  JieqiGenerator jieqi_gen { random_jde };

  while (true) {
    const auto jq_pair = jieqi_gen.next();
    if (jq_pair.jde < jieqi_pairs.front().jde) {
      continue;
    }

    expected_jieqi_pairs.push_back(jq_pair);
    if (jq_pair.jde >= jieqi_pairs.back().jde) {
      break;
    }
  }

  ASSERT_EQ(jieqi_pairs, expected_jieqi_pairs);
}


TEST(LunarAlgo2, LunarMonthGeneratorPeek) {
  // Test that `peek` operations work correctly and does not modify the state of the generator.
  const double random_jde = astro::julian_day::J2000 + util::random(-365250.0, 365250.0);

  LunarMonthGenerator lunar_gen1 { random_jde };
  LunarMonthGenerator lunar_gen2 { random_jde };

  for (auto _ = 0; _ < 64; ++_) {
    std::vector<LunarMonth> month1_peek;
    std::vector<LunarMonth> month2_peek;

    for (auto _ = 0; _ < 3; ++_) {
      if (util::random(0.0, 1.0) < 0.42) {
        month1_peek.push_back(lunar_gen1.peek());
      }
      if (util::random(0.0, 1.0) < 0.42) {
        month2_peek.push_back(lunar_gen2.peek());
      }
    }

    const auto month1 = lunar_gen1.next();
    const auto month2 = lunar_gen2.next();
    ASSERT_EQ(month1, month2);

    for (const auto& m : month1_peek) {
      ASSERT_EQ(m, month1);
    }
    for (const auto& m : month2_peek) {
      ASSERT_EQ(m, month2);
    }
  }
}


TEST(LunarAlgo2, MonthChunks) {
  const int32_t random_year = util::random(1000, 2200);
  const auto& [chunk1, chunk2] = calendar::lunar::algo2::calc_lunar_month_chunks(random_year);

  { // Examine the first chunk.
    // It's length is either 12 or 13.
    ASSERT_TRUE(size(chunk1) == 12 or size(chunk1) == 13);
    // The first month's start moment should fall into previous year.
    const auto start_year = chunk1[0].start_moment_utc8.year();
    ASSERT_EQ(start_year, random_year - 1);
  }

  { // Examine the second chunk.
    // It's length is either 12 or 13.
    ASSERT_TRUE(size(chunk2) == 12 or size(chunk2) == 13);
    // The first month's start moment should fall into current year.
    const auto start_year = chunk2[0].start_moment_utc8.year();
    ASSERT_EQ(start_year, random_year);
  }
}


TEST(LunarAlgo2, LeapMonth) {
  for (auto _ = 0; _ < 8; ++_) {
    const int32_t random_year = util::random(500, 2500);
    const auto& [chunk1, chunk2] = calendar::lunar::algo2::calc_lunar_month_chunks(random_year);

    const auto is_leap = [](const LunarMonth& month) {
      const auto& jq_pairs = month.contained_jieqis;
      return not std::any_of(cbegin(jq_pairs), cend(jq_pairs), [](const auto& jq_pair) {
        return is_qi(jq_pair.jieqi);
      });
    };

    { // Examine the first chunk.
      const auto leap_month_opt = leap_month_in_chunk(chunk1);
      
      if (size(chunk1) == 12) {
        ASSERT_FALSE(leap_month_opt.has_value());
      } else {
        ASSERT_TRUE(leap_month_opt.has_value());
        // Ensure all previous months are not leap months.
        const auto leap_month = *leap_month_opt; // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_TRUE(std::all_of(cbegin(chunk1), cbegin(chunk1) + leap_month, [&](const auto& month) {
          return not is_leap(month);
        }));
        // Ensure the leap month is really leap.
        ASSERT_TRUE(is_leap(chunk1[leap_month]));
      }
    }

    { // Examine the second chunk.
      const auto leap_month_opt = leap_month_in_chunk(chunk2);

      if (size(chunk2) == 12) {
        ASSERT_FALSE(leap_month_opt.has_value());
      } else {
        ASSERT_TRUE(leap_month_opt.has_value());
        // Ensure all previous months are not leap months.
        const auto leap_month = *leap_month_opt; // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_TRUE(std::all_of(cbegin(chunk2), cbegin(chunk2) + leap_month, [&](const auto& month) {
          return not is_leap(month);
        }));
        // Ensure the leap month is really leap.
        ASSERT_TRUE(is_leap(chunk2[leap_month]));
      }
    }
  }
}


TEST(LunarAlgo2, LunarContext) {
  // Perform some random tests.
  for (auto _ = 0; _ < 8; ++_) {
    // Ensure the creation is successful, where some sanity checks are performed.
    const auto context = create_lunar_year_context(util::random(1000, 2200));

    // Also ensure the lunar year's length is reasonable.
    const double non_leap_year_len = 29.53 * 12;
    const double leap_year_len = 29.53 * 13;

    if (context.leap_month_moment_utc8.has_value()) {
      const double actual_len = ut1_to_jde(context.end_moment_utc8) 
                              - ut1_to_jde(context.start_moment_utc8);
      ASSERT_NEAR(leap_year_len, actual_len, 10.0);
      ASSERT_EQ(size(context.months), 13);
    } else {
      const double actual_len = ut1_to_jde(context.end_moment_utc8) 
                              - ut1_to_jde(context.start_moment_utc8);
      ASSERT_NEAR(non_leap_year_len, actual_len, 10.0);
      ASSERT_EQ(size(context.months), 12);
    }

    // Ensure the first month is the start of the year.
    ASSERT_EQ(context.months.front().start_moment_utc8, context.start_moment_utc8);

    // Ensure the last month ends at the end of the year.
    ASSERT_EQ(context.months.back().end_moment_utc8, context.end_moment_utc8);

    // Ensure the months are in order.
    // TODO: Use `std::views::pairwise` or `std::views::slide` when supported.
    const auto month_pairs = std::views::zip(context.months,context.months | std::views::drop(1));
    for (const auto& [a, b] : month_pairs) {
      ASSERT_LE(a.start_moment_utc8, b.start_moment_utc8);
      ASSERT_EQ(a.end_moment_utc8, b.start_moment_utc8);

      const double month_len = ut1_to_jde(a.end_moment_utc8) 
                             - ut1_to_jde(a.start_moment_utc8);
      ASSERT_NEAR(29.53, month_len, 0.75);
    }
  }

  // Checks for year 2024
  // Data source: https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2024.pdf
  {
    const auto context = create_lunar_year_context(2024);

    // No leap month in this year
    ASSERT_FALSE(context.leap_month_moment_utc8.has_value());

    // Check the start moment
    const double est_start_moment_utc8 = ut1_to_jde(calendar::Datetime { util::to_ymd(2024, 2, 10), 0.0 });
    const double actual_start_moment_utc8 = ut1_to_jde(context.start_moment_utc8);
    ASSERT_NEAR(est_start_moment_utc8, actual_start_moment_utc8, 1.0);

    // Check the end moment
    const double est_end_moment_utc8 = ut1_to_jde(calendar::Datetime { util::to_ymd(2025, 1, 29), 0.0 });
    const double actual_end_moment_utc8 = ut1_to_jde(context.end_moment_utc8);
    ASSERT_NEAR(est_end_moment_utc8, actual_end_moment_utc8, 1.0);
  }

  // Checks for year 2025
  // Data source: https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2025.pdf
  {
    const auto context = create_lunar_year_context(2025);

    // Leap month is the 7th month (index 6) in this year
    ASSERT_EQ(size(context.months), 13);
    ASSERT_TRUE(context.leap_month_moment_utc8.has_value());
    ASSERT_EQ(context.leap_month_moment_utc8.value(), context.months[6].start_moment_utc8); // NOLINT(bugprone-unchecked-optional-access)

    // Check the start moment
    const double est_start_moment_utc8 = ut1_to_jde(calendar::Datetime { util::to_ymd(2025, 1, 29), 0.0 });
    const double actual_start_moment_utc8 = ut1_to_jde(context.start_moment_utc8);
    ASSERT_NEAR(est_start_moment_utc8, actual_start_moment_utc8, 1.0);

    // Check the end moment
    const double est_end_moment_utc8 = ut1_to_jde(calendar::Datetime { util::to_ymd(2026, 2, 17), 0.0 });
    const double actual_end_moment_utc8 = ut1_to_jde(context.end_moment_utc8);
    ASSERT_NEAR(est_end_moment_utc8, actual_end_moment_utc8, 1.0);
  }
}

} // namespace calendar::lunar::algo2::test
