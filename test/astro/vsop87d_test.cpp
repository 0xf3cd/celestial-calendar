#include <gtest/gtest.h>
#include "julian_day.hpp"
#include "random.hpp"
#include "vsop87d/vsop87d.hpp"

namespace astro::vsop87d {

TEST(Vsop87d, evaluate) {
  // Data was obtained from PyMeeus (https://pypi.org/project/PyMeeus/).
  // PyMeeus is a well-implemented Python library for astronomical calculations.
  //
  // The values are directly returned by VSOP87D models, no any adjustment or correction.
  const std::unordered_map<double, std::tuple<double, double, double>> EXPECTED {
    //    JDE          L-tables expected   B-tables expected        R-tables expected
    {     2445701.1, { -98.77924318611353, -2.4184395622860954e-07, 0.9832889892830442 } },
    {     2451545.0, {  1.751923868114564, -3.9655715721671785e-06, 0.9833276819105508 } },
    {     2454359.1, {  50.13242197757078,  1.8719976476477224e-06, 1.0057018016353796 } },
    { 2454774.36215, { 57.278324034743825,  1.7796468063658446e-06,  0.991769848723092 } },
    {    2460505.25, {  155.8898001662818,   5.631659339720899e-07, 1.0165107642588653 } },
    { 2462597.96105, { 191.92860080429793,  -5.548701174542588e-07, 1.0006923288119707 } },
    {     2464080.5, { 217.42964975313058,  2.1118905113795144e-06, 1.0065840587631982 } },
  };

  for (const auto& [jde, expected] : EXPECTED) {
    const auto& [lon, lat, r] = expected;
    const auto jm = julian_day::jde_to_jm(jde);

    ASSERT_NEAR(evaluate_tables(earth_coeff::L, jm), lon, 1e-10);
    ASSERT_NEAR(evaluate_tables(earth_coeff::B, jm), lat, 1e-10);
    ASSERT_NEAR(evaluate_tables(earth_coeff::R, jm), r,   1e-10);
  }
}

}
