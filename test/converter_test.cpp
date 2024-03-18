#include <gtest/gtest.h>
#include "date.hpp"
#include "random.hpp"
#include "converter.hpp"

namespace calendar::converter {

TEST(Converter, test_is_valid_solar) {
  using namespace std::literals;

  ASSERT_FALSE(is_valid_solar(1901y / 2 / 18));
  ASSERT_TRUE(is_valid_solar(1901y / 2 / 19));
  ASSERT_TRUE(is_valid_solar(1901y / 2 / 20));

  ASSERT_TRUE(is_valid_solar(2024y / 3 / 17));

  ASSERT_TRUE(is_valid_solar(2099y / 1 / 21));
  ASSERT_TRUE(is_valid_solar(2100y / 2 / 8));
  ASSERT_FALSE(is_valid_solar(2100y / 2 / 9));
}

TEST(Converter, test_is_valid_lunar) {
  using namespace std::literals;

  ASSERT_FALSE(is_valid_lunar(1900y / 12 / 29));
  ASSERT_FALSE(is_valid_lunar(1901y / 1 / 0));
  ASSERT_TRUE(is_valid_lunar(1901y / 1 / 1));
  ASSERT_TRUE(is_valid_lunar(1901y / 1 / 2));

  ASSERT_TRUE(is_valid_lunar(2024y / 3 / 17));
  ASSERT_FALSE(is_valid_lunar(2024y / 0 / 0));
  ASSERT_FALSE(is_valid_lunar(2024y / 0 / 1));
  ASSERT_FALSE(is_valid_lunar(2024y / 0 / 28));
  ASSERT_FALSE(is_valid_lunar(2024y / 1 / 0));
  ASSERT_FALSE(is_valid_lunar(2024y / 12 / 0));
  ASSERT_FALSE(is_valid_lunar(2024y / 14 / 0));

  ASSERT_TRUE(is_valid_lunar(2099y / 1 / 1));
  ASSERT_TRUE(is_valid_lunar(2099y / 12 / 29));
  ASSERT_FALSE(is_valid_lunar(2099y / 12 / 30));
  ASSERT_TRUE(is_valid_lunar(2099y / 13 / 30));
  ASSERT_FALSE(is_valid_lunar(2099y / 14 / 0));
  ASSERT_FALSE(is_valid_lunar(2099y / 14 / 1));
  ASSERT_FALSE(is_valid_lunar(2100y / 1 / 1));
}

TEST(Converter, test_solar_to_lunar_negative) {
  using namespace std::literals;

  ASSERT_EQ(std::nullopt, solar_to_lunar(2024y / 0 / 29));
  ASSERT_EQ(std::nullopt, solar_to_lunar(1901y / 1 / 0));
  ASSERT_EQ(std::nullopt, solar_to_lunar(1901y / 2 / 18));
  ASSERT_EQ(std::nullopt, solar_to_lunar(2099y / 12 / 32));
  ASSERT_EQ(std::nullopt, solar_to_lunar(2100y / 0 / 1));
  ASSERT_EQ(std::nullopt, solar_to_lunar(2100y / 0 / 1));
  ASSERT_EQ(std::nullopt, solar_to_lunar(2100y / 2 / 9));

  ASSERT_NE(std::nullopt, solar_to_lunar(1901y / 2 / 19));
  ASSERT_NE(std::nullopt, solar_to_lunar(2100y / 2 / 8));
  ASSERT_NE(std::nullopt, solar_to_lunar(2024y / 3 / 18));
}

TEST(Converter, test_lunar_to_solar_negative) {
  using namespace std::literals;

  ASSERT_EQ(std::nullopt, lunar_to_solar(2024y / 0 / 29));
  ASSERT_EQ(std::nullopt, lunar_to_solar(1901y / 1 / 0));
  ASSERT_EQ(std::nullopt, lunar_to_solar(2099y / 12 / 30));
  ASSERT_EQ(std::nullopt, lunar_to_solar(2100y / 1 / 1));
  ASSERT_EQ(std::nullopt, lunar_to_solar(2100y / 14 / 1));

  ASSERT_NE(std::nullopt, lunar_to_solar(1901y / 1 / 1));
  ASSERT_NE(std::nullopt, lunar_to_solar(2099y / 13 / 30));
}

TEST(Converter, test_solar_to_lunar) {
  using namespace util;

  for (auto year = START_YEAR; year <= END_YEAR; ++year) {
    const auto& info = lunardata_cache.get(year);
    ASSERT_EQ(util::to_ymd(year, 1, 1), solar_to_lunar(info.date_of_first_day));

    uint32_t days_count = 0;
    const auto& ml = info.month_lengths;
    for (uint32_t month_idx = 0; month_idx < ml.size(); ++month_idx) { // Iterate over all months.
      for (uint32_t day = 1; day <= ml[month_idx]; ++day) { // Iterate over all days in the month.
        const auto lunar_date = util::to_ymd(year, month_idx + 1, day);
        ASSERT_EQ(lunar_date, solar_to_lunar(info.date_of_first_day + days_count));
        days_count++;
      }
    }
  }
}

TEST(Converter, test_lunar_to_solar) {
  using namespace util;

  for (auto year = START_YEAR; year <= END_YEAR; ++year) {
    const auto& info = lunardata_cache.get(year);
    ASSERT_EQ(info.date_of_first_day, lunar_to_solar(util::to_ymd(year, 1, 1)));

    uint32_t days_count = 0;
    const auto& ml = info.month_lengths;
    for (uint32_t month_idx = 0; month_idx < ml.size(); ++month_idx) { // Iterate over all months.
      for (uint32_t day = 1; day <= ml[month_idx]; ++day) { // Iterate over all days in the month.
        const auto lunar_date = util::to_ymd(year, month_idx + 1, day);
        ASSERT_EQ(info.date_of_first_day + days_count, lunar_to_solar(lunar_date));
        days_count++;
      }
    }
  }
}

TEST(Coverter, test_integration) {
  using namespace util;
  using std::chrono::sys_days;
  using std::chrono::year_month_day;

  const uint32_t diffrence = (sys_days(LAST_SOLAR_DATE) - sys_days(FIRST_SOLAR_DATE)).count();
  for (auto _ = 0; _ < 5000; ++_) {
    const year_month_day solar_date = FIRST_SOLAR_DATE + gen_random_value<uint32_t>(0, diffrence);
    ASSERT_TRUE(is_valid_solar(solar_date));

    const std::optional<year_month_day> optional_lunar_date = solar_to_lunar(solar_date);
    ASSERT_TRUE(optional_lunar_date.has_value());
    
    const year_month_day lunar_date = optional_lunar_date.value();
    ASSERT_TRUE(is_valid_lunar(lunar_date));

    ASSERT_EQ(solar_date, lunar_to_solar(lunar_date));
    ASSERT_EQ(lunar_date, solar_to_lunar(solar_date));
  }
}

}
