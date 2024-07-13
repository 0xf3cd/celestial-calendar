#include <gtest/gtest.h>
#include <unordered_map>
#include "util.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"

namespace astro::julian_day {

using namespace util;
using namespace calendar;
using namespace std::chrono_literals;

const std::unordered_map<double, Datetime> jd_test_data {
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


TEST(JulianDay, test_ut1_to_jd) {
  for (const auto& [jd, ut1] : jd_test_data) {
    ASSERT_NEAR(ut1_to_jd(ut1), jd, EPSILON);
  }
}

TEST(JulianDay, test_jd_to_ut1) {
  for (const auto& [jd, expected_dt] : jd_test_data) {
    const auto ut1 = jd_to_ut1(jd);
    ASSERT_EQ(ut1.ymd, expected_dt.ymd);
    ASSERT_NEAR(ut1.fraction(), expected_dt.fraction(), EPSILON);
  }
}

TEST(JulianDay, test_consistency) {  
  const auto random_ymd = [] -> year_month_day {
    using namespace util;
    const std::chrono::year_month_day _ymd = to_ymd(random(500, 2100), 1, 1);
    const auto _random_days = random(0, 365 * 10);
    return _ymd + _random_days;
  };

  const auto random_hms = [] -> hh_mm_ss<nanoseconds> {
    using namespace util;
    const auto ns = nanoseconds { 
      random<uint64_t>(0, in_a_day<nanoseconds>() - 1)
    };
    return hh_mm_ss { ns };
  };

  for (auto i = 0; i < 5000; ++i) {
    const auto ymd = random_ymd();
    const auto hms = random_hms();
    const Datetime ut1 { ymd, hms };

    const double jd = ut1_to_jd(ut1);
    const auto recovered_ut1 = jd_to_ut1(jd);

    ASSERT_EQ(ut1.ymd, recovered_ut1.ymd);
    ASSERT_NEAR(ut1.fraction(), recovered_ut1.fraction(), EPSILON);
  }

  const auto random_jd = [] -> double {
    // Return a jd number falls in gregorian year 500-ish ~ 2100-ish.
    return util::random(1903682.686921, 2488069.686921);
  };

  for (auto i = 0; i < 5000; ++i) {
    const double jd = random_jd();
    const auto ut1 = jd_to_ut1(jd);

    const double recovered_jd = ut1_to_jd(ut1);
    ASSERT_NEAR(recovered_jd, jd, EPSILON);

    const auto recovered_ut1 = jd_to_ut1(recovered_jd);
    ASSERT_EQ(ut1.ymd, recovered_ut1.ymd);
    ASSERT_NEAR(ut1.fraction(), recovered_ut1.fraction(), EPSILON);
  }
}

TEST(JulianDay, test_mjd) {
  ASSERT_EQ(jd_to_mjd(J2000), 51544.5);
  ASSERT_EQ(mjd_to_jd(51544.5), J2000);

  ASSERT_EQ(jd_to_mjd(MJD0), 0.0);
  ASSERT_EQ(mjd_to_jd(0.0), MJD0);
}

TEST(JulianDay, test_jm) {
  ASSERT_EQ(jd_to_jm(J2000), 0.0);
  ASSERT_EQ(jm_to_jd(0.0), J2000);

  ASSERT_NEAR(jd_to_jm(J2000 + 365250.0), 1.0, EPSILON);
  ASSERT_NEAR(jm_to_jd(1.0), J2000 + 365250.0, EPSILON);

  for (auto i = 0; i < 100; ++i) {
    const double random_jd = util::random(1903682.686921, 2488069.686921);

    const double jm = jd_to_jm(random_jd);
    ASSERT_NEAR(jm_to_jd(jm), random_jd, EPSILON);

    const double jd = jm_to_jd(jm);
    ASSERT_NEAR(jd_to_jm(jd), jm, EPSILON);
  }
}

TEST(JulianDay, test_jc) {
  ASSERT_EQ(jd_to_jc(J2000), 0.0);
  ASSERT_EQ(jc_to_jd(0.0), J2000);

  ASSERT_NEAR(jd_to_jc(J2000 + 36525.0), 1.0, EPSILON);
  ASSERT_NEAR(jc_to_jd(1.0), J2000 + 36525.0, EPSILON);

  for (auto i = 0; i < 100; ++i) {
    const double random_jd = util::random(1903682.686921, 2488069.686921);

    const double jc = jd_to_jc(random_jd);
    ASSERT_NEAR(jc_to_jd(jc), random_jd, EPSILON);

    const double jd = jc_to_jd(jc);
    ASSERT_NEAR(jd_to_jc(jd), jc, EPSILON);
  }
}

}
