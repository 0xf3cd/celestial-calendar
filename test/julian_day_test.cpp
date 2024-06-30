#include <gtest/gtest.h>
#include "util.hpp"
#include "julian_day.hpp"

namespace calendar::julian_day {

constexpr double EPSILON = 0.000001;

TEST(JulianDay, test_to_jd_algo1) {
  {
    const auto jd = to_jd_algo1(util::to_ymd(1582, 10, 15));
    ASSERT_NEAR(jd, 2299160.5, EPSILON);
  }
  {
    // JD(2020-01-01 12:00:00) = 2451545.0,
    // so JD(2020-01-01 00:00:00) is expected to be = 2451544.5
    const auto jd = to_jd_algo1(util::to_ymd(2000, 1, 1));
    ASSERT_NEAR(jd, 2451544.5, EPSILON);
  }
}

TEST(JulianDay, test_from_jd_algo1) {
  const double jd = 2299160.5;
  const auto ymd = from_jd_algo1(jd);
  ASSERT_EQ(ymd, util::to_ymd(1582, 10, 15));
}

TEST(JulianDay, test_consistency_algo1) {  
  const auto random_ymd = [] -> std::chrono::year_month_day {
    using namespace util;
    const std::chrono::year_month_day _ymd = util::to_ymd(util::gen_random_value(500, 2100), 1, 1);
    const auto _random_days = gen_random_value(0, 365 * 10);
    return _ymd + _random_days;
  };

  for (auto i = 0; i < 2000; ++i) {
    const auto ymd = random_ymd();
    const double jd = to_jd_algo1(ymd);
    const auto recovered_ymd = from_jd_algo1(jd);

    ASSERT_EQ(recovered_ymd, ymd);
  }

  const auto random_jd = [] -> double {
    return util::gen_random_value(1903682.686921, 2488069.686921); // gregorian year 500 ~ 2100
  };

  for (auto i = 0; i < 2000; ++i) {
    const double jd = random_jd();
    const auto ymd = from_jd_algo1(jd);
    const double recovered_jd = to_jd_algo1(ymd);
    ASSERT_NEAR(recovered_jd, jd, 1.1);

    const auto recovered_ymd = from_jd_algo1(recovered_jd);
    ASSERT_EQ(recovered_ymd, ymd);
  }
}

}
