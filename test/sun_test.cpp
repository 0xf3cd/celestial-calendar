#include <gtest/gtest.h>
#include "util.hpp"
#include "astro.hpp"

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
