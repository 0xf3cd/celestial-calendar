#include <gtest/gtest.h>
#include "util.hpp"
#include "astro.hpp"

namespace astro::sun {

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


// Data used in following testing was obtained from PyMeeus (https://pypi.org/project/PyMeeus/).
// PyMeeus is a well-implemented Python library for astronomical calculations.

struct PyMeeusResult {
  double lon;
  double lat;
  double r;
};

TEST(SunTest, vsop87d) {
  // Simply invoked `Sun.geometric_geocentric_position(epoch, tofk5=False)` to get the following data.
  // No any correction/adjustment. Just the plain calculation results from VSOP87D.
  const std::unordered_map<double, PyMeeusResult> PY_MEEUS_RESULTS {
    { astro::julian_day::J2000, {  280.3778436711984,  0.00022721051444223787, 0.9833276819105508 } },
    {                2445701.1, {  280.3662619392999,  1.3856637992645944e-05, 0.9832889892830442 } },
    {                2454359.1, { 172.37619608369778, -0.00010725756446863268, 1.0057018016353796 } },
    {               2460505.25, { 111.82761866574583,  -3.226703118214394e-05, 1.0165107642588653 } },
    {                2464080.5, {  37.80127186208301, -0.00012100241309577134, 1.0065840587631982 } },
    {            2454774.36215, {  221.8062247735661, -0.00010196625102869854, 0.991769848723092  } },
  };

  for (const auto& [jd, result] : PY_MEEUS_RESULTS) {
    const double lon = vsop87d_longitude(jd);
    const double lat = vsop87d_latitude(jd);
    ASSERT_NEAR(lon, result.lon, 1e-10);
    ASSERT_NEAR(lat, result.lat, 1e-10);
  }
}

}
