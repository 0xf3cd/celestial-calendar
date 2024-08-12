#include <gtest/gtest.h>
#include "lunar/converter.hpp"
#include "lunar/algo1.hpp"
#include "lunar/algo2.hpp"

namespace calendar::lunar::converter::test {

TEST(Converter, IsValidGregorian) {
  using namespace std::literals;

  {
    using Converter = converter::Converter<common::Algo::ALGO_1>;

    ASSERT_FALSE(Converter::is_valid_gregorian(1901y / 2 / 18));
    ASSERT_TRUE(Converter::is_valid_gregorian(1901y / 2 / 19));
    ASSERT_TRUE(Converter::is_valid_gregorian(1901y / 2 / 20));

    ASSERT_TRUE(Converter::is_valid_gregorian(2024y / 3 / 17));

    ASSERT_TRUE(Converter::is_valid_gregorian(2099y / 1 / 21));
    ASSERT_TRUE(Converter::is_valid_gregorian(2100y / 2 / 8));
    ASSERT_FALSE(Converter::is_valid_gregorian(2100y / 2 / 9));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_2>;

    ASSERT_FALSE(Converter::is_valid_gregorian(499y / 2 / 18));
    ASSERT_TRUE(Converter::is_valid_gregorian(501y / 2 / 19));

    ASSERT_TRUE(Converter::is_valid_gregorian(2024y / 3 / 17));

    ASSERT_TRUE(Converter::is_valid_gregorian(3000y / 2 / 8));
    ASSERT_FALSE(Converter::is_valid_gregorian(3002y / 2 / 9));
  }
}

TEST(Converter, IsValidLunar) {
  using namespace std::literals;

  {
    using Converter = converter::Converter<common::Algo::ALGO_1>;

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

  {
    using Converter = converter::Converter<common::Algo::ALGO_2>;
    
    ASSERT_FALSE(Converter::is_valid_lunar(499y / 12 / 29));
    ASSERT_TRUE(Converter::is_valid_lunar(500y / 1 / 1));
  }
}

TEST(Converter, GregorianToLunarNegative) {
  using namespace std::literals;

  {
    using Converter = converter::Converter<common::Algo::ALGO_1>;

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

  {
    using Converter = converter::Converter<common::Algo::ALGO_2>;

    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(499y / 12 / 29));
    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(501y / 1 / 1));

    ASSERT_NE(std::nullopt, Converter::gregorian_to_lunar(3000y / 1 / 1));
    ASSERT_EQ(std::nullopt, Converter::gregorian_to_lunar(3002y / 1 / 1));
  }
}

TEST(Converter, LunarToGregorianNegative) {
  using namespace std::literals;

  {
    using Converter = converter::Converter<common::Algo::ALGO_1>;

    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2024y / 0 / 29));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(1901y / 1 / 0));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2099y / 12 / 30));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2100y / 1 / 1));
    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(2100y / 14 / 1));

    ASSERT_NE(std::nullopt, Converter::lunar_to_gregorian(1901y / 1 / 1));
    ASSERT_NE(std::nullopt, Converter::lunar_to_gregorian(2099y / 13 / 30));
  }

  {
    using Converter = converter::Converter<common::Algo::ALGO_2>;

    ASSERT_EQ(std::nullopt, Converter::lunar_to_gregorian(499y / 12 / 29));
    ASSERT_NE(std::nullopt, Converter::lunar_to_gregorian(500y / 1 / 1));
  }
}

TEST(Converter, GregorianToLunarAlgo1) {
  using namespace util::ymd_operator;

  using AlgoMetadata = common::AlgoMetadata<common::Algo::ALGO_1>;
  using Converter = converter::Converter<common::Algo::ALGO_1>;

  for (auto year = AlgoMetadata::bounds.start_lunar_year; year <= AlgoMetadata::bounds.end_lunar_year; ++year) {
    const auto& info = AlgoMetadata::get_info_for_year(year);
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

TEST(Converter, GregorianToLunarAlgo2) {
  using namespace util::ymd_operator;

  using AlgoMetadata = common::AlgoMetadata<common::Algo::ALGO_2>;
  using Converter = converter::Converter<common::Algo::ALGO_2>;

  // Because algo2 is too slow, we randomly skip some years.
  std::set<int32_t> years;
  for (auto _ = 0; _ < 8; ++_) {
    years.insert(
      util::random(AlgoMetadata::bounds.start_lunar_year, 
                   AlgoMetadata::bounds.end_lunar_year
      )
    );
  }

  for (const auto year : years) {
    const auto& info = AlgoMetadata::get_info_for_year(year);
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

TEST(Converter, LunarToGregorianAlgo1) {
  using namespace util::ymd_operator;

  using AlgoMetadata = common::AlgoMetadata<common::Algo::ALGO_1>;
  using Converter = converter::Converter<common::Algo::ALGO_1>;

  for (auto year = AlgoMetadata::bounds.start_lunar_year; year <= AlgoMetadata::bounds.end_lunar_year; ++year) {
    const auto& info = AlgoMetadata::get_info_for_year(year);
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

TEST(Converter, LunarToGregorianAlgo2) {
  using namespace util::ymd_operator;

  using AlgoMetadata = common::AlgoMetadata<common::Algo::ALGO_2>;
  using Converter = converter::Converter<common::Algo::ALGO_2>;

  // Because algo2 is too slow, we randomly skip some years.
  std::set<int32_t> years;
  for (auto _ = 0; _ < 8; ++_) {
    years.insert(
      util::random(AlgoMetadata::bounds.start_lunar_year, 
                   AlgoMetadata::bounds.end_lunar_year
      )
    );
  }

  for (const auto year : years) {
    const auto& info = AlgoMetadata::get_info_for_year(year);
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

TEST(Converter, IntegrationAlgo1) {
  using namespace util::ymd_operator;
  using std::chrono::sys_days;
  using std::chrono::year_month_day;

  using AlgoMetadata = common::AlgoMetadata<common::Algo::ALGO_1>;
  using Converter = converter::Converter<common::Algo::ALGO_1>;

  const uint32_t difference = (sys_days(AlgoMetadata::bounds.last_gregorian_date) - sys_days(AlgoMetadata::bounds.first_gregorian_date)).count();
  for (auto _ = 0; _ < 5000; ++_) {
    const year_month_day solar_date = AlgoMetadata::bounds.first_gregorian_date + util::random<uint32_t>(0, difference);
    ASSERT_TRUE(Converter::is_valid_gregorian(solar_date));

    const std::optional<year_month_day> optional_lunar_date = Converter::gregorian_to_lunar(solar_date);
    ASSERT_TRUE(optional_lunar_date.has_value());
    
    const year_month_day lunar_date = optional_lunar_date.value(); // NOLINT(bugprone-unchecked-optional-access)
    ASSERT_TRUE(Converter::is_valid_lunar(lunar_date));

    ASSERT_EQ(solar_date, Converter::lunar_to_gregorian(lunar_date));
    ASSERT_EQ(lunar_date, Converter::gregorian_to_lunar(solar_date));
  }
}

TEST(Converter, IntegrationAlgo2) {
  using namespace util::ymd_operator;
  using std::chrono::sys_days;
  using std::chrono::year_month_day;

  using AlgoMetadata = common::AlgoMetadata<common::Algo::ALGO_2>;
  using Converter = converter::Converter<common::Algo::ALGO_2>;

  const uint32_t difference = (sys_days(AlgoMetadata::bounds.last_gregorian_date) - sys_days(AlgoMetadata::bounds.first_gregorian_date)).count();
  for (auto _ = 0; _ < 16; ++_) {
    const year_month_day solar_date = AlgoMetadata::bounds.first_gregorian_date + util::random<uint32_t>(0, difference);
    ASSERT_TRUE(Converter::is_valid_gregorian(solar_date));

    const std::optional<year_month_day> optional_lunar_date = Converter::gregorian_to_lunar(solar_date);
    ASSERT_TRUE(optional_lunar_date.has_value());
    
    const year_month_day lunar_date = optional_lunar_date.value(); // NOLINT(bugprone-unchecked-optional-access)
    ASSERT_TRUE(Converter::is_valid_lunar(lunar_date));

    ASSERT_EQ(solar_date, Converter::lunar_to_gregorian(lunar_date));
    ASSERT_EQ(lunar_date, Converter::gregorian_to_lunar(solar_date));
  }
}

}  // namespace calendar::lunar::converter::test
