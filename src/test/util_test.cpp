#include <gtest/gtest.h>
#include <print>
#include <ranges>
#include "util.hpp"

namespace util::test {

using namespace util;

TEST(Util, ToYmd) {
  using namespace std::literals;

  ASSERT_EQ(to_ymd(1901, 1, 1), 1901y / 1 / 1);
  ASSERT_EQ(to_ymd(2024, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024.0F, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024U, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024LL, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024LLU, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(static_cast<int32_t>(2024), 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(static_cast<int16_t>(2024), 3, 15), 2024y / 3 / 15);
}

TEST(Util, FromYmd) {
  using namespace std::literals;

  {
    const auto [y, m, d] = from_ymd(1901y / 1 / 1);
    ASSERT_EQ(y, 1901);
    ASSERT_EQ(m, 1);
    ASSERT_EQ(d, 1); 
  }

  {
    const auto [y, m, d] = from_ymd(2024y / 3 / 15);
    ASSERT_EQ(y, 2024);
    ASSERT_EQ(m, 3);
    ASSERT_EQ(d, 15); 
  }

  {
    const auto [y, m, d] = from_ymd(0y / 3 / 15);
    ASSERT_EQ(y, 0);
    ASSERT_EQ(m, 3);
    ASSERT_EQ(d, 15); 
  }
}

TEST(Util, OperatorAdd) {
  using namespace std::literals;
  using namespace util::ymd_operator;
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

TEST(Util, OperatorSub) {
  using namespace std::literals;
  using namespace util::ymd_operator;
  
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

TEST(Util, GenRandomValue1) {
  for (size_t i = 0; i < 5000; i++) {
    const auto random_value = random<double>();
    ASSERT_GE(random_value, std::numeric_limits<double>::min());
    ASSERT_LE(random_value, std::numeric_limits<double>::max());
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto random_value = random<uint8_t>();
    ASSERT_GE(random_value, std::numeric_limits<uint8_t>::min());
    ASSERT_LE(random_value, std::numeric_limits<uint8_t>::max());
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto random_value = random<int16_t>();
    ASSERT_GE(random_value, std::numeric_limits<int16_t>::min());
    ASSERT_LE(random_value, std::numeric_limits<int16_t>::max());
  }
}


TEST(Util, GenRandomValue2) {
  for (size_t i = 0; i < 5000; i++) {
    // Not sure if the subtest of float is 100% correct.
    // Maybe an epsilon is needed when comparing?
    const auto random_value1 = random<float>();
    const auto random_value2 = random<float>();
    if (random_value1 == random_value2) {
      continue;
    }
    
    const auto random_value3 = random<float>(
      std::min(random_value1, random_value2), 
      std::max(random_value1, random_value2)
    );
    ASSERT_GE(random_value3, std::min(random_value1, random_value2));
    ASSERT_LE(random_value3, std::max(random_value1, random_value2));
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto random_value1 = random<uint64_t>();
    const auto random_value2 = random<uint64_t>();
    if (random_value1 == random_value2) {
      continue;
    }

    const auto random_value3 = random<uint64_t>(
      std::min(random_value1, random_value2), 
      std::max(random_value1, random_value2)
    );
    ASSERT_GE(random_value3, std::min(random_value1, random_value2));
    ASSERT_LT(random_value3, std::max(random_value1, random_value2));
  }

  for (size_t i = 0; i < 100; i++) {
    const auto gap = random<uint16_t>(1, 20);
    const auto random_value1 = std::invoke([&] {
      while (true) {
        const auto random_value = random<uint16_t>();
        if (random_value < std::numeric_limits<uint16_t>::max() - gap) {
          return random_value;
        }
      }
    });
    const auto random_value2 = random_value1 + gap;

    for (size_t j = 0; j < 100; j++) {
      const auto random_value3 = random<uint16_t>(random_value1, random_value2);
      ASSERT_GE(random_value3, random_value1);
      ASSERT_LE(random_value3, random_value2);
    }
  }
}

} // namespace util::test
