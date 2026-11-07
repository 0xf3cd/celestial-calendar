/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <gtest/gtest.h>
#include "earth/refraction.hpp"
#include "../test_helper.hpp"

namespace astro::earth::refraction::test {
namespace {

using toolbox::AngleDeg;
using util::test::req;

/** @brief Independently apply the printed Meeus 16.3/16.4 weather factor. */
auto correction(const Params& params) -> double {
  return params.pressure_hpa / 1010.0 * 283.0 / (273.0 + params.temperature_c);
}

} // namespace

TEST(RefractionApplication, InclusiveBoundsAndNextafterAcrossWeatherWindow) {
  uint32_t signed_negative = 0;
  for (const auto model : { Model::BENNETT, Model::SAEMUNDSSON }) {
    for (const double temperature : { -90.0, std::nextafter(-90.0, 0.0), 10.0, std::nextafter(60.0, 0.0), 60.0 }) {
      for (const double pressure : { 300.0, std::nextafter(300.0, 1100.0), 1010.0, std::nextafter(1100.0, 300.0), 1100.0 }) {
        const Params params { .temperature_c = temperature, .pressure_hpa = pressure, .model = model };
        const double lower = model == Model::BENNETT ? -detail::apply_tp_correction(bennett(AngleDeg { 0.0 }), params).deg()
                                                     : -at_horizon(params).deg();
        EXPECT_FALSE(apparent_altitude(AngleDeg { std::nextafter(lower, -2.0) }, params).has_value());
        for (const double g : { lower, std::nextafter(lower, 90.0), 0.0, 1.0, 45.0, 89.999, 90.0 }) {
          const double apparent = req(apparent_altitude(AngleDeg { g }, params)).deg();
          if (model == Model::BENNETT) {
            const double forward = apparent - (correction(params)
              / (60.0 * std::tan(toolbox::deg_to_rad(apparent + (7.31 / (apparent + 4.4))))));
            EXPECT_NEAR(forward, g, BENNETT_INVERSE_TOL_DEG);
            EXPECT_GE(apparent, 0.0);
          } else {
            const double expected = g + ((1.02 * correction(params))
              / (60.0 * std::tan(toolbox::deg_to_rad(g + (10.3 / (g + 5.11))))));
            EXPECT_NEAR(apparent, expected, 2e-14);
            EXPECT_GE(apparent, -SAEMUNDSSON_HORIZON_TOL_DEG);
            if (g == lower and apparent < 0.0) {
              ++signed_negative;
              EXPECT_DOUBLE_EQ(apparent, g + detail::apply_tp_correction(saemundsson(AngleDeg { g }), params).deg());
            }
          }
        }
        EXPECT_FALSE(apparent_altitude(AngleDeg { std::nextafter(90.0, 100.0) }, params).has_value());
      }
    }
  }
  EXPECT_GT(signed_negative, UINT32_C(0));
}

TEST(RefractionApplication, WeatherErrorsAreNotUnsupportedAltitude) {
  for (const double temperature : { -90.001, 60.001, std::numeric_limits<double>::quiet_NaN() }) {
    EXPECT_THROW((void)apparent_altitude(AngleDeg { -30.0 }, { .temperature_c = temperature }), std::invalid_argument);
  }
  for (const double pressure : { 299.999, 1100.001, std::numeric_limits<double>::infinity() }) {
    EXPECT_THROW((void)apparent_altitude(AngleDeg { -30.0 }, { .pressure_hpa = pressure }), std::invalid_argument);
  }
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Exercise the public unknown-selector rejection.
  EXPECT_THROW((void)apparent_altitude(AngleDeg { 0.0 }, { .model = static_cast<Model>(255) }), std::invalid_argument);
  EXPECT_THROW((void)apparent_altitude(AngleDeg { std::numeric_limits<double>::quiet_NaN() }), std::invalid_argument);
  EXPECT_FALSE(apparent_altitude(AngleDeg { -30.0 }).has_value());
  EXPECT_LT(req(apparent_altitude(AngleDeg { 90.0 }, { .model = Model::SAEMUNDSSON })).deg(), 90.0);
  EXPECT_GT(req(apparent_altitude(AngleDeg { 0.0 })).deg(), 0.0);
}

} // namespace astro::earth::refraction::test
