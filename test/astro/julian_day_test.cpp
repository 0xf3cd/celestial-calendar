#include <gtest/gtest.h>
#include <unordered_map>
#include "util.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"

namespace astro::julian_day::test {

using namespace astro::julian_day;

using namespace util;
using namespace util::ymd_operator;
using namespace calendar;
using namespace std::chrono_literals;

const std::unordered_map<double, Datetime> jde_test_data {
  { 2299160.5,         Datetime { to_ymd(1582, 10, 15), 0.0, } },
  { 2451544.5,         Datetime { to_ymd(2000, 1, 1),   0.0, } },
  { 2443259.9,         Datetime { to_ymd(1977, 4, 26),  0.4, } },
  { 2450084.0,         Datetime { to_ymd(1996, 1, 1),   0.5, } },
  { 2456293.520833,    Datetime { to_ymd(2013, 1, 1),   hh_mm_ss { 30min } } },
  { 2460491.1846759,   Datetime { to_ymd(2024, 6, 29),  hh_mm_ss { 16h + 25min + 56s } } },
  { 2451545.041666667, Datetime { to_ymd(2000, 1, 1),   hh_mm_ss { 13h } } },
  { 2500000.0,         Datetime { to_ymd(2132, 8, 31),  0.5, } },
  { 2305993.3852315,   Datetime { to_ymd(1601, 6, 29),  hh_mm_ss { 21h + 14min + 44s } } },

  // From http://www.stevegs.com/utils/jd_calc/
  { 2458908.7084259,   Datetime { to_ymd(2020, 2, 29),  hh_mm_ss { 5h + 8s } } },
  { 2461436.1508698,   Datetime { to_ymd(2027, 1, 30),  hh_mm_ss { 15h + 37min + 15s + 150ms } } },
  { 2473063.7966088,   Datetime { to_ymd(2058, 12, 1),  hh_mm_ss { 7h + 7min + 7s } } },
  { 2459048.7966177,   Datetime { to_ymd(2020, 7, 18),  hh_mm_ss { 7h + 7min + 7s + 770ms } } }
};

constexpr double EPSILON = 1e-6;


TEST(JulianDay, test_tt_to_jde) {
  for (const auto& [jde, tt] : jde_test_data) {
    ASSERT_NEAR(tt_to_jde(tt), jde, EPSILON);
  }
}

TEST(JulianDay, test_jde_to_tt) {
  for (const auto& [jde, expected_dt] : jde_test_data) {
    const auto tt = jde_to_tt(jde);
    ASSERT_EQ(tt.ymd, expected_dt.ymd);
    ASSERT_NEAR(tt.fraction(), expected_dt.fraction(), EPSILON);
  }
}

TEST(JulianDay, test_consistency) {  
  const auto random_ymd = [] -> year_month_day {
    using namespace util::ymd_operator;
    const std::chrono::year_month_day _ymd = to_ymd(util::random(500, 2100), 1, 1);
    const auto _random_days = util::random(0, 365 * 10);
    return _ymd + _random_days;
  };

  const auto random_hms = [] -> hh_mm_ss<nanoseconds> {
    using namespace util::ymd_operator;
    const auto ns = nanoseconds { 
      util::random<uint64_t>(0, in_a_day<nanoseconds>() - 1)
    };
    return hh_mm_ss { ns };
  };

  for (auto i = 0; i < 5000; ++i) {
    const auto ymd = random_ymd();
    const auto hms = random_hms();
    const Datetime tt { ymd, hms };

    const double jde = tt_to_jde(tt);
    const auto recovered_tt = jde_to_tt(jde);

    ASSERT_EQ(tt.ymd, recovered_tt.ymd);
    ASSERT_NEAR(tt.fraction(), recovered_tt.fraction(), EPSILON);
  }

  const auto random_jde = [] -> double {
    // Return a jde number falls in gregorian year 500-ish ~ 2100-ish.
    return util::random(1903682.686921, 2488069.686921);
  };

  for (auto i = 0; i < 5000; ++i) {
    const double jde = random_jde();
    const auto tt = jde_to_tt(jde);

    const double recovered_jde = tt_to_jde(tt);
    ASSERT_NEAR(recovered_jde, jde, EPSILON);

    const auto recovered_tt = jde_to_tt(recovered_jde);
    ASSERT_EQ(tt.ymd, recovered_tt.ymd);
    ASSERT_NEAR(tt.fraction(), recovered_tt.fraction(), EPSILON);
  }
}

TEST(JulianDay, test_jm) {
  ASSERT_EQ(jde_to_jm(J2000), 0.0);
  ASSERT_EQ(jm_to_jde(0.0), J2000);

  ASSERT_NEAR(jde_to_jm(J2000 + 365250.0), 1.0, EPSILON);
  ASSERT_NEAR(jm_to_jde(1.0), J2000 + 365250.0, EPSILON);

  for (auto i = 0; i < 100; ++i) {
    const double random_jde = util::random(1903682.686921, 2488069.686921);

    const double jm = jde_to_jm(random_jde);
    ASSERT_NEAR(jm_to_jde(jm), random_jde, EPSILON);

    const double jde = jm_to_jde(jm);
    ASSERT_NEAR(jde_to_jm(jde), jm, EPSILON);
  }
}

TEST(JulianDay, test_jc) {
  ASSERT_EQ(jde_to_jc(J2000), 0.0);
  ASSERT_EQ(jc_to_jde(0.0), J2000);

  ASSERT_NEAR(jde_to_jc(J2000 + 36525.0), 1.0, EPSILON);
  ASSERT_NEAR(jc_to_jde(1.0), J2000 + 36525.0, EPSILON);

  for (auto i = 0; i < 100; ++i) {
    const double random_jde = util::random(1903682.686921, 2488069.686921);

    const double jc = jde_to_jc(random_jde);
    ASSERT_NEAR(jc_to_jde(jc), random_jde, EPSILON);

    const double jde = jc_to_jde(jc);
    ASSERT_NEAR(jde_to_jc(jde), jc, EPSILON);
  }
}

}
