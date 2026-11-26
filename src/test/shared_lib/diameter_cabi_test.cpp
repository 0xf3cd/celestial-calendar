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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <gtest/gtest.h>
#include "apparent_diameter.hpp"
#include "celestial.h"

namespace lib::diameter::test {

// Independent native signature/layout pins, not generated from the subject header.
using Signature = CelestialApparentDiameter (*)(uint32_t, int32_t, uint32_t, uint32_t, double,
  uint32_t, uint32_t, uint32_t, double, double, double);
static_assert(std::is_same_v<decltype(&celestial_apparent_diameter), Signature>);
static_assert(sizeof(CelestialApparentDiameter) == 64 and alignof(CelestialApparentDiameter) == 8);
static_assert(offsetof(CelestialApparentDiameter, target_code) == 4);
static_assert(offsetof(CelestialApparentDiameter, centre_code) == 8);
static_assert(offsetof(CelestialApparentDiameter, source_code) == 12);
static_assert(offsetof(CelestialApparentDiameter, radius_definition_code) == 16);
static_assert(offsetof(CelestialApparentDiameter, delta_t_identity_code) == 20);
static_assert(offsetof(CelestialApparentDiameter, jd_ut1) == 24);
static_assert(offsetof(CelestialApparentDiameter, jde_tt) == 32);
static_assert(offsetof(CelestialApparentDiameter, radius_km) == 40);
static_assert(offsetof(CelestialApparentDiameter, distance_au) == 48);
static_assert(offsetof(CelestialApparentDiameter, diameter_arcsec) == 56);

TEST(DiameterCAbi, AllTargetsFieldsUnitsAndSelectedModel) {
  using namespace astro;
  constexpr std::array radii { 695700.0, 1737.4, 2440.53, 6051.8, 3396.19, 71492.0, 60268.0, 25559.0, 24764.0, 1188.3 };
  constexpr std::array<uint32_t, 10> sources { 1, 2, 3, 3, 3, 3, 3, 3, 3, 4 };
  for (uint32_t target = 0; target < 10; ++target) {
    for (const uint32_t centre : { UINT32_C(0), UINT32_C(1) }) {
      const auto result = celestial_apparent_diameter(target, 2026, 11, 7, 0.0, 1, 2, centre, -33.85, 151.2, 1706.0);
      ASSERT_TRUE(result.valid);
      EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
      const auto site = centre == 0 ? std::nullopt : std::optional { observer::Observer {
        .location = { .latitude = toolbox::AngleDeg { -33.85 }, .longitude = toolbox::AngleDeg { 151.2 } },
        .elevation = toolbox::DistanceKm { 1.706 },
      } };
      const auto expected = apparent_diameter::query(
        calendar::Datetime { util::to_ymd(2026, 11, 7), 0.0 },
        julian_day::Scale::UT1,
        static_cast<ephemeris::Target>(target),
        site,
        delta_t::Model::ALGO2
      );
      EXPECT_EQ(result.target_code, target);
      EXPECT_EQ(result.centre_code, centre);
      EXPECT_EQ(result.source_code, sources.at(target));
      EXPECT_EQ(result.radius_definition_code, target == 0 ? UINT32_C(1) : target == 1 ? UINT32_C(2) : UINT32_C(3));
      EXPECT_EQ(result.delta_t_identity_code, UINT32_C(2));
      EXPECT_DOUBLE_EQ(result.radius_km, radii.at(target));
      EXPECT_DOUBLE_EQ(result.distance_au, expected.distance.au());
      EXPECT_DOUBLE_EQ(result.diameter_arcsec, expected.diameter.deg() * 3600.0);
      EXPECT_DOUBLE_EQ(result.jd_ut1, expected.times.jd_ut1);
      EXPECT_DOUBLE_EQ(result.jde_tt, expected.times.jde_tt);
    }
  }
}

TEST(DiameterCAbi, GeocentricIgnoresSiteStorageAndSelectorErrorsRecover) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const auto ignored = celestial_apparent_diameter(1, 2026, 11, 7, 0.0, 0, 0, 0, nan, nan, nan);
  ASSERT_TRUE(ignored.valid);
  EXPECT_EQ(ignored.delta_t_identity_code, UINT32_C(5));
  for (const uint32_t bad : { UINT32_C(99), UINT32_C(256), UINT32_MAX }) {
    const std::array invalid {
      celestial_apparent_diameter(bad, 2026, 11, 7, 0.0, 0, 0, 0, 0.0, 0.0, 0.0),
      celestial_apparent_diameter(1, 2026, 11, 7, 0.0, bad, 0, 0, 0.0, 0.0, 0.0),
      celestial_apparent_diameter(1, 2026, 11, 7, 0.0, 0, bad, 0, 0.0, 0.0, 0.0),
      celestial_apparent_diameter(1, 2026, 11, 7, 0.0, 0, 0, bad, 0.0, 0.0, 0.0),
    };
    for (const auto& result : invalid) { EXPECT_FALSE(result.valid); }
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    ASSERT_TRUE(celestial_apparent_diameter(1, 2026, 11, 7, 0.0, 0, 0, 0, nan, nan, nan).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  }
  for (uint32_t node = 10; node <= 13; ++node) {
    EXPECT_FALSE(celestial_apparent_diameter(node, 2026, 11, 7, 0.0, 0, 0, 0, nan, nan, nan).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  EXPECT_FALSE(celestial_apparent_diameter(1, 2026, 11, 7, 0.0, 0, 0, 1, nan, nan, nan).valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_FALSE(celestial_apparent_diameter(9, 2100, 1, 1, 0.0, 0, 0, 0, nan, nan, nan).valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  ASSERT_TRUE(celestial_apparent_diameter(0, 1000, 1, 1, 0.0, 1, 0, 0, nan, nan, nan).valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
}

TEST(DiameterCAbi, OperationWideCapabilityOnly) {
  const auto size = celestial_capability_text(8, CELESTIAL_TARGET_ALL, nullptr, 0);
  EXPECT_GT(size, UINT32_C(0));
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  EXPECT_EQ(celestial_capability_text(8, CELESTIAL_TARGET_MOON, nullptr, 0), UINT32_C(0));
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
}

} // namespace lib::diameter::test
