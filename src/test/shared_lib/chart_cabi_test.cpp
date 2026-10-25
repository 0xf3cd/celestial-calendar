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
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

#include <gtest/gtest.h>

#include "celestial.h"
#include "chart.hpp"
#include "../astro/chart_test_helper.hpp"
#include "../astro/house_test_helper.hpp"
#include "../test_helper.hpp"

namespace lib::chart::test {
namespace {

using astro::julian_day::Scale;
using astro::delta_t::Model;
using astro::ephemeris::Target;
using astro::house::System;
using astro::house::test::WITNESS_LATITUDE_DEG;
using astro::house::test::WITNESS_NORTH_LONGITUDE_DEG;
using astro::house::test::WITNESS_SOUTH_LONGITUDE_DEG;
using astro::toolbox::AngleDeg;
using calendar::Datetime;
using astro::chart::test::location;

constexpr std::array MODEL_CODES {
  CELESTIAL_DELTA_T_MODEL_ALGO1, CELESTIAL_DELTA_T_MODEL_ALGO2, CELESTIAL_DELTA_T_MODEL_ALGO3,
  CELESTIAL_DELTA_T_MODEL_ALGO4, CELESTIAL_DELTA_T_MODEL_ALGO5,
};
constexpr std::array MODELS { Model::ALGO1, Model::ALGO2, Model::ALGO3, Model::ALGO4, Model::ALGO5 };
constexpr std::array SYSTEM_CODES {
  CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN, CELESTIAL_HOUSE_SYSTEM_PLACIDUS,
};
constexpr std::array SYSTEMS { System::EQUAL, System::WHOLE_SIGN, System::PLACIDUS };
constexpr std::array TARGET_CODES {
  CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_MOON, CELESTIAL_TARGET_MERCURY, CELESTIAL_TARGET_VENUS,
  CELESTIAL_TARGET_MARS, CELESTIAL_TARGET_JUPITER, CELESTIAL_TARGET_SATURN, CELESTIAL_TARGET_URANUS,
  CELESTIAL_TARGET_NEPTUNE, CELESTIAL_TARGET_PLUTO, CELESTIAL_TARGET_MEAN_ASCENDING,
  CELESTIAL_TARGET_MEAN_DESCENDING, CELESTIAL_TARGET_TRUE_ASCENDING, CELESTIAL_TARGET_TRUE_DESCENDING,
};
constexpr std::array TARGETS {
  Target::SUN, Target::MOON, Target::MERCURY, Target::VENUS, Target::MARS, Target::JUPITER,
  Target::SATURN, Target::URANUS, Target::NEPTUNE, Target::PLUTO, Target::MEAN_ASCENDING,
  Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING,
};
static_assert(TARGETS == astro::ephemeris::TARGETS);
static_assert(TARGET_CODES.size() == TARGETS.size());
constexpr std::array SOURCE_CODES {
  CELESTIAL_POSITION_SOURCE_SUN_VSOP87D, CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT,
  CELESTIAL_POSITION_SOURCE_NODE_MEAN, CELESTIAL_POSITION_SOURCE_NODE_MEAN,
  CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM, CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM,
};
constexpr double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
constexpr double INFINITY_VALUE = std::numeric_limits<double>::infinity();

// The C ABI contract under test throughout this file: the returned struct equals what
// astro::chart::calculate returns for the same civil input. Domain rules, message wording,
// and physical rates are pinned in the C++ suites (chart_test, ephemeris_test, the house
// tests) — re-deriving them here would only duplicate those pins (#347).

auto code_index(const auto& enums, const decltype(enums.front()) value) -> std::size_t {
  return static_cast<std::size_t>(std::ranges::find(enums, value) - enums.begin());
}

// The core call's rejection is a thrown invalid_argument; the C boundary's is `valid = false`.
// Both faces of one contract, bridged here for the domain-parity tests.
[[nodiscard]] auto try_calculate(const Datetime& civil_dt, const Scale scale, const astro::GeoLocation& observer,
                                 const System system, const Model model) -> std::optional<astro::chart::Snapshot> {
  try {
    return astro::chart::calculate(civil_dt, scale, observer, system, model);
  } catch (const std::invalid_argument&) {
    return std::nullopt;
  }
}

static_assert(std::is_standard_layout_v<CelestialChartBody> and std::is_trivially_copyable_v<CelestialChartBody>);
static_assert(std::is_standard_layout_v<CelestialChartHouses> and std::is_trivially_copyable_v<CelestialChartHouses>);
static_assert(std::is_standard_layout_v<CelestialChartSnapshot> and std::is_trivially_copyable_v<CelestialChartSnapshot>);
static_assert(std::is_same_v<decltype(CelestialChartBody::target_code), uint32_t>);
static_assert(std::is_same_v<decltype(CelestialChartBody::present_fields), uint32_t>);
static_assert(std::is_same_v<decltype(&celestial_chart_snapshot),
                           CelestialChartSnapshot (*)(int32_t, uint32_t, uint32_t, double, uint32_t,
                                               double, double, uint32_t, uint32_t)>);
static_assert(std::extent_v<decltype(CelestialChartSnapshot::bodies)> == 14);
static_assert(std::extent_v<decltype(CelestialChartHouses::cusps_deg)> == 12);
static_assert(sizeof(CelestialChartBody) == 48 and alignof(CelestialChartBody) == 8);
static_assert(sizeof(CelestialChartHouses) == 128 and alignof(CelestialChartHouses) == 8);
static_assert(sizeof(CelestialChartSnapshot) == 832 and alignof(CelestialChartSnapshot) == 8);

auto snapshot_at(const Datetime& civil_dt, const uint32_t scale_code, const astro::GeoLocation& observer,
                 const uint32_t system_code, const uint32_t model_code = CELESTIAL_DELTA_T_MODEL_DEFAULT) -> CelestialChartSnapshot {
  const auto [year, month, day] = util::from_ymd(civil_dt.ymd);
  return celestial_chart_snapshot(
    year, month, day, civil_dt.fraction(), scale_code,
    observer.latitude.deg(), observer.longitude.deg(), system_code, model_code
  );
}

auto expect_snapshot(const CelestialChartSnapshot& actual, const astro::chart::Snapshot& expected) -> void {
  ASSERT_TRUE(actual.valid) << celestial_last_error();
  EXPECT_EQ(actual.jd_ut1, expected.times.jd_ut1);
  EXPECT_EQ(actual.jde_tt, expected.times.jde_tt);
  EXPECT_GE(actual.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO1);
  EXPECT_LE(actual.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO5);
  EXPECT_EQ(actual.nutation_identity_code, CELESTIAL_NUTATION_MODEL_IAU_1980);
  for (std::size_t index = 0; index < TARGETS.size(); ++index) {
    SCOPED_TRACE(index);
    const auto& body = std::span { actual.bodies }[index];
    const auto& state = expected.bodies.at(index);
    EXPECT_EQ(body.target_code, TARGET_CODES.at(index));
    EXPECT_EQ(body.source_code, SOURCE_CODES.at(index));
    EXPECT_EQ(state.target, TARGETS.at(index));
    EXPECT_EQ(body.present_fields, CELESTIAL_PRESENT_LATITUDE | (state.distance.has_value() ? CELESTIAL_PRESENT_DISTANCE : 0U));
    EXPECT_EQ(body.longitude_deg, state.longitude.deg());
    EXPECT_EQ(body.latitude_deg, state.latitude.deg());
    if (state.distance.has_value()) {
      EXPECT_EQ(body.distance_au, util::test::req(state.distance).au());
    }
    EXPECT_EQ(std::bit_cast<uint64_t>(body.longitude_rate_deg_per_tt_day),
              std::bit_cast<uint64_t>(state.longitude_rate.deg_per_tt_day));
  }
  EXPECT_EQ(actual.houses.ascendant_deg, expected.houses.ascendant.deg());
  EXPECT_EQ(actual.houses.midheaven_deg, expected.houses.midheaven.deg());
  EXPECT_EQ(actual.houses.descendant_deg, expected.houses.descendant.deg());
  EXPECT_EQ(actual.houses.imum_coeli_deg, expected.houses.imum_coeli.deg());
  for (std::size_t index = 0; index < expected.houses.cusps.size(); ++index) {
    EXPECT_EQ(std::span { actual.houses.cusps_deg }[index], expected.houses.cusps.at(index).deg());
  }
}


} // namespace

TEST(ChartCAbi, SelectedRolesMatchCoreAcrossEveryTarget) {
  const Datetime dt { util::to_ymd(2026, 1, 1), 0.5123456789 };
  for (const auto& row : astro::chart::test::COMPOSITION_ROLES) {
    SCOPED_TRACE(row.role);
    const auto actual = snapshot_at(
      dt,
      CELESTIAL_CIVIL_SCALE_UTC,
      row.observer,
      SYSTEM_CODES.at(code_index(SYSTEMS, row.system)),
      MODEL_CODES.at(code_index(MODELS, row.model))
    );
    ASSERT_TRUE(actual.valid) << celestial_last_error();
    EXPECT_STREQ(celestial_last_error(), "");
    EXPECT_EQ(actual.delta_t_identity_code, MODEL_CODES.at(code_index(MODELS, row.model)));
    expect_snapshot(actual, astro::chart::calculate(dt, Scale::UTC, row.observer, row.system, row.model));
  }
}

TEST(ChartCAbi, DefaultIsAlgo5AndNotAlgo1) {
  const Datetime dt { util::to_ymd(2026, 1, 1), 0.5 };
  const auto observer = location(40.0, -75.0);
  const auto default_result = snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UT1, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL);
  const auto algo5 = snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UT1, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_ALGO5);
  const auto algo1 = snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UT1, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_ALGO1);
  ASSERT_TRUE(default_result.valid);
  ASSERT_TRUE(algo5.valid);
  ASSERT_TRUE(algo1.valid);
  EXPECT_EQ(default_result.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO5);
  EXPECT_EQ(algo5.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO5);
  EXPECT_EQ(algo1.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO1);
  const auto expected = astro::chart::calculate(dt, Scale::UT1, observer, System::EQUAL, Model::ALGO5);
  expect_snapshot(default_result, expected);
  expect_snapshot(algo5, expected);
  EXPECT_NE(default_result.jde_tt, algo1.jde_tt);
  EXPECT_NE(default_result.bodies[0].longitude_deg, algo1.bodies[0].longitude_deg);
}

TEST(ChartCAbi, HistoricalUt1UsesSelectedModel) {
  const Datetime dt { util::to_ymd(1900, 6, 1), 0.5 };
  const auto observer = location(40.0, -75.0);
  for (std::size_t index = 0; index < MODELS.size(); ++index) {
    const auto actual = snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UT1, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL, MODEL_CODES.at(index));
    ASSERT_TRUE(actual.valid) << celestial_last_error();
    EXPECT_EQ(actual.delta_t_identity_code, MODEL_CODES.at(index));
    expect_snapshot(actual, astro::chart::calculate(dt, Scale::UT1, observer, System::EQUAL, MODELS.at(index)));
  }
  EXPECT_FALSE(snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UTC, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "1972"), nullptr);
}

TEST(ChartCAbi, UtcBeginsAt1972AndUsesLeapTable) {
  const auto observer = location(0.0, 0.0);
  const Datetime first { util::to_ymd(1972, 1, 1), 0.0 };
  const Datetime before { util::to_ymd(1971, 12, 31), 0.5 };
  ASSERT_FALSE(snapshot_at(before, CELESTIAL_CIVIL_SCALE_UTC, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "1972"), nullptr);
  const auto historical = snapshot_at(before, CELESTIAL_CIVIL_SCALE_UT1, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL);
  ASSERT_TRUE(historical.valid) << celestial_last_error();
  expect_snapshot(historical, astro::chart::calculate(before, Scale::UT1, observer, System::EQUAL, Model::ALGO5));

  const auto utc = snapshot_at(first, CELESTIAL_CIVIL_SCALE_UTC, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL);
  ASSERT_TRUE(utc.valid) << celestial_last_error();
  expect_snapshot(utc, astro::chart::calculate(first, Scale::UTC, observer, System::EQUAL, Model::ALGO5));
}

TEST(ChartCAbi, RejectsUnknownAndWideSelectorAliases) {
  for (const uint32_t code : std::array { 2U, 255U, 256U, 257U, UINT32_C(0x10000001), UINT32_MAX }) {
    EXPECT_FALSE(celestial_chart_snapshot(2026, 1, 1, 0.5, code, 0.0, 0.0,
                                   CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "civil_scale_code"), nullptr);
  }
  for (const uint32_t code : std::array { 3U, 255U, 256U, 257U, UINT32_C(0x10000002), UINT32_MAX }) {
    EXPECT_FALSE(celestial_chart_snapshot(2026, 1, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   code, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "house_system_code"), nullptr);
  }
  for (const uint32_t code : std::array { 6U, 255U, 256U, 257U, 261U, UINT32_C(0x10000005), UINT32_MAX }) {
    EXPECT_FALSE(celestial_chart_snapshot(2026, 1, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CELESTIAL_HOUSE_SYSTEM_EQUAL, code).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

TEST(ChartCAbi, RejectsYearsAtRepresentationAndAdmissionBoundaries) {
  for (const int32_t year : std::array { INT32_MIN, -63510, 67562, INT32_MAX }) {
    EXPECT_FALSE(celestial_chart_snapshot(year, 1, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "std::chrono::year"), nullptr);
  }

  for (const int32_t year : std::array { 1884, 2100 }) {
    EXPECT_FALSE(celestial_chart_snapshot(year, 1, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_NE(std::string { celestial_last_error() }.find(std::to_string(year)), std::string::npos);
  }
}

TEST(ChartCAbi, RejectsRawMonthAndDayBeforeChronoNarrowing) {
  for (const uint32_t month : std::array { 0U, 13U, 256U, 257U, 268U, 65537U, UINT32_MAX }) {
    EXPECT_FALSE(celestial_chart_snapshot(2026, month, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "month"), nullptr);
  }
  for (const uint32_t day : std::array { 0U, 32U, 256U, 257U, 287U, 65537U, UINT32_MAX }) {
    EXPECT_FALSE(celestial_chart_snapshot(2026, 1, day, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "day"), nullptr);
  }
  EXPECT_FALSE(celestial_chart_snapshot(1900, 2, 29, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                 CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_FALSE(celestial_chart_snapshot(2026, 4, 31, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                 CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_TRUE(celestial_chart_snapshot(2000, 2, 29, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_STREQ(celestial_last_error(), "");
}

TEST(ChartCAbi, RejectsNonfiniteAndOutOfRangeFractions) {
  for (const double fraction : std::array { NAN_VALUE, INFINITY_VALUE, -INFINITY_VALUE, -0.1, 1.0, 1e100 }) {
    EXPECT_FALSE(celestial_chart_snapshot(2026, 1, 1, fraction, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "fraction"), nullptr);
  }
  EXPECT_TRUE(celestial_chart_snapshot(2026, 1, 1, -0.0, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
}

TEST(ChartCAbi, StrictJointDomainIsCheckedAfterTimeResolution) {
  const auto observer = location(0.0, 0.0);
  for (const double edge : std::array { 2409543.875, 2488069.125 }) {
    for (const double seconds : std::array { -1.0, 0.0, 1.0 }) {
      SCOPED_TRACE(edge);
      SCOPED_TRACE(seconds);
      const auto tt = astro::julian_day::jde_to_tt(edge + (seconds / 86400.0));
      const auto ut1 = astro::delta_t::tt_to_ut1(tt);
      const Datetime input { ut1.ymd, ut1.fraction() };
      const auto actual = snapshot_at(input, CELESTIAL_CIVIL_SCALE_UT1, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL);

      // The admission rule itself is chart_test's pin; here the C layer must agree with the
      // core call on whether the recomposed instant is admitted, and pass its result through
      // unchanged when it is.
      const auto expected = try_calculate(input, Scale::UT1, observer, System::EQUAL, Model::ALGO5);
      ASSERT_EQ(actual.valid, expected.has_value()) << celestial_last_error();
      if (expected.has_value()) {
        expect_snapshot(actual, *expected);
      } else {
        EXPECT_NE(std::strstr(celestial_last_error(), "Chart JDE(TT)"), nullptr);
      }
    }
  }
}

TEST(ChartCAbi, Algo4CapIsStrictInDerivedTtAndHasNoDefaultFallback) {
  const auto observer = location(0.0, 0.0);
  // Boundary inputs replayed from chart_test.cpp; conversion wiring, not new physical goldens.
  for (const double fraction : std::array { 0.99919925902642359, 0.99919925925925923 }) {
    const Datetime dt { util::to_ymd(2034, 12, 31), fraction };
    const auto actual = celestial_chart_snapshot(2034, 12, 31, fraction, CELESTIAL_CIVIL_SCALE_UTC, 0.0, 0.0,
                                          CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_ALGO4);
    if (fraction == 0.99919925902642359) {
      ASSERT_TRUE(actual.valid) << celestial_last_error();
      expect_snapshot(actual, astro::chart::calculate(dt, Scale::UTC, observer, System::EQUAL, Model::ALGO4));
    } else {
      EXPECT_FALSE(actual.valid);
      EXPECT_NE(std::strstr(celestial_last_error(), "Civil Delta T"), nullptr);
      EXPECT_NE(std::strstr(celestial_last_error(), "2464328.5"), nullptr);
      EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
      EXPECT_TRUE(celestial_chart_snapshot(2034, 12, 31, fraction, CELESTIAL_CIVIL_SCALE_UTC, 0.0, 0.0,
                                    CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_ALGO5).valid);
    }
  }
  EXPECT_FALSE(celestial_chart_snapshot(2035, 6, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                 CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_ALGO4).valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_TRUE(celestial_chart_snapshot(2035, 6, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
}

TEST(ChartCAbi, LocationRequiresOpenLatitudeAndClosedLongitudeIntervals) {
  const Datetime dt { util::to_ymd(2026, 1, 1), 0.5 };
  for (const auto observer : std::array {
    location(NAN_VALUE, 0.0), location(INFINITY_VALUE, 0.0), location(-INFINITY_VALUE, 0.0),
    location(-90.0, 0.0), location(90.0, 0.0), location(0.0, NAN_VALUE),
    location(0.0, INFINITY_VALUE), location(0.0, -INFINITY_VALUE), location(0.0, -181.0), location(0.0, 181.0)
  }) {
    EXPECT_FALSE(snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UTC, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "location."), nullptr);
  }
  for (const double longitude_deg : std::array { -180.0, 180.0 }) {
    EXPECT_TRUE(snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UTC, location(0.0, longitude_deg), CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
  }
}

TEST(ChartCAbi, PolarPlacidusFailsWithoutPartialResultOrFallback) {
  const Datetime dt { util::to_ymd(2026, 1, 1), 0.5 };
  for (const double latitude_deg : std::array { -70.0, 70.0 }) {
    const auto observer = location(latitude_deg, 0.0);
    ASSERT_FALSE(snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UTC, observer, CELESTIAL_HOUSE_SYSTEM_PLACIDUS).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "Placidus"), nullptr);
    EXPECT_TRUE(snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UTC, observer, CELESTIAL_HOUSE_SYSTEM_EQUAL).valid);
    EXPECT_STREQ(celestial_last_error(), "");
    EXPECT_TRUE(snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UTC, observer, CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN).valid);
  }
}

TEST(ChartCAbi, UnresolvedAscendantRecordsFailureAndRecovers) {
  // #321: the payload is unspecified on failure; inspect only valid and celestial_last_error.
  const Datetime dt { util::to_ymd(2026, 1, 1), std::chrono::seconds { 43205 } };
  const auto placidus = snapshot_at(
    dt,
    CELESTIAL_CIVIL_SCALE_UTC,
    location(WITNESS_LATITUDE_DEG, WITNESS_NORTH_LONGITUDE_DEG),
    CELESTIAL_HOUSE_SYSTEM_PLACIDUS,
    CELESTIAL_DELTA_T_MODEL_ALGO5
  );
  ASSERT_FALSE(placidus.valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "Ascendant"), nullptr);
  for (const auto observer : std::array {
    location(WITNESS_LATITUDE_DEG, WITNESS_NORTH_LONGITUDE_DEG),
    location(-WITNESS_LATITUDE_DEG, WITNESS_SOUTH_LONGITUDE_DEG)
  }) {
    for (const uint32_t system : { CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN }) {
      ASSERT_FALSE(snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UTC, observer, system, CELESTIAL_DELTA_T_MODEL_ALGO5).valid);
      EXPECT_NE(std::strstr(celestial_last_error(), "Ascendant"), nullptr);
      for (const double offset : { -0.000001, 0.000001 }) {
        const auto unresolved = snapshot_at(
          dt,
          CELESTIAL_CIVIL_SCALE_UTC,
          location(observer.latitude.deg(), observer.longitude.deg() + offset),
          system,
          CELESTIAL_DELTA_T_MODEL_ALGO5
        );
        ASSERT_FALSE(unresolved.valid);
        EXPECT_NE(std::strstr(celestial_last_error(), "Ascendant"), nullptr);
        for (const auto control : std::array {
          location(observer.latitude.deg() + offset, observer.longitude.deg()),
          location(observer.latitude.deg() + offset, observer.longitude.deg() + offset)
        }) {
          const auto result = snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UTC, control, system, CELESTIAL_DELTA_T_MODEL_ALGO5);
          ASSERT_TRUE(result.valid) << celestial_last_error();
          EXPECT_STREQ(celestial_last_error(), "");
          expect_snapshot(result, astro::chart::calculate(
            dt, Scale::UTC, control, system == CELESTIAL_HOUSE_SYSTEM_EQUAL ? System::EQUAL : System::WHOLE_SIGN, Model::ALGO5
          ));
        }
      }
    }
  }
  for (const double latitude : { -89.0, 89.0 }) {
    for (const uint32_t system : { CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_HOUSE_SYSTEM_WHOLE_SIGN }) {
      ASSERT_TRUE(snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UTC, location(latitude, 0.0), system).valid) << celestial_last_error();
      EXPECT_STREQ(celestial_last_error(), "");
    }
  }
}

TEST(ChartCAbi, WrapsStationsAndNodeAntipodesMatchCore) {
  const auto observer = location(-33.87, 151.21);
  // Wrap/station epochs replay ephemeris_test.cpp's station cases through the C civil ingress;
  // the signed-rate physics they exercise is pinned there, not re-derived here.
  for (const double jde_tt : std::array { 2460754.876816418, 2460681.293070456, 2460749.7828680002,
                                        2460772.964472, 2460681.5899983719 }) {
    const auto dt = astro::delta_t::tt_to_ut1(astro::julian_day::jde_to_tt(jde_tt));
    const auto actual = snapshot_at(dt, CELESTIAL_CIVIL_SCALE_UT1, observer, CELESTIAL_HOUSE_SYSTEM_PLACIDUS);
    ASSERT_TRUE(actual.valid) << celestial_last_error();
    expect_snapshot(actual, astro::chart::calculate(dt, Scale::UT1, observer, System::PLACIDUS, Model::ALGO5));
  }
  const Datetime dt { util::to_ymd(2025, 3, 25), 0.5 };
  const auto retrograde = celestial_chart_snapshot(2025, 3, 25, 0.5, CELESTIAL_CIVIL_SCALE_UT1, -33.87, 151.21,
                                           CELESTIAL_HOUSE_SYSTEM_PLACIDUS, CELESTIAL_DELTA_T_MODEL_DEFAULT);
  ASSERT_TRUE(retrograde.valid);
  expect_snapshot(retrograde, astro::chart::calculate(dt, Scale::UT1, observer, System::PLACIDUS, Model::ALGO5));
}

TEST(ChartCAbi, ScalarExportsKeepUnitsAndShareLastErrorChannel) {
  const auto actual = celestial_chart_snapshot(2026, 1, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                       CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT);
  ASSERT_TRUE(actual.valid);
  const auto jd = celestial_ut1_to_jd(2026, 1, 1, 0.5);
  ASSERT_TRUE(jd.valid);
  EXPECT_EQ(actual.jd_ut1, jd.value);
  const auto sun = celestial_sun_apparent_geocentric_coord(actual.jde_tt);
  const auto moon = celestial_moon_apparent_geocentric_coord(actual.jde_tt);
  ASSERT_TRUE(sun.valid);
  ASSERT_TRUE(moon.valid);
  EXPECT_EQ(actual.bodies[0].distance_au, sun.r);
  EXPECT_EQ(actual.bodies[1].longitude_deg, moon.lon);
  EXPECT_EQ(actual.bodies[1].latitude_deg, moon.lat);
  EXPECT_EQ(moon.r, astro::moon::geocentric_coord::apparent(actual.jde_tt).r.km());
  EXPECT_NE(actual.bodies[1].distance_au, moon.r);
  EXPECT_EQ(celestial_delta_t(2026.0, CELESTIAL_DELTA_T_MODEL_DEFAULT).value,
            celestial_delta_t(2026.0, CELESTIAL_DELTA_T_MODEL_ALGO5).value);
  ASSERT_FALSE(celestial_ut1_to_jd(2026, 1, 1, NAN_VALUE).valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "fraction"), nullptr);
  ASSERT_TRUE(celestial_chart_snapshot(2026, 1, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                               CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_STREQ(celestial_last_error(), "");
}

TEST(ChartCAbi, LastErrorReplacesClearsAndIsThreadLocal) {
  ASSERT_FALSE(celestial_chart_snapshot(2026, 1, 1, NAN_VALUE, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                 CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "fraction"), nullptr);
  ASSERT_FALSE(celestial_chart_snapshot(2026, 1, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 70.0, 0.0,
                                 CELESTIAL_HOUSE_SYSTEM_PLACIDUS, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "Placidus"), nullptr);
  EXPECT_EQ(std::strstr(celestial_last_error(), "fraction"), nullptr);
  const std::string parent_error { celestial_last_error() };
  const auto parent_kind = celestial_last_error_kind();
  EXPECT_EQ(parent_kind, CELESTIAL_ERROR_INVALID_INPUT);

  std::thread worker { [] {
    EXPECT_STREQ(celestial_last_error(), "");
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    EXPECT_FALSE(celestial_chart_snapshot(2026, 257, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "month"), nullptr);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_TRUE(celestial_chart_snapshot(2026, 1, 1, 0.5, CELESTIAL_CIVIL_SCALE_UT1, 0.0, 0.0,
                                  CELESTIAL_HOUSE_SYSTEM_EQUAL, CELESTIAL_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_STREQ(celestial_last_error(), "");
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  } };
  worker.join();
  EXPECT_EQ(std::string { celestial_last_error() }, parent_error);
  EXPECT_EQ(celestial_last_error_kind(), parent_kind);
  EXPECT_TRUE(celestial_ut1_to_jd(2026, 1, 1, 0.5).valid);
  EXPECT_STREQ(celestial_last_error(), "");
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
}

} // namespace lib::chart::test
