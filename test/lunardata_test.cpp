#include <gtest/gtest.h>
#include <vector>
#include "lunardata.hpp"
#include "random.hpp"

namespace calendar::lunardata::test {

using namespace calendar::lunardata; 

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
    std::vector<uint32_t> { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 }
  ));

  info = get_lunar_year_info(1903);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 1903 } / 1 / 29);
  EXPECT_EQ(info.leap_month, 5);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 29, 30, 29, 30, 29, 29, 30, 29, 29, 30, 30, 29, 30 }
  ));

  info = get_lunar_year_info(2099);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 2099 } / 1 / 21);
  EXPECT_EQ(info.leap_month, 2);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 30, 30, 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30 }
  ));
}

TEST(LunarData, test_copy) {
  using namespace util::ymd_operator;

  for (auto _ = 0; _ < 100; ++_) {
    auto info = get_lunar_year_info(util::random(START_YEAR, END_YEAR));
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

TEST(LunarData, test_cache_performance) {
  const uint64_t elapsed_time_uncached = std::invoke([] {
    const auto start_time = std::chrono::steady_clock::now();
    for (auto _ = 0; _ < 800; ++_) {
      for (auto year = START_YEAR; year <= END_YEAR; ++year) {
        get_lunar_year_info(year);
      }
    }
    for (auto year = START_YEAR; year <= END_YEAR; ++year) {
      for (auto _ = 0; _ < 800; ++_) {
        get_lunar_year_info(year);
      }
    }
    for (auto _ = 0; _ < 2000; ++_) {
      const uint32_t random_year = util::random(START_YEAR, END_YEAR);
      get_lunar_year_info(random_year);
    }
    const auto end_time = std::chrono::steady_clock::now();
    return static_cast<uint64_t>((end_time - start_time).count());
  });

  const uint64_t elapsed_time_cached = std::invoke([] {
    const auto start_time = std::chrono::steady_clock::now();
    for (auto _ = 0; _ < 800; ++_) {
      for (auto year = START_YEAR; year <= END_YEAR; ++year) {
        LUNARDATA_CACHE.get(year);
      }
    }
    for (auto year = START_YEAR; year <= END_YEAR; ++year) {
      for (auto _ = 0; _ < 800; ++_) {
        LUNARDATA_CACHE.get(year);
      }
    }
    for (auto _ = 0; _ < 2000; ++_) {
      const uint32_t random_year = util::random(START_YEAR, END_YEAR);
      LUNARDATA_CACHE.get(random_year);
    }
    const auto end_time = std::chrono::steady_clock::now();
    return static_cast<uint64_t>((end_time - start_time).count());
  });
  
  std::cout << "Elapsed time for uncached: " << elapsed_time_uncached << "ns" << std::endl;
  std::cout << "Elapsed time for cached:   " << elapsed_time_cached << "ns" << std::endl;
  std::cout << "Cached is " << static_cast<double>(elapsed_time_uncached) / elapsed_time_cached << "x faster" << std::endl;
  EXPECT_LT(elapsed_time_cached, elapsed_time_uncached);
}

TEST(LunarData, test_lunardata_cache_correctness) {
  using namespace util::ymd_operator;

  for (auto _ = 0; _ < 100; ++_) {
    const auto year = util::random(START_YEAR, END_YEAR);
    const auto info = get_lunar_year_info(year);
    const auto info2 = LUNARDATA_CACHE.get(year);
    EXPECT_EQ(info.date_of_first_day, info2.date_of_first_day);
    EXPECT_EQ(info.leap_month, info2.leap_month);
    EXPECT_EQ(info.month_lengths, info2.month_lengths);
  }

  for (auto year = START_YEAR; year <= END_YEAR; ++year) {
    std::vector<LunarYearInfo> results;
    for (auto _ = 0; _ < 32; ++_) {
      results.emplace_back(LUNARDATA_CACHE.get(year));
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

}
