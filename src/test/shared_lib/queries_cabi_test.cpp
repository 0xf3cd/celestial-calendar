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
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "celestial.h"

#include "chart.hpp"
#include "delta_t.hpp"
#include "house.hpp"
#include "lunar_node.hpp"
#include "moon.hpp"
#include "planet.hpp"
#include "sun.hpp"

namespace test {
namespace {

/** @brief Pin the selected providers without deriving source identities from core ordinals. */
[[nodiscard]] auto expected_source(const uint32_t target) -> uint32_t {
  switch (target) {
    case CELESTIAL_TARGET_SUN: return CELESTIAL_POSITION_SOURCE_SUN_VSOP87D;
    case CELESTIAL_TARGET_MOON: return CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC;
    case CELESTIAL_TARGET_PLUTO: return CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT;
    case CELESTIAL_TARGET_MEAN_ASCENDING:
    case CELESTIAL_TARGET_MEAN_DESCENDING: return CELESTIAL_POSITION_SOURCE_NODE_MEAN;
    case CELESTIAL_TARGET_TRUE_ASCENDING:
    case CELESTIAL_TARGET_TRUE_DESCENDING: return CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM;
    case CELESTIAL_TARGET_MERCURY:
    case CELESTIAL_TARGET_VENUS:
    case CELESTIAL_TARGET_MARS:
    case CELESTIAL_TARGET_JUPITER:
    case CELESTIAL_TARGET_SATURN:
    case CELESTIAL_TARGET_URANUS:
    case CELESTIAL_TARGET_NEPTUNE: return CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D;
    default: throw std::logic_error { "Unexpected target in position parity test" };
  }
}

} // namespace

// #355: the independent queries across the C boundary. Position selection follows the caller's
// order with duplicates and empty selections rejected; houses carry the resolved identities and
// no snapshot joint window.

TEST(QueriesCAbi, PositionQueryParityWithTheCore) {
  for (const double jde_tt : { 2451545.0, 2460800.25 }) {
    const std::array<uint32_t, 5> selection { CELESTIAL_TARGET_PLUTO, CELESTIAL_TARGET_SUN,
                                              CELESTIAL_TARGET_TRUE_ASCENDING, CELESTIAL_TARGET_MOON, CELESTIAL_TARGET_MARS };
    uint32_t count = 0;
    std::array<CelestialPositionRecord, 5> records {};
    EXPECT_EQ(celestial_positions(selection.data(), selection.size(), jde_tt, &count, records.data(), records.size()), 5);
    EXPECT_EQ(count, 5);

    for (std::size_t index = 0; index < records.size(); ++index) {
      EXPECT_EQ(records.at(index).target_code, selection.at(index));
      EXPECT_EQ(records.at(index).source_code, expected_source(selection.at(index)));
    }

    const std::array coordinates {
      astro::planet::geocentric_coord::apparent(astro::planet::Planet::PLUTO, jde_tt),
      astro::sun::geocentric_coord::apparent(jde_tt),
      astro::moon::geocentric_coord::apparent(jde_tt),
      astro::planet::geocentric_coord::apparent(astro::planet::Planet::MARS, jde_tt),
    };
    constexpr std::array<std::size_t, 4> PHYSICAL_SLOTS { 0, 1, 3, 4 };
    for (std::size_t index = 0; index < coordinates.size(); ++index) {
      const auto& record = records.at(PHYSICAL_SLOTS.at(index));
      const auto& coordinate = coordinates.at(index);
      EXPECT_DOUBLE_EQ(record.longitude_deg, coordinate.λ.deg());
      EXPECT_DOUBLE_EQ(record.latitude_deg, coordinate.β.deg());
      EXPECT_EQ(record.present_fields & CELESTIAL_PRESENT_DISTANCE, CELESTIAL_PRESENT_DISTANCE);
      EXPECT_DOUBLE_EQ(record.distance_au, coordinate.r.au());
    }

    const auto& node = records.at(2);
    EXPECT_DOUBLE_EQ(node.longitude_deg,
      astro::lunar_node::position(astro::lunar_node::Node::TRUE_ASCENDING, jde_tt).deg());
    EXPECT_DOUBLE_EQ(node.latitude_deg, 0.0);
    EXPECT_EQ(node.present_fields & CELESTIAL_PRESENT_DISTANCE, 0U);

    // Output order is the caller's selection order, not the declaration order.
    EXPECT_EQ(records[0].target_code, CELESTIAL_TARGET_PLUTO);
    EXPECT_EQ(records[1].target_code, CELESTIAL_TARGET_SUN);
  }
}

TEST(QueriesCAbi, PositionQueryAtProviderBoundaryAndSubsetSuccess) {
  // Pluto at its apparent window start (1885-01-02 0h TT, 2409543.5): below the snapshot
  // joint window and any rate center, a pure position query succeeds (#355 acceptance: a
  // subset succeeds where the complete snapshot rejects an excluded target's window, with no
  // rate stencil).
  const std::array<uint32_t, 2> selection { CELESTIAL_TARGET_PLUTO, CELESTIAL_TARGET_SUN };
  uint32_t count = 0;
  std::array<CelestialPositionRecord, 2> records {};
  EXPECT_EQ(celestial_positions(selection.data(), selection.size(), 2409543.5, &count, records.data(), records.size()), 2);
  EXPECT_EQ(count, 2);

  // Nodes are symmetric: the node provider window starts at 2409542.5 (1885-01-01 0h TT),
  // below the node rate center, and a node-only query succeeds there.
  const std::array<uint32_t, 1> node { CELESTIAL_TARGET_MEAN_ASCENDING };
  EXPECT_EQ(celestial_positions(node.data(), 1, 2409542.5, &count, records.data(), 1), 1);
  EXPECT_EQ(count, 1);
  EXPECT_FALSE((records[0].present_fields & CELESTIAL_PRESENT_DISTANCE) != 0);

  // At one resolved instant below the snapshot joint window, a subset without Pluto succeeds
  // while a complete snapshot at the same civil instant rejects the query through the joint
  // window.
  const auto early_house = celestial_house_query(1885, 1, 2, 0.0, CELESTIAL_CIVIL_SCALE_UT1, CELESTIAL_DELTA_T_MODEL_DEFAULT,
                                          CELESTIAL_NUTATION_MODEL_DEFAULT, 51.5, 0.0, CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN);
  ASSERT_TRUE(early_house.valid);
  ASSERT_LT(early_house.jde_tt, 2409543.875);
  const std::array<uint32_t, 2> without_pluto { CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_MOON };
  EXPECT_EQ(celestial_positions(without_pluto.data(), without_pluto.size(), early_house.jde_tt, &count,
                              records.data(), records.size()),
            2);
  EXPECT_EQ(count, 2);
  const auto snapshot = celestial_chart_snapshot(1885, 1, 2, 0.0, CELESTIAL_CIVIL_SCALE_UT1, 51.5, 0.0,
                                          CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN, CELESTIAL_DELTA_T_MODEL_DEFAULT);
  EXPECT_FALSE(snapshot.valid);

  // Out of the provider window the whole query fails with no partial list.
  count = 99;
  EXPECT_EQ(celestial_positions(selection.data(), selection.size(), 2409543.4, &count, records.data(), records.size()), 0);
  EXPECT_EQ(count, 0);
  EXPECT_STRNE(celestial_last_error(), "");
}

TEST(QueriesCAbi, PositionQuerySelectionRules) {
  const std::array<uint32_t, 1> single { CELESTIAL_TARGET_SUN };
  uint32_t count = 0;
  CelestialPositionRecord record {};

  // Empty selection is invalid; duplicates are rejected; unknown codes are rejected.
  EXPECT_EQ(celestial_positions(single.data(), 0, 2451545.0, &count, &record, 1), 0);
  EXPECT_EQ(count, 0);
  const std::array<uint32_t, 2> duplicated { CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_SUN };
  EXPECT_EQ(celestial_positions(duplicated.data(), 2, 2451545.0, &count, &record, 1), 0);
  EXPECT_EQ(count, 0);
  constexpr std::array<uint32_t, astro::ephemeris::TARGETS.size() + 1> oversized {};
  count = 99;
  record.longitude_deg = -777.0;
  EXPECT_EQ(celestial_positions(oversized.data(), oversized.size(), 2451545.0, &count, &record, 1), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_NE(std::string { celestial_last_error() }.find("exceeds the target roster"), std::string::npos);
  EXPECT_EQ(record.longitude_deg, -777.0);

  const std::array<uint32_t, 1> invalid { 14 };
  EXPECT_EQ(celestial_positions(invalid.data(), 1, 2451545.0, &count, &record, 1), 0);
  EXPECT_EQ(count, 0);
  EXPECT_EQ(celestial_positions(nullptr, 1, 2451545.0, &count, &record, 1), 0);
  EXPECT_EQ(celestial_positions(single.data(), 1, std::numeric_limits<double>::quiet_NaN(), &count, &record, 1), 0);

  // Count-only pass and truncation.
  EXPECT_EQ(celestial_positions(single.data(), 1, 2451545.0, &count, nullptr, 0), 0);
  EXPECT_EQ(count, 1);
}

TEST(QueriesCAbi, HouseQueryMatchesSnapshotHouses) {
  for (const uint32_t scale : { CELESTIAL_CIVIL_SCALE_UTC, CELESTIAL_CIVIL_SCALE_UT1 }) {
    const auto snapshot = celestial_chart_snapshot(2024, 6, 1, 0.5, scale, 51.5, 0.0,
                                            CELESTIAL_HOUSE_SYSTEM_PLACIDUS, CELESTIAL_DELTA_T_MODEL_DEFAULT);
    ASSERT_TRUE(snapshot.valid);

    const auto query = celestial_house_query(2024, 6, 1, 0.5, scale, CELESTIAL_DELTA_T_MODEL_DEFAULT,
                                      CELESTIAL_NUTATION_MODEL_DEFAULT, 51.5, 0.0, CELESTIAL_HOUSE_SYSTEM_PLACIDUS);
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
  const auto query = celestial_house_query(2024, 6, 1, 0.5, CELESTIAL_CIVIL_SCALE_UTC, CELESTIAL_DELTA_T_MODEL_DEFAULT,
                                    CELESTIAL_NUTATION_MODEL_DEFAULT, 51.5, 0.0, CELESTIAL_HOUSE_SYSTEM_PLACIDUS);
  ASSERT_TRUE(query.valid);
  EXPECT_EQ(query.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO5);
  EXPECT_EQ(query.nutation_identity_code, CELESTIAL_NUTATION_MODEL_IAU_1980);
  EXPECT_EQ(query.house_system_code, CELESTIAL_HOUSE_SYSTEM_PLACIDUS);
}

TEST(QueriesCAbi, HouseQueryProjectsNonDefaultDeltaTIdentity) {
  // Explicit selectors and resolved identities use the same named values; DEFAULT is input-only.
  const auto query = celestial_house_query(2024, 6, 1, 0.5, CELESTIAL_CIVIL_SCALE_UTC, CELESTIAL_DELTA_T_MODEL_ALGO3,
                                    CELESTIAL_NUTATION_MODEL_MEEUS, 51.5, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL);
  ASSERT_TRUE(query.valid);
  EXPECT_EQ(query.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO3);
  EXPECT_EQ(query.nutation_identity_code, CELESTIAL_NUTATION_MODEL_MEEUS);
}

TEST(QueriesCAbi, HouseQuerySkipsTheSnapshotWindow) {
  // 1885-01-01 is inside the civil validation but far below the snapshot joint window; the
  // house query resolves it, a complete snapshot does not.
  const auto query = celestial_house_query(1885, 1, 1, 0.0, CELESTIAL_CIVIL_SCALE_UT1, CELESTIAL_DELTA_T_MODEL_ALGO5,
                                    CELESTIAL_NUTATION_MODEL_MEEUS, 51.5, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL);
  ASSERT_TRUE(query.valid);
  EXPECT_LT(query.jde_tt, 2409543.875);
  EXPECT_EQ(query.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO5);
  EXPECT_EQ(query.nutation_identity_code, CELESTIAL_NUTATION_MODEL_MEEUS);
  EXPECT_STREQ(celestial_last_error(), "");

  const auto snapshot = celestial_chart_snapshot(1885, 1, 1, 0.0, CELESTIAL_CIVIL_SCALE_UT1, 51.5, 0.0,
                                          CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_ALGO5);
  EXPECT_FALSE(snapshot.valid);
}

TEST(QueriesCAbi, PositionSucceedsWhenHouseQueryFails) {
  // High-latitude Placidus rejects polar geometry; the position query is independent (#355).
  const auto failed_house = celestial_house_query(2024, 6, 1, 0.5, CELESTIAL_CIVIL_SCALE_UTC, CELESTIAL_DELTA_T_MODEL_DEFAULT,
                                           CELESTIAL_NUTATION_MODEL_DEFAULT, 89.9, 0.0, CELESTIAL_HOUSE_SYSTEM_PLACIDUS);
  EXPECT_FALSE(failed_house.valid);
  EXPECT_STRNE(celestial_last_error(), "");

  const std::array<uint32_t, 1> selection { CELESTIAL_TARGET_MARS };
  uint32_t count = 0;
  CelestialPositionRecord record {};
  EXPECT_EQ(celestial_positions(selection.data(), 1, 2451545.0, &count, &record, 1), 1);
  EXPECT_EQ(count, 1);
}

TEST(QueriesCAbi, HouseQueryRejectsUnknownCodes) {
  for (const uint32_t code : {
    uint32_t { 9 }, uint32_t { 256 }, uint32_t { 257 }, uint32_t { 65536 }, uint32_t { 65537 },
    uint32_t { 16777216 }, uint32_t { 16777217 }, uint32_t { 0x10000001 }, std::numeric_limits<uint32_t>::max()
  }) {
    const auto bad_delta_t = celestial_house_query(
      2024, 6, 1, 0.5, CELESTIAL_CIVIL_SCALE_UTC, code,
      CELESTIAL_NUTATION_MODEL_DEFAULT, 51.5, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL
    );
    EXPECT_FALSE(bad_delta_t.valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    const auto bad_nutation = celestial_house_query(
      2024, 6, 1, 0.5, CELESTIAL_CIVIL_SCALE_UTC, CELESTIAL_DELTA_T_MODEL_DEFAULT,
      code, 51.5, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL
    );
    EXPECT_FALSE(bad_nutation.valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

TEST(QueriesCAbi, PositionQueryCountFillAndFailureOwnership) {
  const std::array<uint32_t, 3> selection { CELESTIAL_TARGET_MOON, CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_MARS };
  uint32_t count = 99;
  EXPECT_EQ(celestial_positions(selection.data(), selection.size(), 2451545.0, &count, nullptr, 0), 0U);
  EXPECT_EQ(count, selection.size());
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  std::array<CelestialPositionRecord, 3> records {};
  records.at(1).longitude_deg = -777.0;
  EXPECT_EQ(celestial_positions(selection.data(), selection.size(), 2451545.0, &count, records.data(), 1), 1U);
  EXPECT_EQ(count, selection.size());
  EXPECT_EQ(records.at(0).target_code, selection.at(0));
  EXPECT_EQ(records.at(0).source_code, expected_source(selection.at(0)));
  EXPECT_EQ(records.at(1).longitude_deg, -777.0);

  const std::array<uint32_t, 2> partially_invalid { CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_PLUTO };
  for (auto& record : records) {
    record.longitude_deg = -777.0;
    record.target_code = UINT32_MAX;
    record.source_code = UINT32_MAX;
  }
  count = 99;
  EXPECT_EQ(celestial_positions(partially_invalid.data(), partially_invalid.size(), 2409543.4, &count,
    records.data(), records.size()), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  for (const auto& record : records) {
    EXPECT_EQ(record.longitude_deg, -777.0);
    EXPECT_EQ(record.target_code, UINT32_MAX);
    EXPECT_EQ(record.source_code, UINT32_MAX);
  }

  for (const uint32_t code : { 14U, 256U, 257U, 65536U, UINT32_C(0x10000001), UINT32_MAX }) {
    count = 99;
    EXPECT_EQ(celestial_positions(&code, 1, 2451545.0, &count, records.data(), records.size()), 0U);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  EXPECT_EQ(celestial_positions(selection.data(), selection.size(), 2451545.0, &count, nullptr, 1), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_positions(selection.data(), selection.size(), 2451545.0, nullptr, records.data(), 1), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
}

TEST(QueriesCAbi, CivilHouseCapabilityPublishesOnlyItsOwnPolicy) {
  for (const uint32_t model : { CELESTIAL_DELTA_T_MODEL_DEFAULT, CELESTIAL_DELTA_T_MODEL_ALGO1,
    CELESTIAL_DELTA_T_MODEL_ALGO2, CELESTIAL_DELTA_T_MODEL_ALGO3, CELESTIAL_DELTA_T_MODEL_ALGO4, CELESTIAL_DELTA_T_MODEL_ALGO5 }) {
    const auto capability = celestial_house_capability(model);
    ASSERT_TRUE(capability.valid);
    EXPECT_EQ(capability.status_code, CELESTIAL_STATUS_IMPLEMENTATION_WINDOW);
    EXPECT_EQ(capability.civil_start_year, 1885);
    EXPECT_EQ(capability.civil_end_year_exclusive, 2100);
    EXPECT_EQ(capability.civil_scale_mask, (1U << CELESTIAL_CIVIL_SCALE_UTC) | (1U << CELESTIAL_CIVIL_SCALE_UT1));
    EXPECT_EQ(capability.utc_start_year, 1972);
    EXPECT_EQ(capability.utc_start_month, 1U);
    EXPECT_EQ(capability.utc_start_day, 1U);
    EXPECT_EQ(capability.delta_t_identity_code, model == CELESTIAL_DELTA_T_MODEL_DEFAULT ? CELESTIAL_DELTA_T_MODEL_ALGO5 : model);
    EXPECT_EQ(capability.has_derived_tt_cutoff, model == CELESTIAL_DELTA_T_MODEL_ALGO4);
    if (capability.has_derived_tt_cutoff) {
      EXPECT_EQ(capability.derived_tt_end_jde_tt, astro::delta_t::ALGO4_END_JDE_TT);
    }
  }
  for (const uint32_t model : { 6U, 256U, 257U, 65537U, UINT32_C(0x10000005), UINT32_MAX }) {
    EXPECT_FALSE(celestial_house_capability(model).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

TEST(QueriesCAbi, CivilHousePolicyYearAndUtcBoundaries) {
  for (const int32_t year : { INT32_C(1884), INT32_C(2100) }) {
    EXPECT_FALSE(celestial_house_query(year, 1, 1, 0.0, CELESTIAL_CIVIL_SCALE_UT1,
      CELESTIAL_DELTA_T_MODEL_DEFAULT, CELESTIAL_NUTATION_MODEL_DEFAULT, 0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  EXPECT_TRUE(celestial_house_query(1885, 1, 1, 0.0, CELESTIAL_CIVIL_SCALE_UT1,
    CELESTIAL_DELTA_T_MODEL_DEFAULT, CELESTIAL_NUTATION_MODEL_DEFAULT, 0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
  EXPECT_TRUE(celestial_house_query(2099, 12, 31, 0.5, CELESTIAL_CIVIL_SCALE_UT1,
    CELESTIAL_DELTA_T_MODEL_DEFAULT, CELESTIAL_NUTATION_MODEL_DEFAULT, 0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
  EXPECT_FALSE(celestial_house_query(1971, 12, 31, 0.5, CELESTIAL_CIVIL_SCALE_UTC,
    CELESTIAL_DELTA_T_MODEL_DEFAULT, CELESTIAL_NUTATION_MODEL_DEFAULT, 0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_TRUE(celestial_house_query(1972, 1, 1, 0.0, CELESTIAL_CIVIL_SCALE_UTC,
    CELESTIAL_DELTA_T_MODEL_DEFAULT, CELESTIAL_NUTATION_MODEL_DEFAULT, 0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
}

TEST(QueriesCAbi, HouseAlgo4UsesTheSharedDerivedTtCutoff) {
  for (const uint32_t scale : { CELESTIAL_CIVIL_SCALE_UTC, CELESTIAL_CIVIL_SCALE_UT1 }) {
    // The UTC points replay chart_cabi_test; the UT1 points bracket its modelled TT year end.
    const auto fractions = scale == CELESTIAL_CIVIL_SCALE_UTC
      ? std::array { 0.99919925902642359, 0.99919925925925923 }
      : std::array { 0.99, 0.999999 };
    const auto admitted = celestial_house_query(2034, 12, 31, fractions.at(0), scale,
      CELESTIAL_DELTA_T_MODEL_ALGO4, CELESTIAL_NUTATION_MODEL_DEFAULT, 0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL);
    ASSERT_TRUE(admitted.valid) << celestial_last_error();
    EXPECT_LT(admitted.jde_tt, astro::delta_t::ALGO4_END_JDE_TT);
    EXPECT_EQ(admitted.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO4);
    const auto snapshot = celestial_chart_snapshot(2034, 12, 31, fractions.at(0), scale,
      0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_ALGO4);
    ASSERT_TRUE(snapshot.valid) << celestial_last_error();
    EXPECT_EQ(admitted.jd_ut1, snapshot.jd_ut1);
    EXPECT_EQ(admitted.jde_tt, snapshot.jde_tt);
    EXPECT_EQ(admitted.houses.ascendant_deg, snapshot.houses.ascendant_deg);
    EXPECT_EQ(admitted.houses.midheaven_deg, snapshot.houses.midheaven_deg);

    const auto rejected = celestial_house_query(2034, 12, 31, fractions.at(1), scale,
      CELESTIAL_DELTA_T_MODEL_ALGO4, CELESTIAL_NUTATION_MODEL_DEFAULT, 0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL);
    EXPECT_FALSE(rejected.valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_NE(std::string { celestial_last_error() }.find("Civil Delta T requires JDE(TT) before 2464328.5"),
              std::string::npos);
    EXPECT_EQ(std::string { celestial_last_error() }.find("Chart"), std::string::npos);
    EXPECT_FALSE(celestial_chart_snapshot(2034, 12, 31, fractions.at(1), scale,
      0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_ALGO4).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_TRUE(celestial_house_query(2034, 12, 31, fractions.at(1), scale,
      CELESTIAL_DELTA_T_MODEL_ALGO5, CELESTIAL_NUTATION_MODEL_DEFAULT, 0.0, 0.0, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
  }
}

} // namespace test
