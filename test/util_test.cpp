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
    const auto randomValue = gen_random_value<double>();
    ASSERT_GE(randomValue, std::numeric_limits<double>::min());
    ASSERT_LE(randomValue, std::numeric_limits<double>::max());
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto randomValue = gen_random_value<uint8_t>();
    ASSERT_GE(randomValue, std::numeric_limits<uint8_t>::min());
    ASSERT_LE(randomValue, std::numeric_limits<uint8_t>::max());
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto randomValue = gen_random_value<int16_t>();
    ASSERT_GE(randomValue, std::numeric_limits<int16_t>::min());
    ASSERT_LE(randomValue, std::numeric_limits<int16_t>::max());
  }
}


TEST(Util, test_gen_random_value2) {
  for (size_t i = 0; i < 5000; i++) {
    // Not sure if the subtest of float is 100% correct.
    // Maybe an epsilon is needed when comparing?
    const auto randomValue1 = gen_random_value<float>();
    const auto randomValue2 = gen_random_value<float>();
    if (randomValue1 == randomValue2) {
      continue;
    }
    
    const auto randomValue3 = gen_random_value<float>(
      std::min(randomValue1, randomValue2), 
      std::max(randomValue1, randomValue2)
    );
    ASSERT_GE(randomValue3, std::min(randomValue1, randomValue2));
    ASSERT_LT(randomValue3, std::max(randomValue1, randomValue2));
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto randomValue1 = gen_random_value<uint64_t>();
    const auto randomValue2 = gen_random_value<uint64_t>();
    if (randomValue1 == randomValue2) {
      continue;
    }

    const auto randomValue3 = gen_random_value<uint64_t>(
      std::min(randomValue1, randomValue2), 
      std::max(randomValue1, randomValue2)
    );
    ASSERT_GE(randomValue3, std::min(randomValue1, randomValue2));
    ASSERT_LT(randomValue3, std::max(randomValue1, randomValue2));
  }

  for (size_t i = 0; i < 100; i++) {
    const uint16_t gap = gen_random_value<uint16_t>(1, 20);
    const auto randomValue1 = std::invoke([&] {
      while (true) {
        const auto randomValue = gen_random_value<uint16_t>();
        if (randomValue < std::numeric_limits<uint16_t>::max() - gap) {
          return randomValue;
        }
      }
    });
    const auto randomValue2 = randomValue1 + gap;

    for (size_t j = 0; j < 100; j++) {
      const auto randomValue3 = gen_random_value<uint16_t>(randomValue1, randomValue2);
      ASSERT_GE(randomValue3, randomValue1);
      ASSERT_LE(randomValue3, randomValue2);
    }
  }
}

TEST(Util, test_datetime_from_time_point) {
  const auto now = std::chrono::system_clock::now();

  { // Test now.
    const DateTime dt { now };
    const auto ymd = dt.ymd();
    const auto time_of_day = dt.time_of_day();
    const auto fraction = dt.fraction_of_day();

    std::println("Now is {}.\nymd {}, hh_mm_ss {}, fraction of day is {}", 
                  now, ymd, time_of_day, fraction);

    ASSERT_LT(time_of_day.hours().count(), 24);
    ASSERT_GE(fraction, 0.0);
    ASSERT_LT(fraction, 1.0);
  }

  { // Test fraction with random nanoseconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + std::chrono::nanoseconds { gen_random_value<int64_t>() };
      const DateTime dt { tp };

      const auto ymd = dt.ymd();
      const auto time_of_day = dt.time_of_day();
      const auto fraction = dt.fraction_of_day();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_ns_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return std::chrono::nanoseconds { 
        gen_random_value<uint64_t>(0, 86400 * 1e9 - 1)
      }; 
    });

    for (std::chrono::nanoseconds ns : random_ns_views) {
      const auto tp = floor<days>(now) + ns;
      const DateTime dt { tp };

      const double expected_fraction = ns.count() / (86400.0 * 1e9);
      ASSERT_EQ(dt.fraction_of_day(), expected_fraction);
    }
  }

  { // Test fraction with random microseconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + std::chrono::microseconds { gen_random_value<int32_t>() };
      const DateTime dt { tp };

      const auto ymd = dt.ymd();
      const auto time_of_day = dt.time_of_day();
      const auto fraction = dt.fraction_of_day();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_us_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return std::chrono::microseconds { 
        gen_random_value<uint32_t>(0, 86400 * 1e6 - 1)
      }; 
    });

    for (std::chrono::microseconds us : random_us_views) {
      const auto tp = floor<days>(now) + us;
      const DateTime dt { tp };

      const double expected_fraction = us.count() / (86400.0 * 1e6);
      ASSERT_EQ(dt.fraction_of_day(), expected_fraction);
    }
  }

  { // Test fraction with random seconds.
    for (auto i = 0; i < 1000; i++) {
      const auto tp = now + std::chrono::seconds { gen_random_value<int32_t>() };
      const DateTime dt { tp };

      const auto ymd = dt.ymd();
      const auto time_of_day = dt.time_of_day();
      const auto fraction = dt.fraction_of_day();

      ASSERT_LT(time_of_day.hours().count(), 24);
      ASSERT_GE(fraction, 0.0);
      ASSERT_LT(fraction, 1.0);
    }

    const auto random_s_views = std::views::iota(0, 1000) | std::views::transform([](auto) { 
      return std::chrono::seconds { 
        gen_random_value<uint32_t>(0, 86400 - 1)
      }; 
    });

    for (std::chrono::seconds s : random_s_views) {
      const auto tp = floor<days>(now) + s;
      const DateTime dt { tp };

      const double expected_fraction = s.count() / 86400.0;
      ASSERT_EQ(dt.fraction_of_day(), expected_fraction);
    }
  }
}

}
