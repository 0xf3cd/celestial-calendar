#include <gtest/gtest.h>
#include <print>
#include <ranges>
#include "util.hpp"
#include "datetime.hpp"

namespace calendar::datetime {


TEST(Datetime, utc_from_timepoint) {
  const auto now = system_clock::now();

  { // Test now.
    const UTC utc { now };
    const auto ymd = utc.ymd;
    const auto time_of_day = utc.time_of_day;
    const auto fraction = utc.fraction();

    std::println("Now is {}.\nymd {}, hh_mm_ss {}, fraction of day is {}", 
                  now, ymd, time_of_day, fraction);

    ASSERT_LT(time_of_day.hours().count(), 24);
    ASSERT_GE(fraction, 0.0);
    ASSERT_LT(fraction, 1.0);
  }

  { // Test template.
    {
      [[maybe_unused]] const UTC utc { now };
    }
    {
      const sys_time<days> dur { floor<days>(now) };
      [[maybe_unused]] const UTC utc { dur };
    }
    {
      const sys_time<nanoseconds> dur { floor<nanoseconds>(now) };
      [[maybe_unused]] const UTC utc { dur };
    }
    {
      const sys_time<microseconds> dur { floor<microseconds>(now) };
      [[maybe_unused]] const UTC utc { dur };
    }
  }

  { // Test with random nanoseconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + nanoseconds { util::random<int64_t>() };
      const UTC utc { tp };

      const auto time_of_day = utc.time_of_day;
      const auto fraction = utc.fraction();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_ns_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return nanoseconds { 
        util::random<uint64_t>(0, in_a_day<nanoseconds>() - 1)
      }; 
    });

    for (nanoseconds ns : random_ns_views) {
      const auto tp = floor<days>(now) + ns;
      const UTC utc { tp };

      ASSERT_EQ(utc.fraction(), to_fraction(ns));
    }
  }

  { // Test with random microseconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + std::chrono::microseconds { util::random<int32_t>() };
      const UTC utc { tp };

      const auto time_of_day = utc.time_of_day;
      const auto fraction = utc.fraction();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_us_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return microseconds { 
        util::random<uint64_t>(0, in_a_day<microseconds>() - 1)
      }; 
    });

    for (microseconds us : random_us_views) {
      const auto tp = floor<days>(now) + us;
      const UTC utc { tp };
      ASSERT_EQ(utc.fraction(), to_fraction(us));
    }
  }

  { // Test with random seconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + seconds { util::random<int32_t>() };
      const UTC utc { tp };

      const auto time_of_day = utc.time_of_day;
      const auto fraction = utc.fraction();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_s_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return seconds { 
        util::random<uint32_t>(0, in_a_day<seconds>() - 1)
      }; 
    });

    for (seconds s : random_s_views) {
      const auto tp = floor<days>(now) + s;
      const UTC utc { tp };

      ASSERT_EQ(utc.fraction(), to_fraction(s));
    }
  }
}

TEST(Datetime, utc_from_ymd_hms) {
  const auto now = system_clock::now();

  { // Test now.
    const year_month_day ymd { floor<days>(now) };
    const hh_mm_ss<nanoseconds> hms { now - floor<days>(now) };

    const UTC utc { ymd, hms };
    ASSERT_EQ(utc.ymd, ymd);
    ASSERT_EQ(utc.time_of_day.to_duration(), hms.to_duration());
  }

  { // Test template.
    {
      [[maybe_unused]] const UTC utc { now };
    }
    {
      const sys_time<days> dur { floor<days>(now) };
      [[maybe_unused]] const UTC utc { dur };
    }
    {
      const sys_time<nanoseconds> dur { floor<nanoseconds>(now) };
      [[maybe_unused]] const UTC utc { dur };
    }
    {
      const sys_time<microseconds> dur { floor<microseconds>(now) };
      [[maybe_unused]] const UTC utc { dur };
    }
  }

  { // Test with random nanoseconds.
    for (auto i = 0; i < 100; i++) {
      const sys_days random_day = floor<days>(system_clock::now()) + days { 
        util::random<int64_t>(-365 * 30, 365 * 30) 
      };
      const year_month_day ymd { random_day };

      const auto random_hms_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
        return hh_mm_ss<nanoseconds> { 
          nanoseconds { 
            util::random<uint64_t>(0, in_a_day<nanoseconds>() - 1)
          } 
        };
      });

      for (hh_mm_ss<nanoseconds> hms : random_hms_views) {
        const UTC utc { ymd, hms };
        ASSERT_EQ(utc.ymd, ymd);
        ASSERT_EQ(utc.time_of_day.to_duration(), hms.to_duration());
      }
    }
  }

  { // Test with random microseconds.
    for (auto i = 0; i < 100; i++) {
      const sys_days random_day = floor<days>(system_clock::now()) + days { 
        util::random<int64_t>(-365 * 30, 365 * 30) 
      };
      const year_month_day ymd { random_day };

      const auto random_hms_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
        return hh_mm_ss<microseconds> { 
          microseconds { 
            util::random<uint64_t>(0, in_a_day<microseconds>() - 1)
          } 
        };
      });

      for (hh_mm_ss<microseconds> hms : random_hms_views) {
        const UTC utc { ymd, hms };
        ASSERT_EQ(utc.ymd, ymd);
        ASSERT_EQ(utc.time_of_day.to_duration(), hms.to_duration());
      }
    }
  }
}

TEST(Datetime, utc_from_fraction) {
  for (auto i = 0; i < 100; i++) {
    const sys_days random_day = floor<days>(system_clock::now()) + days { 
      util::random<int64_t>(-365 * 30, 365 * 30) 
    };
    const year_month_day ymd { random_day };

    const auto random_fraction_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
      return util::random<double>(0.0, 1.0 - 1e-8);
    });

    for (double fraction : random_fraction_views) {
      const UTC utc { ymd, fraction };
      ASSERT_EQ(utc.ymd, ymd);
      ASSERT_NEAR(utc.fraction(), fraction, 1e-10);
    }
  }
}

TEST(Datetime, utc_consistency) {
  const auto now = system_clock::now();
  constexpr auto ns_per_year = 365 * in_a_day<nanoseconds>();

  const auto random_tp_views = std::views::iota(0, 10000) | std::views::transform([&](auto) {
    return now + microseconds { 
      util::random<int64_t>(-20 * ns_per_year, 20 * ns_per_year) 
    };
  });

  for (auto tp : random_tp_views) {
    const UTC utc { tp };

    const year_month_day ymd { floor<days>(tp) };
    const hh_mm_ss<nanoseconds> hms { tp - floor<days>(tp) };
    const double fraction = to_fraction(hms.to_duration());

    ASSERT_TRUE(utc.ok());
    ASSERT_EQ(utc.ymd, ymd);
    ASSERT_EQ(utc.time_of_day.to_duration(), hms.to_duration());
    ASSERT_NEAR(utc.fraction(), fraction, 1e-10);

    { // Test from ymd and hms.
      const UTC dt2 { ymd, hms };

      ASSERT_TRUE(dt2.ok());
      ASSERT_EQ(dt2.ymd, ymd);
      ASSERT_EQ(dt2.time_of_day.to_duration(), hms.to_duration());
      ASSERT_NEAR(dt2.fraction(), fraction, 1e-10);
    }

    { // Test from ymd and fraction of day.
      const UTC dt2 { ymd, fraction };

      ASSERT_TRUE(dt2.ok());
      ASSERT_EQ(dt2.ymd, ymd);

      const nanoseconds dt2_elapsed_ns = dt2.time_of_day.to_duration();
      ASSERT_NEAR(dt2_elapsed_ns.count(), hms.to_duration().count(), 10);

      ASSERT_NEAR(dt2.fraction(), fraction, 1e-10);
    }
  }
}

TEST(Datetime, utc_edge_cases) {
  { // Test time_point constructor.
    using namespace util;

    const auto today_tp = floor<days>(system_clock::now());
    const year_month_day ymd { today_tp };

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { 0 } };
      const UTC utc { today_tp + hms.to_duration() };
      ASSERT_EQ(utc.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { -1 } };
      const UTC utc { today_tp + hms.to_duration() };
      ASSERT_EQ(utc.ymd, ymd - days { 1 });
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() - 1 } };
      const UTC utc { today_tp + hms.to_duration() };
      ASSERT_EQ(utc.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() } };
      const UTC utc { today_tp + hms.to_duration() };
      ASSERT_EQ(utc.ymd, ymd + days { 1 });
    }
  }

  { // Test ymd and fraction constructor.
    const auto today_tp = floor<days>(system_clock::now());
    
    {
      const UTC utc { today_tp, 0.0 };
      ASSERT_EQ(utc.ymd, today_tp);
      ASSERT_EQ(utc.fraction(), 0.0); // Enforce strict equality here.
    }

    {
      const UTC utc { today_tp, 1.0 - 1e-11 };
      ASSERT_EQ(utc.ymd, today_tp);
      ASSERT_NEAR(utc.fraction(), 1.0, 1e-10); // Enforce strict equality here.
    }

    {
      // Ensure the exceptions are thrown.
      ASSERT_THROW((UTC { today_tp, 1.0 + 1e-11 }), 
                   std::invalid_argument);

      ASSERT_THROW((UTC { today_tp, 1.0 }), 
                   std::invalid_argument);

      ASSERT_THROW((UTC { today_tp, -1e-11 }), 
                   std::invalid_argument);
    }
  }

  { // Test ymd and hms constructor.
    const auto today_tp = floor<days>(system_clock::now());
    const year_month_day ymd { today_tp };

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { 0 } };
      const UTC utc { today_tp, hms };
      ASSERT_EQ(utc.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { -1 } };
      ASSERT_THROW((UTC { today_tp, hms }),
                   std::runtime_error);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() - 1 } };
      const UTC utc { today_tp, hms };
      ASSERT_EQ(utc.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() } };
      ASSERT_THROW((UTC { today_tp, hms }),
                   std::runtime_error);
    }
  }
}

}
