#include <gtest/gtest.h>
#include <unordered_map>
#include "util.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"

namespace calendar::julian_day {

using namespace util;
using namespace calendar::datetime;
using namespace std::chrono_literals;

const std::unordered_map<double, UTC> jd_test_data {
  { 2299160.5,         UTC { to_ymd(1582, 10, 15), 0.0, } },
  { 2451544.5,         UTC { to_ymd(2000, 1, 1),   0.0, } },
  { 2443259.9,         UTC { to_ymd(1977, 4, 26),  0.4, } },
  { 2450084.0,         UTC { to_ymd(1996, 1, 1),   0.5, } },
  { 2456293.520833,    UTC { to_ymd(2013, 1, 1),   hh_mm_ss { 30min } } },
  { 2460491.1846759,   UTC { to_ymd(2024, 6, 29),  hh_mm_ss { 16h + 25min + 56s } } },
  { 2451545.041666667, UTC { to_ymd(2000, 1, 1),   hh_mm_ss { 13h } } },
  { 2500000.0,         UTC { to_ymd(2132, 8, 31),  0.5, } },
  { 2305993.3852315,   UTC { to_ymd(1601, 6, 29),  hh_mm_ss { 21h + 14min + 44s } } },

  // From http://www.stevegs.com/utils/jd_calc/
  { 2458908.7084259,   UTC { to_ymd(2020, 2, 29),  hh_mm_ss { 5h + 8s } } },
  { 2461436.1508698,   UTC { to_ymd(2027, 1, 30),  hh_mm_ss { 15h + 37min + 15s + 150ms } } },
  { 2473063.7966088,   UTC { to_ymd(2058, 12, 1),  hh_mm_ss { 7h + 7min + 7s } } },
  { 2459048.7966177,   UTC { to_ymd(2020, 7, 18),  hh_mm_ss { 7h + 7min + 7s + 770ms } } }
};

constexpr double EPSILON = 1e-6;


TEST(JulianDay, test_utc_to_jd) {
  for (const auto& [jd, utc] : jd_test_data) {
    ASSERT_NEAR(utc_to_jd(utc), jd, EPSILON);
  }
}

TEST(JulianDay, test_jd_to_utc) {
  for (const auto& [jd, expected_dt] : jd_test_data) {
    const auto utc = jd_to_utc(jd);
    ASSERT_EQ(utc.ymd, expected_dt.ymd);
    ASSERT_NEAR(utc.fraction(), expected_dt.fraction(), EPSILON);
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
    const UTC utc { ymd, hms };

    const double jd = utc_to_jd(utc);
    const auto recovered_utc = jd_to_utc(jd);

    ASSERT_EQ(utc.ymd, recovered_utc.ymd);
    ASSERT_NEAR(utc.fraction(), recovered_utc.fraction(), EPSILON);
  }

  const auto random_jd = [] -> double {
    // Return a jd number falls in gregorian year 500-ish ~ 2100-ish.
    return util::random(1903682.686921, 2488069.686921);
  };

  for (auto i = 0; i < 5000; ++i) {
    const double jd = random_jd();
    const auto utc = jd_to_utc(jd);

    const double recovered_jd = utc_to_jd(utc);
    ASSERT_NEAR(recovered_jd, jd, EPSILON);

    const auto recovered_utc = jd_to_utc(recovered_jd);
    ASSERT_EQ(utc.ymd, recovered_utc.ymd);
    ASSERT_NEAR(utc.fraction(), recovered_utc.fraction(), EPSILON);
  }
}

}
