#include <gtest/gtest.h>
#include "lunardata.hpp"
#include "random.hpp"

namespace calendar::lunardata {

TEST(LunarData, test_array_size) {
  EXPECT_EQ(199, LUNAR_DATA.size());
  EXPECT_EQ(END_YEAR - START_YEAR + 1, LUNAR_DATA.size());
}

TEST(LunarData, test_get_lunar_year_info) {
  ASSERT_THROW(get_lunar_year_info(START_YEAR - 1), std::out_of_range);
  ASSERT_THROW(get_lunar_year_info(END_YEAR + 1), std::out_of_range);

  const auto check_month_lengths = [](const auto& l1, const auto& l2) -> bool {
    if (l1.size() != l2.size()) {
      return false;
    }
    for (size_t i = 0; i < l1.size(); ++i) {
      if (l1[i] != l2[i]) {
        return false;
      }
    }
    return true;
  };

  auto info = get_lunar_year_info(1901);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 1901 } / 2 / 19);
  EXPECT_EQ(info.leap_month, 0);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 }
  ));

  info = get_lunar_year_info(1903);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 1903 } / 1 / 29);
  EXPECT_EQ(info.leap_month, 5);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector { 29, 30, 29, 30, 29, 29, 30, 29, 29, 30, 30, 29, 30 }
  ));

  info = get_lunar_year_info(2099);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 2099 } / 1 / 21);
  EXPECT_EQ(info.leap_month, 2);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector { 30, 30, 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30 }
  ));
}

TEST(LunarData, test_copy) {
  using namespace util;

  for (auto _ = 0; _ < 100; ++_) {
    auto info = get_lunar_year_info(gen_random_value(START_YEAR, END_YEAR));
    auto info2 = info;

    EXPECT_NE(&info, &info2);
    EXPECT_NE(&info.month_lengths, &info2.month_lengths);

    EXPECT_EQ(info.date_of_first_day, info2.date_of_first_day);
    EXPECT_EQ(info.leap_month, info2.leap_month);
    EXPECT_EQ(info.month_lengths, info2.month_lengths);

    info.month_lengths.emplace_back(29);
    EXPECT_NE(info.month_lengths, info2.month_lengths);
  }
}

}
