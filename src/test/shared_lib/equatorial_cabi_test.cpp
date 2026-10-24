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
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include <gtest/gtest.h>

#include "celestial.h"
#include "ephemeris.hpp"
#include "moon.hpp"
#include "sun.hpp"

namespace lib::equatorial::test {
namespace {

constexpr std::array TARGETS {
  CELESTIAL_TARGET_TRUE_DESCENDING, CELESTIAL_TARGET_TRUE_ASCENDING,
  CELESTIAL_TARGET_MEAN_DESCENDING, CELESTIAL_TARGET_MEAN_ASCENDING,
  CELESTIAL_TARGET_PLUTO, CELESTIAL_TARGET_NEPTUNE, CELESTIAL_TARGET_URANUS,
  CELESTIAL_TARGET_SATURN, CELESTIAL_TARGET_JUPITER, CELESTIAL_TARGET_MARS,
  CELESTIAL_TARGET_VENUS, CELESTIAL_TARGET_MERCURY, CELESTIAL_TARGET_MOON, CELESTIAL_TARGET_SUN,
};
constexpr std::array SOURCES {
  CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM, CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM,
  CELESTIAL_POSITION_SOURCE_NODE_MEAN, CELESTIAL_POSITION_SOURCE_NODE_MEAN,
  CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC, CELESTIAL_POSITION_SOURCE_SUN_VSOP87D,
};

static_assert(std::is_standard_layout_v<CelestialEquatorialPositionRecord>);
static_assert(std::is_trivially_copyable_v<CelestialEquatorialPositionRecord>);
static_assert(std::is_same_v<decltype(&celestial_equatorial_positions),
  uint32_t (*)(const uint32_t*, uint32_t, double, uint32_t*, CelestialEquatorialPositionRecord*, uint32_t)>);

} // namespace

TEST(EquatorialCAbi, AllTargetsPreserveSelectionOrderSourcesAndDistancePresence) {
  for (const double jde_tt : { 2451545.0, 2460800.25 }) {
    uint32_t count = 99;
    std::array<CelestialEquatorialPositionRecord, TARGETS.size()> records {};
    ASSERT_EQ(celestial_equatorial_positions(TARGETS.data(), TARGETS.size(), jde_tt,
      &count, records.data(), records.size()), records.size());
    ASSERT_EQ(count, TARGETS.size());
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    for (std::size_t index = 0; index < records.size(); ++index) {
      const auto& record = records.at(index);
      EXPECT_EQ(record.target_code, TARGETS.at(index));
      EXPECT_EQ(record.source_code, SOURCES.at(index));
      EXPECT_TRUE(std::isfinite(record.right_ascension_deg));
      EXPECT_GE(record.right_ascension_deg, 0.0);
      EXPECT_LT(record.right_ascension_deg, 360.0);
      EXPECT_TRUE(std::isfinite(record.declination_deg));
      EXPECT_GE(record.declination_deg, -90.0);
      EXPECT_LE(record.declination_deg, 90.0);
      const bool has_distance = record.target_code <= CELESTIAL_TARGET_PLUTO;
      EXPECT_EQ(record.present_fields, has_distance ? CELESTIAL_PRESENT_DISTANCE : 0U);
      if (has_distance) {
        EXPECT_TRUE(std::isfinite(record.distance_au));
        EXPECT_GT(record.distance_au, 0.0);
      }
    }
  }
}

TEST(EquatorialCAbi, SunAndMoonExactlyMatchExistingCoreCoordinates) {
  const std::array selection { CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_MOON };
  for (const double jde_tt : { 2448724.5, 2451545.0, 2460659.5, 2460754.5, 2460755.5, 2460800.25 }) {
    uint32_t count = 0;
    std::array<CelestialEquatorialPositionRecord, 2> records {};
    ASSERT_EQ(celestial_equatorial_positions(selection.data(), selection.size(), jde_tt,
      &count, records.data(), records.size()), records.size());
    const auto sun = astro::sun::equatorial_coord::apparent(jde_tt);
    const auto moon = astro::moon::equatorial_coord::apparent(jde_tt);
    EXPECT_EQ(records.at(0).right_ascension_deg, sun.α.deg());
    EXPECT_EQ(records.at(0).declination_deg, sun.δ.deg());
    EXPECT_EQ(records.at(1).right_ascension_deg, moon.α.deg());
    EXPECT_EQ(records.at(1).declination_deg, moon.δ.deg());
    EXPECT_EQ(records.at(0).distance_au, astro::sun::geocentric_coord::apparent(jde_tt).r.au());
    EXPECT_EQ(records.at(1).distance_au, astro::moon::geocentric_coord::apparent(jde_tt).r.au());
  }
}

TEST(EquatorialCAbi, SunRightAscensionWrapsWithoutChangingItsRange) {
  const uint32_t sun = CELESTIAL_TARGET_SUN;
  uint32_t count = 0;
  CelestialEquatorialPositionRecord before {};
  CelestialEquatorialPositionRecord after {};
  ASSERT_EQ(celestial_equatorial_positions(&sun, 1, 2460754.5, &count, &before, 1), 1U);
  ASSERT_EQ(celestial_equatorial_positions(&sun, 1, 2460755.5, &count, &after, 1), 1U);
  EXPECT_GT(before.right_ascension_deg, after.right_ascension_deg);
  EXPECT_LT(before.right_ascension_deg, 360.0);
  EXPECT_GE(after.right_ascension_deg, 0.0);
}

TEST(EquatorialCAbi, CountOnlyTruncationAndCallerOwnership) {
  const std::array selection { CELESTIAL_TARGET_MOON, CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_MARS };
  uint32_t count = 99;
  EXPECT_EQ(celestial_equatorial_positions(selection.data(), selection.size(), 2451545.0, &count, nullptr, 0), 0U);
  ASSERT_EQ(count, selection.size());
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);

  std::array<CelestialEquatorialPositionRecord, 3> records {};
  records.at(1).right_ascension_deg = -777.0;
  ASSERT_EQ(celestial_equatorial_positions(selection.data(), selection.size(), 2451545.0,
    &count, records.data(), 1), 1U);
  EXPECT_EQ(count, selection.size());
  EXPECT_EQ(records.at(0).target_code, selection.at(0));
  EXPECT_EQ(records.at(1).right_ascension_deg, -777.0);
  const auto owned = records.at(0);
  CelestialEquatorialPositionRecord other {};
  EXPECT_EQ(celestial_equatorial_positions(selection.data(), 1, 2460800.25, &count, &other, 1), 1U);
  EXPECT_EQ(records.at(0).right_ascension_deg, owned.right_ascension_deg);
  EXPECT_EQ(records.at(0).declination_deg, owned.declination_deg);
  EXPECT_EQ(records.at(0).distance_au, owned.distance_au);
}

TEST(EquatorialCAbi, RejectsEmptyDuplicateUnknownAndMalformedSelections) {
  const uint32_t sun = CELESTIAL_TARGET_SUN;
  uint32_t count = 99;
  CelestialEquatorialPositionRecord record {};
  EXPECT_EQ(celestial_equatorial_positions(&sun, 0, 2451545.0, &count, &record, 1), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  const std::array duplicated { sun, sun };
  EXPECT_EQ(celestial_equatorial_positions(duplicated.data(), duplicated.size(), 2451545.0, &count, &record, 1), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  for (const uint32_t code : { 14U, 256U, 257U, 65536U, UINT32_C(0x10000001), UINT32_MAX }) {
    count = 99;
    EXPECT_EQ(celestial_equatorial_positions(&code, 1, 2451545.0, &count, &record, 1), 0U);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  EXPECT_EQ(celestial_equatorial_positions(nullptr, 1, 2451545.0, &count, &record, 1), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_equatorial_positions(&sun, 1, 2451545.0, &count, nullptr, 1), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_equatorial_positions(&sun, 1, 2451545.0, nullptr, &record, 1), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  for (const double epoch : { std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() }) {
    count = 99;
    EXPECT_EQ(celestial_equatorial_positions(&sun, 1, epoch, &count, &record, 1), 0U);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

TEST(EquatorialCAbi, FailedLaterTargetPublishesNoPartialSlotsOrCount) {
  const std::array selection { CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_PLUTO };
  std::array<CelestialEquatorialPositionRecord, 2> records {};
  for (auto& record : records) {
    record.target_code = UINT32_MAX;
    record.present_fields = UINT32_MAX;
    record.source_code = UINT32_MAX;
    record.right_ascension_deg = -777.0;
    record.declination_deg = -777.0;
    record.distance_au = -777.0;
  }
  uint32_t count = 99;
  EXPECT_EQ(celestial_equatorial_positions(selection.data(), selection.size(), 2409543.4,
    &count, records.data(), records.size()), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  for (const auto& record : records) {
    EXPECT_EQ(record.target_code, UINT32_MAX);
    EXPECT_EQ(record.present_fields, UINT32_MAX);
    EXPECT_EQ(record.source_code, UINT32_MAX);
    EXPECT_EQ(record.right_ascension_deg, -777.0);
    EXPECT_EQ(record.declination_deg, -777.0);
    EXPECT_EQ(record.distance_au, -777.0);
  }
  EXPECT_EQ(celestial_equatorial_positions(selection.data(), 1, 2409543.4, &count, records.data(), records.size()), 1U);
  EXPECT_EQ(count, 1U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
}

TEST(EquatorialCAbi, PositionAtProviderStartDoesNotEvaluateARateStencil) {
  const uint32_t pluto = CELESTIAL_TARGET_PLUTO;
  uint32_t count = 0;
  CelestialEquatorialPositionRecord record {};
  EXPECT_THROW(std::ignore = astro::ephemeris::longitude_rate(astro::ephemeris::Target::PLUTO, 2409543.5), std::invalid_argument);
  EXPECT_EQ(celestial_equatorial_positions(&pluto, 1, 2409543.5, &count, &record, 1), 1U);
  EXPECT_EQ(count, 1U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  const uint32_t node = CELESTIAL_TARGET_MEAN_ASCENDING;
  EXPECT_EQ(celestial_equatorial_positions(&node, 1, 2409542.5, &count, &record, 1), 1U);
  EXPECT_EQ(count, 1U);
  EXPECT_EQ(record.present_fields, 0U);
}

} // namespace lib::equatorial::test
