#include <gtest/gtest.h>
#include <random>
#include <vector>
#include "lunar/algo1.hpp"
#include "lunar/algo2.hpp"
#include "lunar/algo3.hpp"

namespace calendar::lunar::algo3::test {

using namespace calendar::lunar::common;

TEST(LunarAlgo3, Correctness) {
  const auto in_algo1_range = [](const int32_t year) -> bool {
    return algo1::START_YEAR <= year and year <= algo1::END_YEAR;
  };

  const auto expected_info = [&](const int32_t year) -> LunarYear {
    if (in_algo1_range(year)) {
      return algo1::calc_lunar_year(year);
    }
    return algo2::calc_lunar_year(year);
  };

  std::vector<int32_t> years = std::views::iota(algo3::START_YEAR, algo3::END_YEAR + 1)
                             | std::ranges::to<std::vector>();

  std::shuffle(years.begin(), years.end(), std::mt19937 { std::random_device()() });
  years.resize(32);

  for (const auto year : years) {
    const auto expected = expected_info(year);
    const auto actual = algo3::calc_lunar_year(year);

    ASSERT_EQ(expected.date_of_first_day, actual.date_of_first_day);
    ASSERT_EQ(expected.leap_month, actual.leap_month);
    ASSERT_EQ(expected.month_lengths, actual.month_lengths);
  }
}

} // namespace calendar::lunar::algo3::test
