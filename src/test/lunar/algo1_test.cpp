#include <gtest/gtest.h>
#include <vector>
#include <print>
#include "ymd.hpp"
#include "random.hpp"
#include "lunar/algo1.hpp"

namespace calendar::lunar::algo1::test {

using namespace calendar::lunar::algo1;
using namespace calendar::lunar::algo1::data; 

TEST(LunarData, ArraySize) {
  EXPECT_EQ(199, LUNAR_DATA.size());
  EXPECT_EQ(END_YEAR - START_YEAR + 1, LUNAR_DATA.size());
}

TEST(LunarData, LunarYearInfo) {
  ASSERT_THROW(parse_lunar_year_info(START_YEAR - 1), std::out_of_range);
  ASSERT_THROW(parse_lunar_year_info(END_YEAR + 1), std::out_of_range);

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

  auto info = parse_lunar_year_info(1901);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 1901 } / 2 / 19);
  EXPECT_EQ(info.leap_month, 0);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 29, 30, 29, 29, 30, 29, 30, 29, 30, 30, 30, 29 }
  ));

  info = parse_lunar_year_info(1903);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 1903 } / 1 / 29);
  EXPECT_EQ(info.leap_month, 5);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 29, 30, 29, 30, 29, 29, 30, 29, 29, 30, 30, 29, 30 }
  ));

  info = parse_lunar_year_info(2099);
  EXPECT_EQ(info.date_of_first_day, std::chrono::year { 2099 } / 1 / 21);
  EXPECT_EQ(info.leap_month, 2);
  EXPECT_TRUE(check_month_lengths(
    info.month_lengths, 
    std::vector<uint32_t> { 30, 30, 29, 30, 30, 29, 29, 30, 29, 29, 30, 29, 30 }
  ));
}

TEST(LunarData, Copy) {
  using namespace util::ymd_operator;

  for (auto _ = 0; _ < 100; ++_) {
    auto info = parse_lunar_year_info(util::random(START_YEAR, END_YEAR));
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

TEST(LunarData, CachePerf) {
  const uint64_t elapsed_time_uncached = std::invoke([] {
    const auto start_time = std::chrono::steady_clock::now();
    for (auto _ = 0; _ < 800; ++_) {
      for (auto year = START_YEAR; year <= END_YEAR; ++year) {
        parse_lunar_year_info(year);
      }
    }
    for (auto year = START_YEAR; year <= END_YEAR; ++year) {
      for (auto _ = 0; _ < 800; ++_) {
        parse_lunar_year_info(year);
      }
    }
    for (auto _ = 0; _ < 2000; ++_) {
      const uint32_t random_year = util::random(START_YEAR, END_YEAR);
      parse_lunar_year_info(random_year);
    }
    const auto end_time = std::chrono::steady_clock::now();
    return static_cast<uint64_t>((end_time - start_time).count());
  });

  const uint64_t elapsed_time_cached = std::invoke([] {
    const auto start_time = std::chrono::steady_clock::now();
    for (auto _ = 0; _ < 800; ++_) {
      for (auto year = START_YEAR; year <= END_YEAR; ++year) {
        [[maybe_unused]] const auto& result = get_lunar_year_info(year);
      }
    }
    for (auto year = START_YEAR; year <= END_YEAR; ++year) {
      for (auto _ = 0; _ < 800; ++_) {
        [[maybe_unused]] const auto& result = get_lunar_year_info(year);
      }
    }
    for (auto _ = 0; _ < 2000; ++_) {
      const int32_t random_year = util::random(START_YEAR, END_YEAR);
      [[maybe_unused]] const auto& result = get_lunar_year_info(random_year);
    }
    const auto end_time = std::chrono::steady_clock::now();
    return static_cast<uint64_t>((end_time - start_time).count());
  });
  
  std::println("Elapsed time for uncached: {}ns", elapsed_time_uncached);
  std::println("Elapsed time for cached:   {}ns", elapsed_time_cached);
  std::println("Cached is {}x faster", static_cast<double>(elapsed_time_uncached) / static_cast<double>(elapsed_time_cached));

  EXPECT_LT(elapsed_time_cached, elapsed_time_uncached);
}

TEST(LunarData, CacheCorrectness) {
  using namespace util::ymd_operator;

  for (auto _ = 0; _ < 100; ++_) {
    const auto year = util::random(START_YEAR, END_YEAR);
    const auto info = parse_lunar_year_info(year);
    const auto& info2 = get_lunar_year_info(year);
    EXPECT_EQ(info.date_of_first_day, info2.date_of_first_day);
    EXPECT_EQ(info.leap_month, info2.leap_month);
    EXPECT_EQ(info.month_lengths, info2.month_lengths);
  }

  for (auto year = START_YEAR; year <= END_YEAR; ++year) {
    std::vector<LunarYearInfo> results;
    for (auto _ = 0; _ < 32; ++_) {
      results.emplace_back(get_lunar_year_info(year)); // NOLINT(performance-inefficient-vector-operation)
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

TEST(LunarAlgo1, IsValidGregorian) {
  using namespace std::literals;

  ASSERT_FALSE(Converter::is_valid_gregorian(1901y / 2 / 18));
  ASSERT_TRUE(Converter::is_valid_gregorian(1901y / 2 / 19));
  ASSERT_TRUE(Converter::is_valid_gregorian(1901y / 2 / 20));

  ASSERT_TRUE(Converter::is_valid_gregorian(2024y / 3 / 17));

  ASSERT_TRUE(Converter::is_valid_gregorian(2099y / 1 / 21));
  ASSERT_TRUE(Converter::is_valid_gregorian(2100y / 2 / 8));
  ASSERT_FALSE(Converter::is_valid_gregorian(2100y / 2 / 9));
}

TEST(LunarAlgo1, IsValidLunar) {
  using namespace std::literals;

  ASSERT_FALSE(Converter::is_valid_lunar(1900y / 12 / 29));
  ASSERT_FALSE(Converter::is_valid_lunar(1901y / 1 / 0));
  ASSERT_TRUE(Converter::is_valid_lunar(1901y / 1 / 1));
  ASSERT_TRUE(Converter::is_valid_lunar(1901y / 1 / 2));

  ASSERT_TRUE(Converter::is_valid_lunar(2024y / 3 / 17));
  ASSERT_FALSE(Converter::is_valid_lunar(2024y / 0 / 0));
  ASSERT_FALSE(Converter::is_valid_lunar(2024y / 0 / 1));
  ASSERT_FALSE(Converter::is_valid_lunar(2024y / 0 / 28));
  ASSERT_FALSE(Converter::is_valid_lunar(2024y / 1 / 0));
  ASSERT_FALSE(Converter::is_valid_lunar(2024y / 12 / 0));
  ASSERT_FALSE(Converter::is_valid_lunar(2024y / 14 / 0));

  ASSERT_TRUE(Converter::is_valid_lunar(2099y / 1 / 1));
  ASSERT_TRUE(Converter::is_valid_lunar(2099y / 12 / 29));
  ASSERT_FALSE(Converter::is_valid_lunar(2099y / 12 / 30));
  ASSERT_TRUE(Converter::is_valid_lunar(2099y / 13 / 30));
  ASSERT_FALSE(Converter::is_valid_lunar(2099y / 14 / 0));
  ASSERT_FALSE(Converter::is_valid_lunar(2099y / 14 / 1));
  ASSERT_FALSE(Converter::is_valid_lunar(2100y / 1 / 1));
}

TEST(LunarAlgo1, GregorianToLunarNegative) {
  using namespace std::literals;

  ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2024y / 0 / 29));
  ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(1901y / 1 / 0));
  ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(1901y / 2 / 18));
  ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2099y / 12 / 32));
  ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2100y / 0 / 1));
  ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2100y / 0 / 1));
  ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(2100y / 2 / 9));

  ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(1901y / 2 / 19));
  ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(2100y / 2 / 8));
  ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(2024y / 3 / 18));
}

TEST(LunarAlgo1, LunarToGregorianNegative) {
  using namespace std::literals;

  ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2024y / 0 / 29));
  ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(1901y / 1 / 0));
  ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2099y / 12 / 30));
  ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2100y / 1 / 1));
  ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2100y / 14 / 1));

  ASSERT_NE(std::nullopt, Converter::lunar_to_gregorian(1901y / 1 / 1));
  ASSERT_NE(std::nullopt, Converter::lunar_to_gregorian(2099y / 13 / 30));
}

TEST(LunarAlgo1, GregorianToLunar) {
  using namespace util::ymd_operator;

  for (auto year = START_YEAR; year <= END_YEAR; ++year) {
    const auto& info = get_lunar_year_info(year);
    ASSERT_EQ(util::to_ymd(year, 1, 1), Converter::gregorian_to_lunar(info.date_of_first_day));

    uint32_t days_count = 0;
    const auto& ml = info.month_lengths;
    for (uint32_t month_idx = 0; month_idx < ml.size(); ++month_idx) { // Iterate over all months.
      for (uint32_t day = 1; day <= ml[month_idx]; ++day) { // Iterate over all days in the month.
        const auto lunar_date = util::to_ymd(year, month_idx + 1, day);
        ASSERT_EQ(lunar_date, Converter::gregorian_to_lunar(info.date_of_first_day + days_count));
        days_count++;
      }
    }
  }
}

TEST(LunarAlgo1, LunarToGregorian) {
  using namespace util::ymd_operator;

  for (auto year = START_YEAR; year <= END_YEAR; ++year) {
    const auto& info = get_lunar_year_info(year);
    ASSERT_EQ(info.date_of_first_day, Converter::lunar_to_gregorian(util::to_ymd(year, 1, 1)));

    uint32_t days_count = 0;
    const auto& ml = info.month_lengths;
    for (uint32_t month_idx = 0; month_idx < ml.size(); ++month_idx) { // Iterate over all months.
      for (uint32_t day = 1; day <= ml[month_idx]; ++day) { // Iterate over all days in the month.
        const auto lunar_date = util::to_ymd(year, month_idx + 1, day);
        ASSERT_EQ(info.date_of_first_day + days_count, Converter::lunar_to_gregorian(lunar_date));
        days_count++;
      }
    }
  }
}

TEST(LunarAlgo1, Integration) {
  using namespace util::ymd_operator;
  using std::chrono::sys_days;
  using std::chrono::year_month_day;

  const uint32_t diffrence = (sys_days(LAST_GREGORIAN_DATE) - sys_days(FIRST_GREGORIAN_DATE)).count();
  for (auto _ = 0; _ < 5000; ++_) {
    const year_month_day solar_date = FIRST_GREGORIAN_DATE + util::random<uint32_t>(0, diffrence);
    ASSERT_TRUE(Converter::is_valid_gregorian(solar_date));

    const std::optional<year_month_day> optional_lunar_date = Converter::gregorian_to_lunar(solar_date);
    ASSERT_TRUE(optional_lunar_date.has_value());
    
    const year_month_day lunar_date = optional_lunar_date.value(); // NOLINT(bugprone-unchecked-optional-access)
    ASSERT_TRUE(Converter::is_valid_lunar(lunar_date));

    ASSERT_EQ(solar_date, Converter::lunar_to_gregorian(lunar_date));
    ASSERT_EQ(lunar_date, Converter::gregorian_to_lunar(solar_date));
  }
}

} // namespace calendar::lunar::algo1::test
