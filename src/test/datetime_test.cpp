#include <gtest/gtest.h>
#include <print>
#include <ranges>
#include "util.hpp"
#include "datetime.hpp"
#include "ymd.hpp"

namespace calendar::test {

TEST(Datetime, FromTimepoint) {
  const auto now = system_clock::now();

  { // Test now.
    const Datetime dt { now };
    const auto ymd = dt.ymd;
    const auto time_of_day = dt.time_of_day;
    const auto fraction = dt.fraction();

    std::println("Now is {}.\nymd {}, hh_mm_ss {}, fraction of day is {}", 
                  now, ymd, time_of_day, fraction);

    ASSERT_LT(time_of_day.hours().count(), 24);
    ASSERT_GE(fraction, 0.0);
    ASSERT_LT(fraction, 1.0);
  }

  { // Test template.
    {
      [[maybe_unused]] const Datetime dt { now };
    }
    {
      const sys_time<days> dur { floor<days>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
    {
      const sys_time<nanoseconds> dur { floor<nanoseconds>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
    {
      const sys_time<microseconds> dur { floor<microseconds>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
  }

  { // Test with random nanoseconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + nanoseconds { util::random<int64_t>() };
      const Datetime dt { tp };

      const auto time_of_day = dt.time_of_day;
      const auto fraction = dt.fraction();

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
      const Datetime dt { tp };

      ASSERT_EQ(dt.fraction(), to_fraction(ns));
    }
  }

  { // Test with random microseconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + std::chrono::microseconds { util::random<int32_t>() };
      const Datetime dt { tp };

      const auto time_of_day = dt.time_of_day;
      const auto fraction = dt.fraction();

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
      const Datetime dt { tp };
      ASSERT_EQ(dt.fraction(), to_fraction(us));
    }
  }

  { // Test with random seconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + seconds { util::random<int32_t>() };
      const Datetime dt { tp };

      const auto time_of_day = dt.time_of_day;
      const auto fraction = dt.fraction();

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
      const Datetime dt { tp };

      ASSERT_EQ(dt.fraction(), to_fraction(s));
    }
  }
}

TEST(Datetime, FromYmdHms) {
  const auto now = system_clock::now();

  { // Test now.
    const year_month_day ymd { floor<days>(now) };
    const hh_mm_ss<nanoseconds> hms { now - floor<days>(now) };

    const Datetime dt { ymd, hms };
    ASSERT_EQ(dt.ymd, ymd);
    ASSERT_EQ(dt.time_of_day.to_duration(), hms.to_duration());
  }

  { // Test template.
    {
      [[maybe_unused]] const Datetime dt { now };
    }
    {
      const sys_time<days> dur { floor<days>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
    {
      const sys_time<nanoseconds> dur { floor<nanoseconds>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
    {
      const sys_time<microseconds> dur { floor<microseconds>(now) };
      [[maybe_unused]] const Datetime dt { dur };
    }
  }

  { // Test with random nanoseconds.
    for (auto i = 0; i < 100; i++) {
      const sys_days random_day = floor<days>(system_clock::now()) + days { 
        util::random<int32_t>(-365 * 30, 365 * 30) 
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
        const Datetime dt { ymd, hms };
        ASSERT_EQ(dt.ymd, ymd);
        ASSERT_EQ(dt.time_of_day.to_duration(), hms.to_duration());
      }
    }
  }

  { // Test with random microseconds.
    for (auto i = 0; i < 100; i++) {
      const sys_days random_day = floor<days>(system_clock::now()) + days { 
        util::random<int32_t>(-365 * 30, 365 * 30) 
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
        const Datetime dt { ymd, hms };
        ASSERT_EQ(dt.ymd, ymd);
        ASSERT_EQ(dt.time_of_day.to_duration(), hms.to_duration());
      }
    }
  }
}

TEST(Datetime, FromFraction) {
  for (auto i = 0; i < 100; i++) {
    const sys_days random_day = floor<days>(system_clock::now()) + days { 
      util::random<int32_t>(-365 * 30, 365 * 30) 
    };
    const year_month_day ymd { random_day };

    const auto random_fraction_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
      return util::random<double>(0.0, 1.0 - 1e-8);
    });

    for (double fraction : random_fraction_views) {
      const Datetime dt { ymd, fraction };
      ASSERT_EQ(dt.ymd, ymd);
      ASSERT_NEAR(dt.fraction(), fraction, 1e-10);
    }
  }
}

TEST(Datetime, Consistency) {
  const auto now = system_clock::now();
  constexpr auto ns_per_year = 365 * in_a_day<nanoseconds>();

  const auto random_tp_views = std::views::iota(0, 10000) | std::views::transform([&](auto) {
    const auto signed_ns = static_cast<int64_t>(ns_per_year);
    return now + microseconds { 
      util::random<int64_t>(-20 * signed_ns, 20 * signed_ns) 
    };
  });

  for (auto tp : random_tp_views) {
    const Datetime dt { tp };

    const year_month_day ymd { floor<days>(tp) };
    const hh_mm_ss<nanoseconds> hms { tp - floor<days>(tp) };
    const double fraction = to_fraction(hms.to_duration());

    ASSERT_TRUE(dt.ok());
    ASSERT_EQ(dt.ymd, ymd);
    ASSERT_EQ(dt.time_of_day.to_duration(), hms.to_duration());
    ASSERT_NEAR(dt.fraction(), fraction, 1e-10);

    { // Test from ymd and hms.
      const Datetime dt2 { ymd, hms };

      ASSERT_TRUE(dt2.ok());
      ASSERT_EQ(dt2.ymd, ymd);
      ASSERT_EQ(dt2.time_of_day.to_duration(), hms.to_duration());
      ASSERT_NEAR(dt2.fraction(), fraction, 1e-10);
    }

    { // Test from ymd and fraction of day.
      const Datetime dt2 { ymd, fraction };

      ASSERT_TRUE(dt2.ok());
      ASSERT_EQ(dt2.ymd, ymd);

      const nanoseconds dt2_elapsed_ns = dt2.time_of_day.to_duration();
      ASSERT_NEAR(dt2_elapsed_ns.count(), hms.to_duration().count(), 10);

      ASSERT_NEAR(dt2.fraction(), fraction, 1e-10);
    }
  }
}

TEST(Datetime, EdgeCases) {
  { // Test time_point constructor.
    using namespace util::ymd_operator;

    const auto today_tp = floor<days>(system_clock::now());
    const year_month_day ymd { today_tp };

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { 0 } };
      const Datetime dt { today_tp + hms.to_duration() };
      ASSERT_EQ(dt.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { -1 } };
      const Datetime dt { today_tp + hms.to_duration() };
      ASSERT_EQ(dt.ymd, ymd - days { 1 });
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() - 1 } };
      const Datetime dt { today_tp + hms.to_duration() };
      ASSERT_EQ(dt.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() } };
      const Datetime dt { today_tp + hms.to_duration() };
      ASSERT_EQ(dt.ymd, ymd + days { 1 });
    }
  }

  { // Test ymd and fraction constructor.
    const auto today_tp = floor<days>(system_clock::now());
    
    {
      const Datetime dt { today_tp, 0.0 };
      ASSERT_EQ(dt.ymd, today_tp);
      ASSERT_EQ(dt.fraction(), 0.0); // Enforce strict equality here.
    }

    {
      const Datetime dt { today_tp, 1.0 - 1e-11 };
      ASSERT_EQ(dt.ymd, today_tp);
      ASSERT_NEAR(dt.fraction(), 1.0, 1e-10); // Enforce strict equality here.
    }

    {
      // Ensure the exceptions are thrown.
      ASSERT_THROW((Datetime { today_tp, 1.0 + 1e-11 }), 
                   std::invalid_argument);

      ASSERT_THROW((Datetime { today_tp, 1.0 }), 
                   std::invalid_argument);

      ASSERT_THROW((Datetime { today_tp, -1e-11 }), 
                   std::invalid_argument);
    }
  }

  { // Test ymd and hms constructor.
    const auto today_tp = floor<days>(system_clock::now());
    const year_month_day ymd { today_tp };

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { 0 } };
      const Datetime dt { today_tp, hms };
      ASSERT_EQ(dt.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { -1 } };
      ASSERT_THROW((Datetime { today_tp, hms }),
                   std::runtime_error);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() - 1 } };
      const Datetime dt { today_tp, hms };
      ASSERT_EQ(dt.ymd, ymd);
    }

    {
      const hh_mm_ss<nanoseconds> hms { nanoseconds { in_a_day<nanoseconds>() } };
      ASSERT_THROW((Datetime { today_tp, hms }),
                   std::runtime_error);
    }
  }
}

TEST(Datetime, OperatorsEqualNonequal) {
  const Datetime dt1 { util::to_ymd(2024, 1, 1), 0.0 };
  const Datetime dt2 { util::to_ymd(2024, 1, 1), 0.0 };
  const Datetime dt3 { util::to_ymd(2024, 1, 1), 0.5 };

  ASSERT_EQ(dt1, dt2);
  ASSERT_NE(dt1, dt3);
}

TEST(Datetime, OperatorsSpaceship) {
  const Datetime dt1 { util::to_ymd(2024, 1, 1), 0.0 };
  const Datetime dt2 { util::to_ymd(2024, 1, 1), 0.0 };
  const Datetime dt3 { util::to_ymd(2024, 1, 1), 0.5 };
  const Datetime dt4 { util::to_ymd(2024, 1, 2), 0.0 };

  // Equal
  ASSERT_EQ(dt1 <=> dt2, std::strong_ordering::equal);

  // Less than
  ASSERT_EQ(dt1 <=> dt3, std::strong_ordering::less);
  ASSERT_EQ(dt3 <=> dt4, std::strong_ordering::less);
  ASSERT_TRUE(dt1 < dt3);
  ASSERT_TRUE(dt3 < dt4);

  // Less than or equal
  ASSERT_EQ(dt1 <=> dt2, std::strong_ordering::equal);
  ASSERT_EQ(dt1 <=> dt3, std::strong_ordering::less);
  ASSERT_EQ(dt3 <=> dt4, std::strong_ordering::less);
  ASSERT_TRUE(dt1 <= dt2);
  ASSERT_TRUE(dt1 <= dt3);
  ASSERT_TRUE(dt3 <= dt4);

  // Greater than
  ASSERT_EQ(dt4 <=> dt3, std::strong_ordering::greater);
  ASSERT_EQ(dt3 <=> dt1, std::strong_ordering::greater);
  ASSERT_TRUE(dt4 > dt3);
  ASSERT_TRUE(dt3 > dt1);

  // Greater than or equal
  ASSERT_EQ(dt4 <=> dt3, std::strong_ordering::greater);
  ASSERT_EQ(dt3 <=> dt1, std::strong_ordering::greater);
  ASSERT_EQ(dt2 <=> dt1, std::strong_ordering::equal);
  ASSERT_TRUE(dt4 >= dt3);
  ASSERT_TRUE(dt3 >= dt1);
  ASSERT_TRUE(dt2 >= dt1);
}

} // namespace calendar::test
