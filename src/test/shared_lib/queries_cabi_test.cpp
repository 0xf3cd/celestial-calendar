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
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "celestial.h"

#include "capability.hpp"
#include "chart.hpp"
#include "house.hpp"

namespace test {

// #355: the independent queries across the C boundary. Position selection follows the caller's
// order with duplicates and empty selections rejected; houses carry the resolved identities and
// no snapshot joint window.

TEST(QueriesCAbi, PositionQueryParityWithTheCore) {
  for (const double jde_tt : { 2451545.0, 2460800.25 }) {
    const std::array<uint32_t, 5> selection { CHART_TARGET_PLUTO, CHART_TARGET_SUN,
                                              CHART_TARGET_TRUE_ASCENDING, CHART_TARGET_MOON, CHART_TARGET_MARS };
    uint32_t count = 0;
    std::array<PositionRecordV1, 5> records {};
    EXPECT_EQ(position_query_v1(selection.data(), selection.size(), jde_tt, &count, records.data(), records.size()), 5);
    EXPECT_EQ(count, 5);

    for (const auto& record : records) {
      const auto coordinate = astro::chart::position(static_cast<astro::ephemeris::Target>(record.target_code), jde_tt);
      EXPECT_DOUBLE_EQ(record.longitude_deg, coordinate.longitude.deg());
      EXPECT_DOUBLE_EQ(record.latitude_deg, coordinate.latitude.deg());
      ASSERT_EQ((record.present_fields & POSITION_PRESENT_DISTANCE) != 0, coordinate.distance.has_value());
      if (coordinate.distance.has_value()) {
        EXPECT_DOUBLE_EQ(record.distance_au, coordinate.distance->au());
      }
      EXPECT_EQ(record.source_code, static_cast<uint32_t>(astro::capability::position_source_of(
                static_cast<astro::ephemeris::Target>(record.target_code))));
    }

    // Output order is the caller's selection order, not the declaration order.
    EXPECT_EQ(records[0].target_code, CHART_TARGET_PLUTO);
    EXPECT_EQ(records[1].target_code, CHART_TARGET_SUN);
  }
}

TEST(QueriesCAbi, PositionQueryAtProviderBoundaryAndSubsetSuccess) {
  // Pluto at its apparent window start (1885-01-02 0h TT, 2409543.5): below the snapshot
  // joint window and any rate center, a pure position query succeeds (#355 acceptance: a
  // subset succeeds where the complete snapshot rejects an excluded target's window, with no
  // rate stencil).
  const std::array<uint32_t, 2> selection { CHART_TARGET_PLUTO, CHART_TARGET_SUN };
  uint32_t count = 0;
  std::array<PositionRecordV1, 2> records {};
  EXPECT_EQ(position_query_v1(selection.data(), selection.size(), 2409543.5, &count, records.data(), records.size()), 2);
  EXPECT_EQ(count, 2);

  // Nodes are symmetric: the node provider window starts at 2409542.5 (1885-01-01 0h TT),
  // below the node rate center, and a node-only query succeeds there.
  const std::array<uint32_t, 1> node { CHART_TARGET_MEAN_ASCENDING };
  EXPECT_EQ(position_query_v1(node.data(), 1, 2409542.5, &count, records.data(), 1), 1);
  EXPECT_EQ(count, 1);
  EXPECT_FALSE((records[0].present_fields & POSITION_PRESENT_DISTANCE) != 0);

  // At one resolved instant below the snapshot joint window, a subset without Pluto succeeds
  // while a complete snapshot at the same civil instant rejects the query through the joint
  // window.
  const auto early_house = house_query_v1(1885, 1, 2, 0.0, CHART_CIVIL_SCALE_UT1, CHART_DELTA_T_MODEL_DEFAULT,
                                          HOUSE_NUTATION_MODEL_DEFAULT, 51.5, 0.0, CHART_HOUSE_SYSTEM_WHOLE_SIGN);
  ASSERT_TRUE(early_house.valid);
  ASSERT_LT(early_house.jde_tt, 2409543.875);
  const std::array<uint32_t, 2> without_pluto { CHART_TARGET_SUN, CHART_TARGET_MOON };
  EXPECT_EQ(position_query_v1(without_pluto.data(), without_pluto.size(), early_house.jde_tt, &count,
                              records.data(), records.size()),
            2);
  EXPECT_EQ(count, 2);
  const auto snapshot = chart_snapshot_v1(1885, 1, 2, 0.0, CHART_CIVIL_SCALE_UT1, 51.5, 0.0,
                                          CHART_HOUSE_SYSTEM_WHOLE_SIGN, CHART_DELTA_T_MODEL_DEFAULT);
  EXPECT_FALSE(snapshot.valid);

  // Out of the provider window the whole query fails with no partial list.
  count = 99;
  EXPECT_EQ(position_query_v1(selection.data(), selection.size(), 2409543.4, &count, records.data(), records.size()), 0);
  EXPECT_EQ(count, 0);
  EXPECT_STRNE(last_error(), "");
}

TEST(QueriesCAbi, PositionQuerySelectionRules) {
  const std::array<uint32_t, 1> single { CHART_TARGET_SUN };
  uint32_t count = 0;
  PositionRecordV1 record {};

  // Empty selection is invalid; duplicates are rejected; unknown codes are rejected.
  EXPECT_EQ(position_query_v1(single.data(), 0, 2451545.0, &count, &record, 1), 0);
  EXPECT_EQ(count, 0);
  const std::array<uint32_t, 2> duplicated { CHART_TARGET_SUN, CHART_TARGET_SUN };
  EXPECT_EQ(position_query_v1(duplicated.data(), 2, 2451545.0, &count, &record, 1), 0);
  EXPECT_EQ(count, 0);
  const std::array<uint32_t, 1> invalid { 14 };
  EXPECT_EQ(position_query_v1(invalid.data(), 1, 2451545.0, &count, &record, 1), 0);
  EXPECT_EQ(count, 0);
  EXPECT_EQ(position_query_v1(nullptr, 1, 2451545.0, &count, &record, 1), 0);
  EXPECT_EQ(position_query_v1(single.data(), 1, std::numeric_limits<double>::quiet_NaN(), &count, &record, 1), 0);

  // Count-only pass and truncation.
  EXPECT_EQ(position_query_v1(single.data(), 1, 2451545.0, &count, nullptr, 0), 0);
  EXPECT_EQ(count, 1);
}

TEST(QueriesCAbi, HouseQueryMatchesSnapshotHouses) {
  for (const uint32_t scale : { CHART_CIVIL_SCALE_UTC, CHART_CIVIL_SCALE_UT1 }) {
    const auto snapshot = chart_snapshot_v1(2024, 6, 1, 0.5, scale, 51.5, 0.0,
                                            CHART_HOUSE_SYSTEM_PLACIDUS, CHART_DELTA_T_MODEL_DEFAULT);
    ASSERT_TRUE(snapshot.valid);

    const auto query = house_query_v1(2024, 6, 1, 0.5, scale, CHART_DELTA_T_MODEL_DEFAULT,
                                      HOUSE_NUTATION_MODEL_DEFAULT, 51.5, 0.0, CHART_HOUSE_SYSTEM_PLACIDUS);
    ASSERT_TRUE(query.valid);
    EXPECT_DOUBLE_EQ(query.jd_ut1, snapshot.jd_ut1);
    EXPECT_DOUBLE_EQ(query.jde_tt, snapshot.jde_tt);
    EXPECT_DOUBLE_EQ(query.houses.ascendant_deg, snapshot.houses.ascendant_deg);
    EXPECT_DOUBLE_EQ(query.houses.midheaven_deg, snapshot.houses.midheaven_deg);
    EXPECT_DOUBLE_EQ(query.houses.descendant_deg, snapshot.houses.descendant_deg);
    EXPECT_DOUBLE_EQ(query.houses.imum_coeli_deg, snapshot.houses.imum_coeli_deg);
    for (std::size_t index = 0; index < 12; ++index) {
      EXPECT_DOUBLE_EQ(std::span { query.houses.cusps_deg }[index], std::span { snapshot.houses.cusps_deg }[index]);
    }
  }

  // Resolved identities: DEFAULT is never recorded unresolved (#357).
  const auto query = house_query_v1(2024, 6, 1, 0.5, CHART_CIVIL_SCALE_UTC, CHART_DELTA_T_MODEL_DEFAULT,
                                    HOUSE_NUTATION_MODEL_DEFAULT, 51.5, 0.0, CHART_HOUSE_SYSTEM_PLACIDUS);
  ASSERT_TRUE(query.valid);
  EXPECT_EQ(query.delta_t_identity_code, MODEL_IDENTITY_DELTA_T_ALGO5);
  EXPECT_EQ(query.nutation_identity_code, MODEL_IDENTITY_NUTATION_IAU1980);
  EXPECT_EQ(query.house_system_code, CHART_HOUSE_SYSTEM_PLACIDUS);
}

TEST(QueriesCAbi, HouseQueryProjectsNonDefaultDeltaTIdentity) {
  // The resolved-identity fields project the selected model, not a hardcoded default: Algo3's
  // kind-internal ordinal is 2, distinct from the CHART_DELTA_T_MODEL_ALGO3 selector (3).
  const auto query = house_query_v1(2024, 6, 1, 0.5, CHART_CIVIL_SCALE_UTC, CHART_DELTA_T_MODEL_ALGO3,
                                    HOUSE_NUTATION_MODEL_MEEUS, 51.5, 0.0, CHART_HOUSE_SYSTEM_EQUAL);
  ASSERT_TRUE(query.valid);
  EXPECT_EQ(query.delta_t_identity_code, MODEL_IDENTITY_DELTA_T_ALGO3);
  EXPECT_EQ(query.nutation_identity_code, MODEL_IDENTITY_NUTATION_MEEUS);
}

TEST(QueriesCAbi, HouseQuerySkipsTheSnapshotWindow) {
  // 1885-01-01 is inside the civil validation but far below the snapshot joint window; the
  // house query resolves it, a complete snapshot does not.
  const auto query = house_query_v1(1885, 1, 1, 0.0, CHART_CIVIL_SCALE_UT1, CHART_DELTA_T_MODEL_ALGO5,
                                    HOUSE_NUTATION_MODEL_MEEUS, 51.5, 0.0, CHART_HOUSE_SYSTEM_EQUAL);
  ASSERT_TRUE(query.valid);
  EXPECT_LT(query.jde_tt, 2409543.875);
  EXPECT_EQ(query.delta_t_identity_code, MODEL_IDENTITY_DELTA_T_ALGO5);
  EXPECT_EQ(query.nutation_identity_code, MODEL_IDENTITY_NUTATION_MEEUS);
  EXPECT_STREQ(last_error(), "");

  const auto snapshot = chart_snapshot_v1(1885, 1, 1, 0.0, CHART_CIVIL_SCALE_UT1, 51.5, 0.0,
                                          CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_ALGO5);
  EXPECT_FALSE(snapshot.valid);
}

TEST(QueriesCAbi, PositionSucceedsWhenHouseQueryFails) {
  // High-latitude Placidus rejects polar geometry; the position query is independent (#355).
  const auto failed_house = house_query_v1(2024, 6, 1, 0.5, CHART_CIVIL_SCALE_UTC, CHART_DELTA_T_MODEL_DEFAULT,
                                           HOUSE_NUTATION_MODEL_DEFAULT, 89.9, 0.0, CHART_HOUSE_SYSTEM_PLACIDUS);
  EXPECT_FALSE(failed_house.valid);
  EXPECT_STRNE(last_error(), "");

  const std::array<uint32_t, 1> selection { CHART_TARGET_MARS };
  uint32_t count = 0;
  PositionRecordV1 record {};
  EXPECT_EQ(position_query_v1(selection.data(), 1, 2451545.0, &count, &record, 1), 1);
  EXPECT_EQ(count, 1);
}

TEST(QueriesCAbi, HouseQueryRejectsUnknownCodes) {
  const auto bad_delta_t = house_query_v1(2024, 6, 1, 0.5, CHART_CIVIL_SCALE_UTC, 9,
                                          HOUSE_NUTATION_MODEL_DEFAULT, 51.5, 0.0, CHART_HOUSE_SYSTEM_EQUAL);
  EXPECT_FALSE(bad_delta_t.valid);
  EXPECT_STRNE(last_error(), "");
  const auto bad_nutation = house_query_v1(2024, 6, 1, 0.5, CHART_CIVIL_SCALE_UTC, CHART_DELTA_T_MODEL_DEFAULT,
                                           9, 51.5, 0.0, CHART_HOUSE_SYSTEM_EQUAL);
  EXPECT_FALSE(bad_nutation.valid);
}

} // namespace test
