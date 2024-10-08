#include <gtest/gtest.h>
#include <vector>
#include <print>
#include "ymd.hpp"
#include "random.hpp"
#include "lunar/algo1.hpp"

namespace calendar::lunar::algo1::test {

using namespace calendar::lunar::common;
using namespace calendar::lunar::algo1;

TEST(LunarAlgo1, ArraySize) {
  EXPECT_EQ(199, LUNAR_DATA.size());
  EXPECT_EQ(END_YEAR - START_YEAR + 1, LUNAR_DATA.size());
}

TEST(LunarAlgo1, LunarYear) {
  ASSERT_THROW(calc_lunar_year(START_YEAR - 1), std::out_of_range);
  ASSERT_THROW(calc_lunar_year(END_YEAR + 1), std::out_of_range);

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

  auto info = calc_lunar_year(1901);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 1901 } / 2 / 19);
  EXPECT_EQ(info.leap_month, 0);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 }
  ));

  info = calc_lunar_year(1903);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 1903 } / 1 / 29);
  EXPECT_EQ(info.leap_month, 5);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 29, 30, 29, 30, 29, 29, 30, 29, 29, 30, 30, 29, 30 }
  ));

  info = calc_lunar_year(2099);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 2099 } / 1 / 21);
  EXPECT_EQ(info.leap_month, 2);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 30, 30, 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30 }
  ));
}

TEST(LunarAlgo1, Copy) {
  using namespace util::ymd_operator;

  for (auto _ = 0; _ < 100; ++_) {
    auto info = calc_lunar_year(util::random(START_YEAR, END_YEAR));
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

TEST(LunarAlgo1, CachePerf) {
  const uint64_t elapsed_time_uncached = std::invoke([] {
    const auto start_time = std::chrono::steady_clock::now();
    for (auto _ = 0; _ < 800; ++_) {
      for (auto year = START_YEAR; year <= END_YEAR; ++year) {
        calc_lunar_year(year);
      }
    }
    for (auto year = START_YEAR; year <= END_YEAR; ++year) {
      for (auto _ = 0; _ < 800; ++_) {
        calc_lunar_year(year);
      }
    }
    for (auto _ = 0; _ < 2000; ++_) {
      const int32_t random_year = util::random(START_YEAR, END_YEAR);
      calc_lunar_year(random_year);
    }
    const auto end_time = std::chrono::steady_clock::now();
    return static_cast<uint64_t>((end_time - start_time).count());
  });

  const uint64_t elapsed_time_cached = std::invoke([] {
    const auto start_time = std::chrono::steady_clock::now();
    for (auto _ = 0; _ < 800; ++_) {
      for (auto year = START_YEAR; year <= END_YEAR; ++year) {
        [[maybe_unused]] const auto& result = get_info_for_year(year);
      }
    }
    for (auto year = START_YEAR; year <= END_YEAR; ++year) {
      for (auto _ = 0; _ < 800; ++_) {
        [[maybe_unused]] const auto& result = get_info_for_year(year);
      }
    }
    for (auto _ = 0; _ < 2000; ++_) {
      const int32_t random_year = util::random(START_YEAR, END_YEAR);
      [[maybe_unused]] const auto& result = get_info_for_year(random_year);
    }
    const auto end_time = std::chrono::steady_clock::now();
    return static_cast<uint64_t>((end_time - start_time).count());
  });
  
  std::println("Elapsed time for uncached: {}ns", elapsed_time_uncached);
  std::println("Elapsed time for cached:   {}ns", elapsed_time_cached);
  std::println("Cached is {}x faster", static_cast<double>(elapsed_time_uncached) / static_cast<double>(elapsed_time_cached));

  EXPECT_LT(elapsed_time_cached, elapsed_time_uncached);
}

TEST(LunarAlgo1, CacheCorrectness) {
  using namespace util::ymd_operator;

  for (auto _ = 0; _ < 100; ++_) {
    const auto year = util::random(START_YEAR, END_YEAR);
    const auto info = calc_lunar_year(year);
    const auto& info2 = get_info_for_year(year);
    EXPECT_EQ(info.date_of_first_day, info2.date_of_first_day);
    EXPECT_EQ(info.leap_month, info2.leap_month);
    EXPECT_EQ(info.month_lengths, info2.month_lengths);
  }

  for (auto year = START_YEAR; year <= END_YEAR; ++year) {
    std::vector<LunarYear> results;
    for (auto _ = 0; _ < 32; ++_) {
      results.emplace_back(get_info_for_year(year)); // NOLINT(performance-inefficient-vector-operation)
    }

    for (const auto& info1 : results) { // TODO: Use `std::views::cartesian_product` when supported.
      for (const auto& info2 : results) {
        EXPECT_EQ(info1.date_of_first_day, info2.date_of_first_day);
        EXPECT_EQ(info1.leap_month, info2.leap_month);
        EXPECT_EQ(info1.month_lengths, info2.month_lengths);
      }
    }
  }
}

} // namespace calendar::lunar::algo1::test
