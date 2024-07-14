#include <gtest/gtest.h>
#include "math.hpp"
#include "util.hpp"

TEST(VSPO87D, math) {
  using namespace astro::math;
  {
    const auto x = ensure_positive_deg(361);
    ASSERT_EQ(x, 1);
  }
  {
    const auto x = ensure_positive_deg(-1);
    ASSERT_EQ(x, 359);
  }
  {
    const auto x = ensure_positive_deg(355);
    ASSERT_EQ(x, 355);
  }
  {
    const auto x = ensure_positive_deg(0);
    ASSERT_EQ(x, 0);
  }
  {
    const auto x = ensure_positive_deg(180);
    ASSERT_EQ(x, 180);
  }
  {
    const auto x = ensure_positive_deg(-180.05);
    ASSERT_FLOAT_EQ(x, 179.95);
  }
  {
    for (auto i = 0; i < 1000; ++i) {
      const double random_deg = util::random(-720.0, 720.0);
      const double random_rad = deg_to_rad(random_deg);

      const auto rad = deg_to_rad(random_deg);
      const auto deg = rad_to_deg(random_rad);

      ASSERT_FLOAT_EQ(deg, random_deg);
      ASSERT_FLOAT_EQ(rad, random_rad);
    }
  }
}
