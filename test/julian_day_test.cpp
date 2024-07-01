#include <gtest/gtest.h>
#include <unordered_map>
#include "util.hpp"
#include "julian_day.hpp"

namespace calendar::julian_day {

using namespace util::datetime;

const std::unordered_map<double, DateTime> jd_test_data {
  { 
    2299160.5, 
    util::datetime::DateTime {
      util::to_ymd(1582, 10, 15),
      0.0,
    }
  },
  {
    2451544.5,
    util::datetime::DateTime {
      util::to_ymd(2000, 1, 1),
      0.0,
    }
  },
  {
    2443259.9,
    util::datetime::DateTime {
      util::to_ymd(1977, 4, 26),
      0.4,
    }
  },
  {
    2450084.0,
    util::datetime::DateTime {
      util::to_ymd(1996, 1, 1),
      0.5,
    }
  },
  {
    2456293.520833,
    util::datetime::DateTime {
      util::to_ymd(2013, 1, 1),
      hh_mm_ss { minutes { 30 } },
    }
  },
  {
    2460491.1846759,
    util::datetime::DateTime {
      util::to_ymd(2024, 6, 29),
      hh_mm_ss<seconds> { hours { 16 } + minutes { 25 } + seconds { 56 } },
    }
  },
  {
    2451545.041666667,
    util::datetime::DateTime {
      util::to_ymd(2000, 1, 1),
      hh_mm_ss { hours { 13 } },
    }
  },
  {
    2500000.0,
    util::datetime::DateTime {
      util::to_ymd(2132, 8, 31),
      0.5,
    }
  },
  {
    2305993.3852315,
    util::datetime::DateTime {
      util::to_ymd(1601, 6, 29),
      hh_mm_ss<seconds> { hours { 21 } + minutes { 14 } + seconds { 44 } },
    }
  },
  { // From http://www.stevegs.com/utils/jd_calc/
    2458908.7084259,
    util::datetime::DateTime {
      util::to_ymd(2020, 2, 29),
      hh_mm_ss<seconds> { hours { 5 } + seconds { 8 } },
    }
  },
  { // From http://www.stevegs.com/utils/jd_calc/
    2461436.1508698,
    util::datetime::DateTime {
      util::to_ymd(2027, 1, 30),
      hh_mm_ss<milliseconds> { hours { 15 } + minutes { 37 } + seconds { 15 } + milliseconds { 150 } },
    }
  },
  { // From http://www.stevegs.com/utils/jd_calc/
    2473063.7966088,
    util::datetime::DateTime {
      util::to_ymd(2058, 12, 1),
      hh_mm_ss<seconds> { hours { 7 } + minutes { 7 } + seconds { 7 } },
    }
  },
  { // From http://www.stevegs.com/utils/jd_calc/
    2459048.7966177,
    util::datetime::DateTime {
      util::to_ymd(2020, 7, 18),
      hh_mm_ss<milliseconds> { hours { 7 } + minutes { 7 } + seconds { 7 } + milliseconds { 770 } },
    }
  }
};

constexpr double EPSILON = 1e-6;


TEST(JulianDay, test_to_jd) {
  for (const auto& [jd, dt] : jd_test_data) {
    ASSERT_NEAR(to_jd(dt), jd, EPSILON);
  }
}

TEST(JulianDay, test_from_jd) {
  for (const auto& [jd, expected_dt] : jd_test_data) {
    const auto dt = from_jd(jd);
    ASSERT_EQ(dt.ymd(), expected_dt.ymd());
    ASSERT_NEAR(dt.fraction_of_day(), expected_dt.fraction_of_day(), EPSILON);
  }
}

TEST(JulianDay, test_consistency) {  
  const auto random_ymd = [] -> year_month_day {
    using namespace util;
    const std::chrono::year_month_day _ymd = util::to_ymd(gen_random_value(500, 2100), 1, 1);
    const auto _random_days = gen_random_value(0, 365 * 10);
    return _ymd + _random_days;
  };

  const auto random_hms = [] -> hh_mm_ss<nanoseconds> {
    using namespace util;
    return hh_mm_ss { 
      nanoseconds { 
        gen_random_value<uint64_t>(0, in_a_day<nanoseconds>() - 1)
      },
    };
  };

  for (auto i = 0; i < 5000; ++i) {
    const auto ymd = random_ymd();
    const auto hms = random_hms();
    const auto dt = DateTime { ymd, hms };
    const double jd = to_jd(dt);
    const auto recovered_dt = from_jd(jd);

    ASSERT_EQ(dt.ymd(), recovered_dt.ymd());
    ASSERT_NEAR(dt.fraction_of_day(), recovered_dt.fraction_of_day(), EPSILON);
  }

  const auto random_jd = [] -> double {
    return util::gen_random_value(1903682.686921, 2488069.686921); // gregorian year 500 ~ 2100
  };

  for (auto i = 0; i < 5000; ++i) {
    const double jd = random_jd();
    const auto dt = from_jd(jd);

    const double recovered_jd = to_jd(dt);
    ASSERT_NEAR(recovered_jd, jd, EPSILON);

    const auto recovered_dt = from_jd(recovered_jd);
    ASSERT_EQ(dt.ymd(), recovered_dt.ymd());
    ASSERT_NEAR(dt.fraction_of_day(), recovered_dt.fraction_of_day(), EPSILON);
  }
}

}
