#include <gtest/gtest.h>
#include "converter.hpp"

namespace calendar::converter {

TEST(ConverterTest, test_array_size) {
  EXPECT_EQ(199, LUNAR_DATA.size());
  EXPECT_EQ(END_YEAR - START_YEAR + 1, LUNAR_DATA.size());
}

TEST(ConverterTest, test_get_lunar_year_info) {
  ASSERT_THROW(get_lunar_year_info(START_YEAR - 1), std::out_of_range);
  ASSERT_THROW(get_lunar_year_info(END_YEAR + 1), std::out_of_range);

  // Print the first solar day of year 1901.
  // const auto ymd = get_lunar_year_info(1901).date_of_first_day;

  const uint32_t year = 2024;

  // using namespace std::chrono;
  std::chrono::year_month_day first_solar_day { std::chrono::January / 1 / year };
  std::cout << static_cast<int>(first_solar_day.year()) << std::endl;

  auto ymd { first_solar_day.year() / first_solar_day.month() / (first_solar_day.day() + std::chrono::days{100})};
  std::cout << static_cast<int>(ymd.year()) << std::endl;
  std::cout << static_cast<unsigned>(ymd.month()) << std::endl;
  std::cout << static_cast<unsigned>(ymd.day()) << std::endl;
  std::cout << ymd.ok() << std::endl;

  auto ret = std::chrono::sys_days(first_solar_day) + std::chrono::days{100};
  auto new_ymd = std::chrono::year_month_day{ret};

  std::cout << static_cast<int>(new_ymd.year()) << std::endl;
  std::cout << static_cast<unsigned>(new_ymd.month()) << std::endl;
  std::cout << static_cast<unsigned>(new_ymd.day()) << std::endl;
}

}
