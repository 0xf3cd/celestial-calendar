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

#include <array>
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
    const auto result = celestial_topocentric_position(
      target,
      2026,
      11,
      7,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_ALGO5,
      -33.85,
      151.2,
      1706.0,
      CELESTIAL_REFRACTION_NONE,
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity()
    );
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    const auto expected = astro::observer::query(
      calendar::Datetime { util::to_ymd(2026, 11, 7), 0.0 },
      julian_day::Scale::UTC,
      { .location = { .latitude = toolbox::AngleDeg { -33.85 }, .longitude = toolbox::AngleDeg { 151.2 } },
        .elevation = toolbox::DistanceKm { 1.706 } },
      static_cast<ephemeris::Target>(target)
    );
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
    const auto result = celestial_topocentric_position(
      CELESTIAL_TARGET_SUN,
      2026,
      11,
      7,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      pole,
      42.0,
      0.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    );
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.present_fields & CELESTIAL_PRESENT_AZIMUTH, UINT32_C(0));
    EXPECT_TRUE(std::isfinite(result.altitude_deg));
    const auto adjacent = celestial_topocentric_position(
      CELESTIAL_TARGET_SUN,
      2026,
      11,
      7,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      std::nextafter(pole, 0.0),
      42.0,
      0.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    );
    ASSERT_TRUE(adjacent.valid);
    EXPECT_NE(adjacent.present_fields & CELESTIAL_PRESENT_AZIMUTH, UINT32_C(0));
  }
  uint32_t supported = 0;
  uint32_t unsupported = 0;
  for (const uint32_t model : { CELESTIAL_REFRACTION_BENNETT, CELESTIAL_REFRACTION_SAEMUNDSSON }) {
    for (uint32_t hour = 0; hour < 24; ++hour) {
      const auto result = celestial_topocentric_position(
        CELESTIAL_TARGET_SUN,
        2026,
        11,
        7,
        hour / 24.0,
        CELESTIAL_CIVIL_SCALE_UTC,
        CELESTIAL_DELTA_T_MODEL_DEFAULT,
        33.0,
        -116.0,
        1706.0,
        model,
        15.0,
        1013.25
      );
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
    const auto result = celestial_topocentric_position(
      CELESTIAL_TARGET_SUN,
      2026,
      11,
      7,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      33.0,
      -116.0,
      0.0,
      model,
      -91.0,
      1010.0
    );
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

TEST(ObserverCAbi, CivilModelProviderAndNodeGuards) {
  EXPECT_TRUE(
    celestial_topocentric_position(
      CELESTIAL_TARGET_SUN,
      1000,
      1,
      1,
      0.0,
      CELESTIAL_CIVIL_SCALE_UT1,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      0.0,
      0.0,
      0.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    ).valid
  );
  EXPECT_TRUE(
    celestial_topocentric_position(
      CELESTIAL_TARGET_SUN,
      1972,
      1,
      1,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      0.0,
      0.0,
      0.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    ).valid
  );
  EXPECT_FALSE(
    celestial_topocentric_position(
      CELESTIAL_TARGET_SUN,
      1971,
      12,
      31,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      0.0,
      0.0,
      0.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    ).valid
  );
  EXPECT_FALSE(
    celestial_topocentric_position(
      CELESTIAL_TARGET_SUN,
      2036,
      1,
      1,
      0.0,
      CELESTIAL_CIVIL_SCALE_UT1,
      CELESTIAL_DELTA_T_MODEL_ALGO4,
      0.0,
      0.0,
      0.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    ).valid
  );
  EXPECT_TRUE(
    celestial_topocentric_position(
      CELESTIAL_TARGET_PLUTO,
      2099,
      12,
      31,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      0.0,
      0.0,
      0.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    ).valid
  );
  EXPECT_FALSE(
    celestial_topocentric_position(
      CELESTIAL_TARGET_PLUTO,
      2100,
      1,
      1,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      0.0,
      0.0,
      0.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    ).valid
  );
  for (const uint32_t target : { UINT32_C(10), UINT32_C(13), UINT32_C(99) }) {
    const auto result = celestial_topocentric_position(
      target,
      2026,
      1,
      1,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      0.0,
      0.0,
      0.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    );
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

TEST(ObserverCAbi, RefractionCompositionParity) {
  using namespace astro;
  const auto civil = calendar::Datetime { util::to_ymd(2026, 11, 7), 0.0 };
  const astro::observer::Observer site {
    .location = { .latitude = toolbox::AngleDeg { -33.85 }, .longitude = toolbox::AngleDeg { 151.2 } },
    .elevation = toolbox::DistanceKm { 1.706 },
  };
  const auto geometric = astro::observer::query(civil, julian_day::Scale::UTC, site, ephemeris::Target::MOON);
  for (const uint32_t code : { CELESTIAL_REFRACTION_BENNETT, CELESTIAL_REFRACTION_SAEMUNDSSON }) {
    const auto model = code == CELESTIAL_REFRACTION_BENNETT ? earth::refraction::Model::BENNETT
                                                         : earth::refraction::Model::SAEMUNDSSON;
    const earth::refraction::Params params { .temperature_c = 5.0, .pressure_hpa = 900.0, .model = model };
    const auto standalone = util::test::req(earth::refraction::apparent_altitude(geometric.position.altitude, params));
    const auto expected = astro::observer::query(civil, julian_day::Scale::UTC, site, ephemeris::Target::MOON, params);
    const auto result = celestial_topocentric_position(
      CELESTIAL_TARGET_MOON,
      2026,
      11,
      7,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_ALGO5,
      -33.85,
      151.2,
      1706.0,
      code,
      5.0,
      900.0
    );
    ASSERT_TRUE(result.valid);
    ASSERT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    EXPECT_EQ(result.present_fields, CELESTIAL_PRESENT_RIGHT_ASCENSION | CELESTIAL_PRESENT_AZIMUTH
                                   | CELESTIAL_PRESENT_APPARENT_ALTITUDE);
    EXPECT_DOUBLE_EQ(result.jd_ut1, expected.times.jd_ut1);
    EXPECT_DOUBLE_EQ(result.jde_tt, expected.times.jde_tt);
    EXPECT_DOUBLE_EQ(result.right_ascension_deg, util::test::req(expected.position.right_ascension).deg());
    EXPECT_DOUBLE_EQ(result.declination_deg, expected.position.declination.deg());
    EXPECT_DOUBLE_EQ(result.distance_au, expected.position.distance.au());
    EXPECT_DOUBLE_EQ(result.azimuth_deg, util::test::req(expected.position.azimuth).deg());
    EXPECT_DOUBLE_EQ(result.altitude_deg, geometric.position.altitude.deg());
    EXPECT_DOUBLE_EQ(result.apparent_altitude_deg, util::test::req(expected.position.apparent_altitude).deg());
    EXPECT_DOUBLE_EQ(result.apparent_altitude_deg, standalone.deg());
  }
}

TEST(ObserverCAbi, SiteErrorsAndImmediateRecovery) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  const double floor_m = astro::observer::MINIMUM_ELEVATION_KM * 1000.0;
  const std::array invalid_sites {
    std::array { nan, 0.0, 0.0 }, std::array { infinity, 0.0, 0.0 }, std::array { -infinity, 0.0, 0.0 },
    std::array { 0.0, nan, 0.0 }, std::array { 0.0, infinity, 0.0 }, std::array { 0.0, -infinity, 0.0 },
    std::array { 0.0, 0.0, nan }, std::array { 0.0, 0.0, infinity }, std::array { 0.0, 0.0, -infinity },
    std::array { std::nextafter(-90.0, -infinity), 0.0, 0.0 },
    std::array { std::nextafter(90.0, infinity), 0.0, 0.0 },
    std::array { 0.0, std::nextafter(-180.0, -infinity), 0.0 },
    std::array { 0.0, std::nextafter(180.0, infinity), 0.0 },
    std::array { 0.0, 0.0, floor_m }, std::array { 0.0, 0.0, std::nextafter(floor_m, -infinity) },
  };
  for (const auto& site : invalid_sites) {
    SCOPED_TRACE(::testing::PrintToString(site));
    const auto invalid = celestial_topocentric_position(
      CELESTIAL_TARGET_MOON,
      2026,
      11,
      7,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      site[0],
      site[1],
      site[2],
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    );
    ASSERT_FALSE(invalid.valid);
    ASSERT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);

    const auto recovered = celestial_topocentric_position(
      CELESTIAL_TARGET_MOON,
      2026,
      11,
      7,
      0.0,
      CELESTIAL_CIVIL_SCALE_UTC,
      CELESTIAL_DELTA_T_MODEL_DEFAULT,
      -33.85,
      151.2,
      1706.0,
      CELESTIAL_REFRACTION_NONE,
      0.0,
      0.0
    );
    ASSERT_TRUE(recovered.valid);
    ASSERT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  }
}

} // namespace lib::observer::test
