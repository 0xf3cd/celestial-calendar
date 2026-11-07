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
#include <gtest/gtest.h>
#include "celestial.h"
#include "observer.hpp"
#include "../test_helper.hpp"

namespace lib::observer::test {

TEST(ObserverCAbi, NativeParityIdentitiesAndOldGeocentricPreservation) {
  using namespace astro;
  for (uint32_t target = 0; target < 10; ++target) {
    const auto before = celestial_sun_apparent_geocentric_coord(2461351.5);
    const auto result = celestial_topocentric_position(target, 2026, 11, 7, 0.0, CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_ALGO5, -33.85, 151.2, 1706.0, CELESTIAL_REFRACTION_NONE,
      std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity());
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    const auto expected = astro::observer::query(calendar::Datetime { util::to_ymd(2026, 11, 7), 0.0 }, julian_day::Scale::UTC,
      { .location = { .latitude = toolbox::AngleDeg { -33.85 }, .longitude = toolbox::AngleDeg { 151.2 } },
        .elevation = toolbox::DistanceKm { 1.706 } }, static_cast<ephemeris::Target>(target));
    EXPECT_EQ(result.target_code, target);
    EXPECT_EQ(result.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO5);
    EXPECT_EQ(result.present_fields, CELESTIAL_PRESENT_RIGHT_ASCENSION | CELESTIAL_PRESENT_AZIMUTH);
    EXPECT_DOUBLE_EQ(result.jd_ut1, expected.times.jd_ut1);
    EXPECT_DOUBLE_EQ(result.jde_tt, expected.times.jde_tt);
    EXPECT_DOUBLE_EQ(result.altitude_deg, expected.position.altitude.deg());
    EXPECT_DOUBLE_EQ(result.azimuth_deg, util::test::req(expected.position.azimuth).deg());
    EXPECT_DOUBLE_EQ(result.distance_au, expected.position.distance.au());
    const auto after = celestial_sun_apparent_geocentric_coord(2461351.5);
    EXPECT_DOUBLE_EQ(before.lon, after.lon);
    EXPECT_DOUBLE_EQ(before.lat, after.lat);
    EXPECT_DOUBLE_EQ(before.r, after.r);
  }
}

TEST(ObserverCAbi, PoleConventionAndWeatherAbsenceErrors) {
  for (const double pole : { -90.0, 90.0 }) {
    const auto result = celestial_topocentric_position(0, 2026, 11, 7, 0.0, 0, 0, pole, 42.0, 0.0, 0, 0.0, 0.0);
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.present_fields & CELESTIAL_PRESENT_AZIMUTH, UINT32_C(0));
    EXPECT_TRUE(std::isfinite(result.altitude_deg));
    const auto adjacent = celestial_topocentric_position(0, 2026, 11, 7, 0.0, 0, 0, std::nextafter(pole, 0.0), 42.0, 0.0, 0, 0.0, 0.0);
    ASSERT_TRUE(adjacent.valid);
    EXPECT_NE(adjacent.present_fields & CELESTIAL_PRESENT_AZIMUTH, UINT32_C(0));
  }
  uint32_t supported = 0;
  uint32_t unsupported = 0;
  for (const uint32_t model : { CELESTIAL_REFRACTION_BENNETT, CELESTIAL_REFRACTION_SAEMUNDSSON }) {
    for (uint32_t hour = 0; hour < 24; ++hour) {
      const auto result = celestial_topocentric_position(0, 2026, 11, 7, hour / 24.0, 0, 0, 33.0, -116.0, 1706.0, model, 15.0, 1013.25);
      ASSERT_TRUE(result.valid);
      if ((result.present_fields & CELESTIAL_PRESENT_APPARENT_ALTITUDE) != 0) {
        ++supported;
      } else {
        ++unsupported;
      }
    }
  }
  EXPECT_GT(supported, UINT32_C(0));
  EXPECT_GT(unsupported, UINT32_C(0));
  for (const uint32_t model : { CELESTIAL_REFRACTION_BENNETT, CELESTIAL_REFRACTION_SAEMUNDSSON, UINT32_C(99) }) {
    EXPECT_FALSE(celestial_topocentric_position(0, 2026, 11, 7, 0.0, 0, 0, 33.0, -116.0, 0.0, model, -91.0, 1010.0).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

TEST(ObserverCAbi, CivilModelProviderAndNodeGuards) {
  EXPECT_TRUE(celestial_topocentric_position(0, 1000, 1, 1, 0.0, 1, 0, 0.0, 0.0, 0.0, 0, 0.0, 0.0).valid);
  EXPECT_TRUE(celestial_topocentric_position(0, 1972, 1, 1, 0.0, 0, 0, 0.0, 0.0, 0.0, 0, 0.0, 0.0).valid);
  EXPECT_FALSE(celestial_topocentric_position(0, 1971, 12, 31, 0.0, 0, 0, 0.0, 0.0, 0.0, 0, 0.0, 0.0).valid);
  EXPECT_FALSE(celestial_topocentric_position(0, 2036, 1, 1, 0.0, 1, 4, 0.0, 0.0, 0.0, 0, 0.0, 0.0).valid);
  EXPECT_TRUE(celestial_topocentric_position(9, 2099, 12, 31, 0.0, 0, 0, 0.0, 0.0, 0.0, 0, 0.0, 0.0).valid);
  EXPECT_FALSE(celestial_topocentric_position(9, 2100, 1, 1, 0.0, 0, 0, 0.0, 0.0, 0.0, 0, 0.0, 0.0).valid);
  for (const uint32_t target : { UINT32_C(10), UINT32_C(13), UINT32_C(99) }) {
    EXPECT_FALSE(celestial_topocentric_position(target, 2026, 1, 1, 0.0, 0, 0, 0.0, 0.0, 0.0, 0, 0.0, 0.0).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

} // namespace lib::observer::test
