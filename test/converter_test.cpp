#include <gtest/gtest.h>
#include "date.hpp"
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

}
