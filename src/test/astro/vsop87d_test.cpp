/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>
#include "julian_day.hpp"
#include "vsop87d/vsop87d.hpp"

namespace astro::vsop87d::test {

using namespace astro::vsop87d;

// Data was obtained from PyMeeus (https://pypi.org/project/PyMeeus/).
// PyMeeus is a well-implemented Python library for astronomical calculations.
//
// The values are directly returned by VSOP87D models, no any adjustment or correction.
const std::unordered_map<double, std::tuple<double, double, double>> EARTH_DATASET {
  //    JDE          L-tables expected   B-tables expected        R-tables expected
  {     2445701.1, { -98.77924318611353, -2.4184395622860954e-07, 0.9832889892830442 } },
  {     2451545.0, {  1.751923868114564, -3.9655715721671785e-06, 0.9833276819105508 } },
  {     2454359.1, {  50.13242197757078,  1.8719976476477224e-06, 1.0057018016353796 } },
  { 2454774.36215, { 57.278324034743825,  1.7796468063658446e-06,  0.991769848723092 } },
  {    2460505.25, {  155.8898001662818,   5.631659339720899e-07, 1.0165107642588653 } },
  { 2462597.96105, { 191.92860080429793,  -5.548701174542588e-07, 1.0006923288119707 } },
  {     2464080.5, { 217.42964975313058,  2.1118905113795144e-06, 1.0065840587631982 } },
};

TEST(Vsop87d, Evaluate) {
  for (const auto& [jde, expected] : EARTH_DATASET) {
    const auto& [lon, lat, r] = expected;
    const auto jm = julian_day::jde_to_jm(jde);

    ASSERT_NEAR(evaluate_tables(earth_coeff::L, jm), lon, 1e-10);
    ASSERT_NEAR(evaluate_tables(earth_coeff::B, jm), lat, 1e-10);
    ASSERT_NEAR(evaluate_tables(earth_coeff::R, jm), r,   1e-10);
  }
}

TEST(Vsop87d, EvaluateTemplate) {
  // `evaluate<Planet::EAR>` is the field-mapping layer over `evaluate_tables`: .λ ← L, .β ← B,
  // .r ← R, unadjusted. A swapped or rescaled field passes neither assertion family below.
  for (const auto& [jde, expected] : EARTH_DATASET) {
    const auto& [lon, lat, r] = expected;
    const auto jm = julian_day::jde_to_jm(jde);
    const auto eval = evaluate<Planet::EAR>(jm);

    // The external dataset through the template path.
    ASSERT_NEAR(eval.λ, lon, 1e-10);
    ASSERT_NEAR(eval.β, lat, 1e-10);
    ASSERT_NEAR(eval.r, r,   1e-10);

    // Same bits as reading the tables directly — the wrapper neither scales nor reorders.
    ASSERT_EQ(eval.λ, evaluate_tables(earth_coeff::L, jm));
    ASSERT_EQ(eval.β, evaluate_tables(earth_coeff::B, jm));
    ASSERT_EQ(eval.r, evaluate_tables(earth_coeff::R, jm));
  }
}

}  // namespace astro::vsop87d::test
