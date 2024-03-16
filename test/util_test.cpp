#include <gtest/gtest.h>
#include "util.hpp"

namespace util {

TEST(FormatterTest, test_format) {
  ASSERT_EQ(format(""), "");
  ASSERT_EQ(format("%d", 42), "42");
  ASSERT_EQ(format("%d", 42), std::string { "42" });
  ASSERT_EQ(format("This is my msg: %d\n", 42), "This is my msg: 42\n");
}

TEST(DateTest, test_to_date) {
  using namespace std::literals;

  ASSERT_EQ(to_date(1901, 1, 1), 1901y / 1 / 1);
  ASSERT_EQ(to_date(2024, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_date(2024.0f, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_date(2024u, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_date(2024ll, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_date(2024llu, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_date(static_cast<int32_t>(2024), 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_date(static_cast<int16_t>(2024), 3, 15), 2024y / 3 / 15);
}

TEST(DateTest, test_operator_add) {
  using namespace std::literals;
  constexpr auto date = to_date(1901, 1, 1);

  ASSERT_EQ(date + std::chrono::days { -365 }, 1900y / 1 / 1);
  ASSERT_EQ(date + std::chrono::days { -1 }, 1900y / 12 / 31);
  ASSERT_EQ(date + std::chrono::days { 0 }, 1901y / 1 / 1);
  ASSERT_EQ(date + std::chrono::days { 1 }, 1901y / 1 / 2);
  ASSERT_EQ(date + std::chrono::days { 365 }, 1902y / 1 / 1);

  ASSERT_EQ(date + (-365), 1900y / 1 / 1);
  ASSERT_EQ(date + (-1), 1900y / 12 / 31);
  ASSERT_EQ(date + 0, 1901y / 1 / 1);
  ASSERT_EQ(date + 1, 1901y / 1 / 2);
  ASSERT_EQ(date + 365, 1902y / 1 / 1);

  ASSERT_EQ(-365 + date, 1900y / 1 / 1);
  ASSERT_EQ(-1 + date, 1900y / 12 / 31);
  ASSERT_EQ(0 + date, 1901y / 1 / 1);
  ASSERT_EQ(1 + date, 1901y / 1 / 2);
  ASSERT_EQ(365 + date, 1902y / 1 / 1);
}

TEST(DateTest, test_operator_sub) {
  using namespace std::literals;
  constexpr auto date = to_date(1901, 1, 1);

  ASSERT_EQ(date - std::chrono::days { 365 }, 1900y / 1 / 1);
  ASSERT_EQ(date - std::chrono::days { 1 }, 1900y / 12 / 31);
  ASSERT_EQ(date - std::chrono::days { 0 }, 1901y / 1 / 1);
  ASSERT_EQ(date - std::chrono::days { -1 }, 1901y / 1 / 2);
  ASSERT_EQ(date - std::chrono::days { -365 }, 1902y / 1 / 1);

  ASSERT_EQ(date - 365, 1900y / 1 / 1);
  ASSERT_EQ(date - 1, 1900y / 12 / 31);
  ASSERT_EQ(date - 0, 1901y / 1 / 1);
  ASSERT_EQ(date - (-1), 1901y / 1 / 2);
  ASSERT_EQ(date - (-365), 1902y / 1 / 1);
}

}
