#include <gtest/gtest.h>
#include "util.hpp"
#include "converter.hpp"

namespace calendar::converter {

TEST(Converter, test_solar_to_lunar) {
  using namespace std::literals;
  using util::to_ymd;

  ASSERT_EQ(solar_to_lunar(to_ymd(1901, 1, 1)), 1901y / 1 / 1);
}

}
