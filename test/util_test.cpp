#include <gtest/gtest.h>
#include "util.hpp"

namespace util {

TEST(Util, test_format) {
  ASSERT_EQ(format(""), "");
  ASSERT_EQ(format("%d", 42), "42");
  ASSERT_EQ(format("%d", 42), std::string { "42" });
  ASSERT_EQ(format("This is my msg: %d\n", 42), "This is my msg: 42\n");
}

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

TEST(Util, gen_random_value_test1) {
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


TEST(Util, gen_random_value_test2) {
  for (size_t i = 0; i < 5000; i++) {
    const auto randomValue1 = gen_random_value<float>();
    const auto randomValue2 = gen_random_value<float>();
    
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

}
