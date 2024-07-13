#include <gtest/gtest.h>
#include "util.hpp"
#include "astro.hpp"

TEST(AstroHelperTest, helper) {
  using namespace astro::helper;
  {
    const auto x = normalize_deg(361);
    ASSERT_TRUE(0 <= x and x < 360) << x;
  }
  {
    const auto x = normalize_deg(-1);
    ASSERT_TRUE(0 <= x and x < 360) << x;
  }
  {
    const auto x = normalize_deg(355);
    ASSERT_TRUE(0 <= x and x < 360) << x;
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


namespace astro::ecliptic::sun {

TEST(SunTest, vsop87d_longitude) {
  for (int i = 0; i < 1000; ++i) {
    const double random_jd = astro::julian_day::J2000 + util::random(-1000.0, 1000.0);
    const double lon = vsop87d_longitude(random_jd);
    ASSERT_TRUE(0 <= lon and lon < 360);
  }
}

TEST(SunTest, vsop87d_latitude) {
  for (int i = 0; i < 1000; ++i) {
    const double random_jd = astro::julian_day::J2000 + util::random(-1000.0, 1000.0);
    const double lat = vsop87d_latitude(random_jd);
    ASSERT_TRUE(-90 <= lat and lat <= 90);
  }
}

}
