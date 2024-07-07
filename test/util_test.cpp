#include <gtest/gtest.h>
#include <print>
#include <ranges>
#include "util.hpp"

namespace util {

TEST(Util, test_to_ymd) {
  using namespace std::literals;

  ASSERT_EQ(to_ymd(1901, 1, 1), 1901y / 1 / 1);
  ASSERT_EQ(to_ymd(2024, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024.0f, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024u, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024ll, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024llu, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(static_cast<int32_t>(2024), 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(static_cast<int16_t>(2024), 3, 15), 2024y / 3 / 15);
}

TEST(Util, test_from_ymd) {
  using namespace std::literals;

  {
    const auto [ y, m, d ] = from_ymd(1901y / 1 / 1);
    ASSERT_EQ(y, 1901);
    ASSERT_EQ(m, 1);
    ASSERT_EQ(d, 1); 
  }

  {
    const auto [ y, m, d ] = from_ymd(2024y / 3 / 15);
    ASSERT_EQ(y, 2024);
    ASSERT_EQ(m, 3);
    ASSERT_EQ(d, 15); 
  }

  {
    const auto [ y, m, d ] = from_ymd(0y / 3 / 15);
    ASSERT_EQ(y, 0);
    ASSERT_EQ(m, 3);
    ASSERT_EQ(d, 15); 
  }
}

TEST(Util, test_operator_add) {
  using namespace std::literals;
  constexpr auto ymd = to_ymd(1901, 1, 1);

  ASSERT_EQ(ymd + std::chrono::days { -365 }, 1900y / 1 / 1);
  ASSERT_EQ(ymd + std::chrono::days { -1 }, 1900y / 12 / 31);
  ASSERT_EQ(ymd + std::chrono::days { 0 }, 1901y / 1 / 1);
  ASSERT_EQ(ymd + std::chrono::days { 1 }, 1901y / 1 / 2);
  ASSERT_EQ(ymd + std::chrono::days { 365 }, 1902y / 1 / 1);

  ASSERT_EQ(ymd + (-365), 1900y / 1 / 1);
  ASSERT_EQ(ymd + (-1), 1900y / 12 / 31);
  ASSERT_EQ(ymd + 0, 1901y / 1 / 1);
  ASSERT_EQ(ymd + 1, 1901y / 1 / 2);
  ASSERT_EQ(ymd + 365, 1902y / 1 / 1);

  ASSERT_EQ(-365 + ymd, 1900y / 1 / 1);
  ASSERT_EQ(-1 + ymd, 1900y / 12 / 31);
  ASSERT_EQ(0 + ymd, 1901y / 1 / 1);
  ASSERT_EQ(1 + ymd, 1901y / 1 / 2);
  ASSERT_EQ(365 + ymd, 1902y / 1 / 1);
}

TEST(Util, test_operator_sub) {
  using namespace std::literals;
  constexpr auto ymd = to_ymd(1901, 1, 1);

  ASSERT_EQ(ymd - std::chrono::days { 365 }, 1900y / 1 / 1);
  ASSERT_EQ(ymd - std::chrono::days { 1 }, 1900y / 12 / 31);
  ASSERT_EQ(ymd - std::chrono::days { 0 }, 1901y / 1 / 1);
  ASSERT_EQ(ymd - std::chrono::days { -1 }, 1901y / 1 / 2);
  ASSERT_EQ(ymd - std::chrono::days { -365 }, 1902y / 1 / 1);

  ASSERT_EQ(ymd - 365, 1900y / 1 / 1);
  ASSERT_EQ(ymd - 1, 1900y / 12 / 31);
  ASSERT_EQ(ymd - 0, 1901y / 1 / 1);
  ASSERT_EQ(ymd - (-1), 1901y / 1 / 2);
  ASSERT_EQ(ymd - (-365), 1902y / 1 / 1);
}

TEST(Util, test_gen_random_value1) {
  for (size_t i = 0; i < 5000; i++) {
    const auto randomValue = random<double>();
    ASSERT_GE(randomValue, std::numeric_limits<double>::min());
    ASSERT_LE(randomValue, std::numeric_limits<double>::max());
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto randomValue = random<uint8_t>();
    ASSERT_GE(randomValue, std::numeric_limits<uint8_t>::min());
    ASSERT_LE(randomValue, std::numeric_limits<uint8_t>::max());
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto randomValue = random<int16_t>();
    ASSERT_GE(randomValue, std::numeric_limits<int16_t>::min());
    ASSERT_LE(randomValue, std::numeric_limits<int16_t>::max());
  }
}


TEST(Util, test_gen_random_value2) {
  for (size_t i = 0; i < 5000; i++) {
    // Not sure if the subtest of float is 100% correct.
    // Maybe an epsilon is needed when comparing?
    const auto randomValue1 = random<float>();
    const auto randomValue2 = random<float>();
    if (randomValue1 == randomValue2) {
      continue;
    }
    
    const auto randomValue3 = random<float>(
      std::min(randomValue1, randomValue2), 
      std::max(randomValue1, randomValue2)
    );
    ASSERT_GE(randomValue3, std::min(randomValue1, randomValue2));
    ASSERT_LT(randomValue3, std::max(randomValue1, randomValue2));
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto randomValue1 = random<uint64_t>();
    const auto randomValue2 = random<uint64_t>();
    if (randomValue1 == randomValue2) {
      continue;
    }

    const auto randomValue3 = random<uint64_t>(
      std::min(randomValue1, randomValue2), 
      std::max(randomValue1, randomValue2)
    );
    ASSERT_GE(randomValue3, std::min(randomValue1, randomValue2));
    ASSERT_LT(randomValue3, std::max(randomValue1, randomValue2));
  }

  for (size_t i = 0; i < 100; i++) {
    const uint16_t gap = random<uint16_t>(1, 20);
    const auto randomValue1 = std::invoke([&] {
      while (true) {
        const auto randomValue = random<uint16_t>();
        if (randomValue < std::numeric_limits<uint16_t>::max() - gap) {
          return randomValue;
        }
      }
    });
    const auto randomValue2 = randomValue1 + gap;

    for (size_t j = 0; j < 100; j++) {
      const auto randomValue3 = random<uint16_t>(randomValue1, randomValue2);
      ASSERT_GE(randomValue3, randomValue1);
      ASSERT_LE(randomValue3, randomValue2);
    }
  }
}

TEST(Util, utc_from_timepoint) {
  using namespace util::datetime;
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
      const auto tp = now + nanoseconds { random<int64_t>() };
      const UTC utc { tp };

      const auto time_of_day = utc.time_of_day;
      const auto fraction = utc.fraction();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_ns_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return nanoseconds { 
        random<uint64_t>(0, in_a_day<nanoseconds>() - 1)
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
      const auto tp = now + std::chrono::microseconds { random<int32_t>() };
      const UTC utc { tp };

      const auto time_of_day = utc.time_of_day;
      const auto fraction = utc.fraction();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_us_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return microseconds { 
        random<uint64_t>(0, in_a_day<microseconds>() - 1)
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
      const auto tp = now + seconds { random<int32_t>() };
      const UTC utc { tp };

      const auto time_of_day = utc.time_of_day;
      const auto fraction = utc.fraction();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_s_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return seconds { 
        random<uint32_t>(0, in_a_day<seconds>() - 1)
      }; 
    });

    for (seconds s : random_s_views) {
      const auto tp = floor<days>(now) + s;
      const UTC utc { tp };

      ASSERT_EQ(utc.fraction(), to_fraction(s));
    }
  }
}

TEST(Util, utc_from_ymd_hms) {
  using namespace util::datetime;
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
        random<int64_t>(-365 * 30, 365 * 30) 
      };
      const year_month_day ymd { random_day };

      const auto random_hms_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
        return hh_mm_ss<nanoseconds> { 
          nanoseconds { 
            random<uint64_t>(0, in_a_day<nanoseconds>() - 1)
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
        random<int64_t>(-365 * 30, 365 * 30) 
      };
      const year_month_day ymd { random_day };

      const auto random_hms_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
        return hh_mm_ss<microseconds> { 
          microseconds { 
            random<uint64_t>(0, in_a_day<microseconds>() - 1)
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

TEST(Util, utc_from_fraction) {
  using namespace util::datetime;

  for (auto i = 0; i < 100; i++) {
    const sys_days random_day = floor<days>(system_clock::now()) + days { 
      random<int64_t>(-365 * 30, 365 * 30) 
    };
    const year_month_day ymd { random_day };

    const auto random_fraction_views = std::views::iota(0, 1000) | std::views::transform([](auto) {
      return random<double>(0.0, 1.0 - 1e-8);
    });

    for (double fraction : random_fraction_views) {
      const UTC utc { ymd, fraction };
      ASSERT_EQ(utc.ymd, ymd);
      ASSERT_NEAR(utc.fraction(), fraction, 1e-10);
    }
  }
}

TEST(Util, utc_consistency) {
  using namespace util::datetime;

  const auto now = system_clock::now();
  constexpr auto ns_per_year = 365 * in_a_day<nanoseconds>();

  const auto random_tp_views = std::views::iota(0, 10000) | std::views::transform([&](auto) {
    return now + microseconds { 
      random<int64_t>(-20 * ns_per_year, 20 * ns_per_year) 
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

TEST(Util, utc_edge_cases) {
  using namespace util::datetime;

  { // Test time_point constructor.
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
