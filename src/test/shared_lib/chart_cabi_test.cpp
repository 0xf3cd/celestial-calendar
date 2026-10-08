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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

#include <gtest/gtest.h>

#include "celestial.h"
#include "chart.hpp"

namespace lib::chart::test {
namespace {

using astro::chart::Scale;
using astro::delta_t::Model;
using astro::ephemeris::Target;
using astro::house::System;
using astro::toolbox::AngleDeg;
using calendar::Datetime;

constexpr std::array MODEL_CODES {
  CHART_DELTA_T_MODEL_ALGO1, CHART_DELTA_T_MODEL_ALGO2, CHART_DELTA_T_MODEL_ALGO3,
  CHART_DELTA_T_MODEL_ALGO4, CHART_DELTA_T_MODEL_ALGO5,
};
constexpr std::array MODELS { Model::ALGO1, Model::ALGO2, Model::ALGO3, Model::ALGO4, Model::ALGO5 };
constexpr std::array SYSTEM_CODES {
  CHART_HOUSE_SYSTEM_EQUAL, CHART_HOUSE_SYSTEM_WHOLE_SIGN, CHART_HOUSE_SYSTEM_PLACIDUS,
};
constexpr std::array SYSTEMS { System::EQUAL, System::WHOLE_SIGN, System::PLACIDUS };
constexpr std::array TARGET_CODES {
  CHART_TARGET_SUN, CHART_TARGET_MOON, CHART_TARGET_MERCURY, CHART_TARGET_VENUS,
  CHART_TARGET_MARS, CHART_TARGET_JUPITER, CHART_TARGET_SATURN, CHART_TARGET_URANUS,
  CHART_TARGET_NEPTUNE, CHART_TARGET_PLUTO, CHART_TARGET_MEAN_ASCENDING,
  CHART_TARGET_MEAN_DESCENDING, CHART_TARGET_TRUE_ASCENDING, CHART_TARGET_TRUE_DESCENDING,
};
constexpr std::array TARGETS {
  Target::SUN, Target::MOON, Target::MERCURY, Target::VENUS, Target::MARS, Target::JUPITER,
  Target::SATURN, Target::URANUS, Target::NEPTUNE, Target::PLUTO, Target::MEAN_ASCENDING,
  Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING,
};
constexpr double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
constexpr double INFINITY_VALUE = std::numeric_limits<double>::infinity();

static_assert(std::is_standard_layout_v<ChartBodyV1> and std::is_trivially_copyable_v<ChartBodyV1>);
static_assert(std::is_standard_layout_v<ChartHousesV1> and std::is_trivially_copyable_v<ChartHousesV1>);
static_assert(std::is_standard_layout_v<ChartSnapshotV1> and std::is_trivially_copyable_v<ChartSnapshotV1>);
static_assert(std::is_same_v<decltype(ChartBodyV1::target_code), uint32_t>);
static_assert(std::is_same_v<decltype(ChartBodyV1::present_fields), uint32_t>);
static_assert(std::is_same_v<decltype(&chart_snapshot_v1),
                           ChartSnapshotV1 (*)(int32_t, uint32_t, uint32_t, double, uint32_t,
                                               double, double, uint32_t, uint32_t)>);
static_assert(std::extent_v<decltype(ChartSnapshotV1::bodies)> == 14);
static_assert(std::extent_v<decltype(ChartHousesV1::cusps_deg)> == 12);
static_assert(sizeof(ChartBodyV1) == 40 and alignof(ChartBodyV1) == 8);
static_assert(sizeof(ChartHousesV1) == 128 and alignof(ChartHousesV1) == 8);
static_assert(sizeof(ChartSnapshotV1) == 712 and alignof(ChartSnapshotV1) == 8);

auto location(const double latitude_deg, const double longitude_deg) -> astro::GeoLocation {
  return { .latitude = AngleDeg { latitude_deg }, .longitude = AngleDeg { longitude_deg } };
}

auto snapshot_at(const Datetime& civil_dt, const uint32_t scale_code, const astro::GeoLocation& observer,
                 const uint32_t system_code, const uint32_t model_code = CHART_DELTA_T_MODEL_DEFAULT) -> ChartSnapshotV1 {
  const auto [year, month, day] = util::from_ymd(civil_dt.ymd);
  return chart_snapshot_v1(
    year, month, day, civil_dt.fraction(), scale_code,
    observer.latitude.deg(), observer.longitude.deg(), system_code, model_code
  );
}

auto distance_au(const std::optional<astro::toolbox::DistanceAu>& distance) -> double {
  if (not distance.has_value()) {
    throw std::logic_error { "Missing test distance" };
  }
  return distance->au();
}

auto expected_coordinate(const Target target, const double jde_tt) -> astro::toolbox::SphericalCoordinate {
  using astro::planet::Planet;

  switch (target) {
    case Target::SUN: return astro::sun::geocentric_coord::apparent(jde_tt);
    case Target::MOON: return astro::moon::geocentric_coord::apparent(jde_tt);
    case Target::MERCURY: return astro::planet::geocentric_coord::apparent(Planet::MERCURY, jde_tt);
    case Target::VENUS: return astro::planet::geocentric_coord::apparent(Planet::VENUS, jde_tt);
    case Target::MARS: return astro::planet::geocentric_coord::apparent(Planet::MARS, jde_tt);
    case Target::JUPITER: return astro::planet::geocentric_coord::apparent(Planet::JUPITER, jde_tt);
    case Target::SATURN: return astro::planet::geocentric_coord::apparent(Planet::SATURN, jde_tt);
    case Target::URANUS: return astro::planet::geocentric_coord::apparent(Planet::URANUS, jde_tt);
    case Target::NEPTUNE: return astro::planet::geocentric_coord::apparent(Planet::NEPTUNE, jde_tt);
    case Target::PLUTO: return astro::planet::geocentric_coord::apparent(Planet::PLUTO, jde_tt);
    default: throw std::logic_error { "Nodes have no test spherical coordinate" };
  }
}

auto expect_snapshot(const ChartSnapshotV1& actual, const astro::chart::Snapshot& expected) -> void {
  ASSERT_TRUE(actual.valid) << last_error();
  EXPECT_EQ(actual.jd_ut1, expected.times.jd_ut1);
  EXPECT_EQ(actual.jde_tt, expected.times.jde_tt);
  for (std::size_t index = 0; index < TARGETS.size(); ++index) {
    SCOPED_TRACE(index);
    const auto& body = std::span { actual.bodies }[index];
    const auto& state = expected.bodies.at(index);
    EXPECT_EQ(body.target_code, TARGET_CODES.at(index));
    EXPECT_EQ(state.target, TARGETS.at(index));
    EXPECT_EQ(body.present_fields, CHART_PRESENT_LATITUDE | (state.distance.has_value() ? CHART_PRESENT_DISTANCE : 0U));
    EXPECT_EQ(body.longitude_deg, state.longitude.deg());
    EXPECT_EQ(body.latitude_deg, state.latitude.deg());
    if (state.distance.has_value()) {
      EXPECT_EQ(body.distance_au, distance_au(state.distance));
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

auto expect_recomposition(const ChartSnapshotV1& actual, const astro::GeoLocation& observer, const System system) -> void {
  ASSERT_TRUE(actual.valid) << last_error();
  for (std::size_t index = 0; index < TARGETS.size(); ++index) {
    SCOPED_TRACE(index);
    const auto& body = std::span { actual.bodies }[index];
    EXPECT_EQ(body.target_code, TARGET_CODES.at(index));
    EXPECT_TRUE(std::isfinite(body.longitude_deg));
    EXPECT_GE(body.longitude_deg, 0.0);
    EXPECT_LT(body.longitude_deg, 360.0);
    EXPECT_EQ(std::bit_cast<uint64_t>(body.longitude_rate_deg_per_tt_day),
              std::bit_cast<uint64_t>(astro::ephemeris::longitude_rate(TARGETS.at(index), actual.jde_tt).deg_per_tt_day));
    if (index < 10) {
      const auto expected = expected_coordinate(TARGETS.at(index), actual.jde_tt);
      EXPECT_EQ(body.present_fields, CHART_PRESENT_LATITUDE | CHART_PRESENT_DISTANCE);
      EXPECT_EQ(body.longitude_deg, expected.λ.deg());
      EXPECT_EQ(body.latitude_deg, expected.β.deg());
      EXPECT_EQ(body.distance_au, expected.r.au());
      EXPECT_GT(body.distance_au, 0.0);
    } else {
      using astro::lunar_node::Node;
      constexpr std::array NODES { Node::MEAN_ASCENDING, Node::MEAN_DESCENDING, Node::TRUE_ASCENDING, Node::TRUE_DESCENDING };

      EXPECT_EQ(body.present_fields, CHART_PRESENT_LATITUDE);
      EXPECT_EQ(body.latitude_deg, 0.0);
      EXPECT_EQ(body.longitude_deg, astro::lunar_node::position(NODES.at(index - 10), actual.jde_tt).deg());
    }
  }
  const auto expected = astro::house::calculate(actual.jd_ut1, actual.jde_tt, observer, system);
  EXPECT_EQ(actual.houses.ascendant_deg, expected.ascendant.deg());
  EXPECT_EQ(actual.houses.midheaven_deg, expected.midheaven.deg());
  EXPECT_EQ(actual.houses.descendant_deg, expected.descendant.deg());
  EXPECT_EQ(actual.houses.imum_coeli_deg, expected.imum_coeli.deg());
  for (std::size_t index = 0; index < expected.cusps.size(); ++index) {
    EXPECT_EQ(std::span { actual.houses.cusps_deg }[index], expected.cusps.at(index).deg());
  }
}

} // namespace

TEST(ChartCAbi, EveryModelSystemAndTargetMatchesCore) {
  const Datetime dt { util::to_ymd(2026, 1, 1), 0.5123456789 };
  for (std::size_t model_index = 0; model_index < MODELS.size(); ++model_index) {
    for (std::size_t system_index = 0; system_index < SYSTEMS.size(); ++system_index) {
      for (const auto observer : std::array { location(51.5, 0.0), location(-33.87, 151.21) }) {
        SCOPED_TRACE(model_index);
        SCOPED_TRACE(system_index);
        const auto actual = snapshot_at(dt, CHART_CIVIL_SCALE_UTC, observer,
                                        SYSTEM_CODES.at(system_index), MODEL_CODES.at(model_index));
        ASSERT_TRUE(actual.valid) << last_error();
        EXPECT_STREQ(last_error(), "");
        const Datetime input { dt.ymd, dt.fraction() };
        const auto expected = astro::chart::calculate(input, Scale::UTC, observer,
                                                      SYSTEMS.at(system_index), MODELS.at(model_index));
        expect_snapshot(actual, expected);
        expect_recomposition(actual, observer, SYSTEMS.at(system_index));
      }
    }
  }
}

TEST(ChartCAbi, DefaultIsAlgo5AndNotAlgo1) {
  const Datetime dt { util::to_ymd(2026, 1, 1), 0.5 };
  const auto observer = location(40.0, -75.0);
  const auto default_result = snapshot_at(dt, CHART_CIVIL_SCALE_UT1, observer, CHART_HOUSE_SYSTEM_EQUAL);
  const auto algo5 = snapshot_at(dt, CHART_CIVIL_SCALE_UT1, observer, CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_ALGO5);
  const auto algo1 = snapshot_at(dt, CHART_CIVIL_SCALE_UT1, observer, CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_ALGO1);
  ASSERT_TRUE(default_result.valid);
  ASSERT_TRUE(algo5.valid);
  ASSERT_TRUE(algo1.valid);
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
    const auto actual = snapshot_at(dt, CHART_CIVIL_SCALE_UT1, observer, CHART_HOUSE_SYSTEM_EQUAL, MODEL_CODES.at(index));
    ASSERT_TRUE(actual.valid) << last_error();
    EXPECT_EQ(actual.jd_ut1, astro::julian_day::ut1_to_jd(dt));
    EXPECT_EQ(actual.jde_tt, astro::julian_day::tt_to_jde(
      calendar::add_seconds(dt, astro::delta_t::compute(MODELS.at(index), dt))));
    expect_recomposition(actual, observer, System::EQUAL);
  }
  EXPECT_FALSE(snapshot_at(dt, CHART_CIVIL_SCALE_UTC, observer, CHART_HOUSE_SYSTEM_EQUAL).valid);
  EXPECT_NE(std::strstr(last_error(), "1972"), nullptr);
}

TEST(ChartCAbi, UtcBeginsAt1972AndUsesLeapTable) {
  const auto observer = location(0.0, 0.0);
  const Datetime first { util::to_ymd(1972, 1, 1), 0.0 };
  const Datetime before { util::to_ymd(1971, 12, 31), 0.5 };
  ASSERT_FALSE(snapshot_at(before, CHART_CIVIL_SCALE_UTC, observer, CHART_HOUSE_SYSTEM_EQUAL).valid);
  EXPECT_NE(std::strstr(last_error(), "1972"), nullptr);
  const auto historical = snapshot_at(before, CHART_CIVIL_SCALE_UT1, observer, CHART_HOUSE_SYSTEM_EQUAL);
  ASSERT_TRUE(historical.valid) << last_error();
  expect_recomposition(historical, observer, System::EQUAL);

  const auto utc = snapshot_at(first, CHART_CIVIL_SCALE_UTC, observer, CHART_HOUSE_SYSTEM_EQUAL);
  ASSERT_TRUE(utc.valid) << last_error();
  const auto tt = astro::leap_second::utc_to_tt(first);
  EXPECT_EQ(utc.jde_tt, astro::julian_day::tt_to_jde(tt));
  EXPECT_EQ(utc.jd_ut1, astro::julian_day::ut1_to_jd(
    calendar::add_seconds(tt, -astro::delta_t::compute(Model::ALGO5, tt))));
  expect_recomposition(utc, observer, System::EQUAL);
}

TEST(ChartCAbi, RejectsUnknownAndWideSelectorAliases) {
  for (const uint32_t code : std::array { 2U, 255U, 256U, 257U, UINT32_C(0x10000001), UINT32_MAX }) {
    EXPECT_FALSE(chart_snapshot_v1(2026, 1, 1, 0.5, code, 0.0, 0.0,
                                   CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(last_error(), "civil_scale_code"), nullptr);
  }
  for (const uint32_t code : std::array { 3U, 255U, 256U, 257U, UINT32_C(0x10000002), UINT32_MAX }) {
    EXPECT_FALSE(chart_snapshot_v1(2026, 1, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   code, CHART_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(last_error(), "house_system_code"), nullptr);
  }
  for (const uint32_t code : std::array { 6U, 255U, 256U, 257U, 261U, UINT32_C(0x10000005), UINT32_MAX }) {
    EXPECT_FALSE(chart_snapshot_v1(2026, 1, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CHART_HOUSE_SYSTEM_EQUAL, code).valid);
    EXPECT_NE(std::strstr(last_error(), "delta_t_model_code"), nullptr);
  }
}

TEST(ChartCAbi, RejectsYearsAtRepresentationAndAdmissionBoundaries) {
  for (const int32_t year : std::array { INT32_MIN, -63510, 67562, INT32_MAX }) {
    EXPECT_FALSE(chart_snapshot_v1(year, 1, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(last_error(), "std::chrono::year"), nullptr);
  }

  for (const int32_t year : std::array { 1884, 2100 }) {
    EXPECT_FALSE(chart_snapshot_v1(year, 1, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(last_error(), "Chart civil year"), nullptr);
  }
}

TEST(ChartCAbi, RejectsRawMonthAndDayBeforeChronoNarrowing) {
  for (const uint32_t month : std::array { 0U, 13U, 256U, 257U, 268U, 65537U, UINT32_MAX }) {
    EXPECT_FALSE(chart_snapshot_v1(2026, month, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(last_error(), "month"), nullptr);
  }
  for (const uint32_t day : std::array { 0U, 32U, 256U, 257U, 287U, 65537U, UINT32_MAX }) {
    EXPECT_FALSE(chart_snapshot_v1(2026, 1, day, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(last_error(), "day"), nullptr);
  }
  EXPECT_FALSE(chart_snapshot_v1(1900, 2, 29, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                 CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_FALSE(chart_snapshot_v1(2026, 4, 31, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                 CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_TRUE(chart_snapshot_v1(2000, 2, 29, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_STREQ(last_error(), "");
}

TEST(ChartCAbi, RejectsNonfiniteAndOutOfRangeFractions) {
  for (const double fraction : std::array { NAN_VALUE, INFINITY_VALUE, -INFINITY_VALUE, -0.1, 1.0, 1e100 }) {
    EXPECT_FALSE(chart_snapshot_v1(2026, 1, 1, fraction, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(last_error(), "fraction"), nullptr);
  }
  EXPECT_TRUE(chart_snapshot_v1(2026, 1, 1, -0.0, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
}

TEST(ChartCAbi, StrictJointDomainIsCheckedAfterTimeResolution) {
  const auto observer = location(0.0, 0.0);
  for (const double edge : std::array { astro::chart::detail::START_JDE_TT, astro::chart::detail::END_JDE_TT }) {
    for (const double seconds : std::array { -1.0, 0.0, 1.0 }) {
      SCOPED_TRACE(edge);
      SCOPED_TRACE(seconds);
      const auto tt = astro::julian_day::jde_to_tt(edge + (seconds / 86400.0));
      const auto ut1 = astro::delta_t::tt_to_ut1(tt);
      const Datetime input { ut1.ymd, ut1.fraction() };
      const double recomposed = astro::julian_day::tt_to_jde(
        calendar::add_seconds(input, astro::delta_t::compute(Model::ALGO5, input)));
      const bool admitted = recomposed >= astro::chart::detail::START_JDE_TT and
                            recomposed < astro::chart::detail::END_JDE_TT;
      const auto actual = snapshot_at(ut1, CHART_CIVIL_SCALE_UT1, observer, CHART_HOUSE_SYSTEM_EQUAL);
      ASSERT_EQ(actual.valid, admitted) << last_error();
      if (admitted) {
        EXPECT_EQ(actual.jde_tt, recomposed);
        expect_recomposition(actual, observer, System::EQUAL);
      } else {
        EXPECT_NE(std::strstr(last_error(), "Chart JDE(TT)"), nullptr);
      }
    }
  }
}

TEST(ChartCAbi, Algo4CapIsStrictInDerivedTtAndHasNoDefaultFallback) {
  const auto observer = location(0.0, 0.0);
  // Boundary replays from chart_test.cpp; conversion wiring, not new physical goldens.
  for (const double fraction : std::array { 0.99919925902642359, 0.99919925925925923 }) {
    const Datetime dt { util::to_ymd(2034, 12, 31), fraction };
    const auto actual = chart_snapshot_v1(2034, 12, 31, fraction, CHART_CIVIL_SCALE_UTC, 0.0, 0.0,
                                          CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_ALGO4);
    if (fraction == 0.99919925902642359) {
      ASSERT_TRUE(actual.valid) << last_error();
      EXPECT_EQ(actual.jde_tt, std::nextafter(astro::chart::detail::ALGO4_END_JDE_TT, 0.0));
      expect_recomposition(actual, observer, System::EQUAL);
    } else {
      EXPECT_EQ(astro::julian_day::utc_to_jde(dt), astro::chart::detail::ALGO4_END_JDE_TT);
      EXPECT_FALSE(actual.valid);
      EXPECT_NE(std::strstr(last_error(), "Algo4"), nullptr);
      EXPECT_TRUE(chart_snapshot_v1(2034, 12, 31, fraction, CHART_CIVIL_SCALE_UTC, 0.0, 0.0,
                                    CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_ALGO5).valid);
    }
  }
  EXPECT_FALSE(chart_snapshot_v1(2035, 6, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                 CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_ALGO4).valid);
  EXPECT_STRNE(last_error(), "");
  EXPECT_TRUE(chart_snapshot_v1(2035, 6, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
}

TEST(ChartCAbi, LocationRequiresOpenLatitudeAndClosedLongitudeIntervals) {
  const Datetime dt { util::to_ymd(2026, 1, 1), 0.5 };
  for (const auto observer : std::array {
    location(NAN_VALUE, 0.0), location(INFINITY_VALUE, 0.0), location(-INFINITY_VALUE, 0.0),
    location(-90.0, 0.0), location(90.0, 0.0), location(0.0, NAN_VALUE),
    location(0.0, INFINITY_VALUE), location(0.0, -INFINITY_VALUE), location(0.0, -181.0), location(0.0, 181.0)
  }) {
    EXPECT_FALSE(snapshot_at(dt, CHART_CIVIL_SCALE_UTC, observer, CHART_HOUSE_SYSTEM_EQUAL).valid);
    EXPECT_NE(std::strstr(last_error(), "location."), nullptr);
  }
  for (const double longitude_deg : std::array { -180.0, 180.0 }) {
    EXPECT_TRUE(snapshot_at(dt, CHART_CIVIL_SCALE_UTC, location(0.0, longitude_deg), CHART_HOUSE_SYSTEM_EQUAL).valid);
  }
}

TEST(ChartCAbi, PolarPlacidusFailsWithoutPartialResultOrFallback) {
  const Datetime dt { util::to_ymd(2026, 1, 1), 0.5 };
  for (const double latitude_deg : std::array { -70.0, 70.0 }) {
    const auto observer = location(latitude_deg, 0.0);
    ASSERT_FALSE(snapshot_at(dt, CHART_CIVIL_SCALE_UTC, observer, CHART_HOUSE_SYSTEM_PLACIDUS).valid);
    EXPECT_NE(std::strstr(last_error(), "Placidus"), nullptr);
    EXPECT_TRUE(snapshot_at(dt, CHART_CIVIL_SCALE_UTC, observer, CHART_HOUSE_SYSTEM_EQUAL).valid);
    EXPECT_STREQ(last_error(), "");
    EXPECT_TRUE(snapshot_at(dt, CHART_CIVIL_SCALE_UTC, observer, CHART_HOUSE_SYSTEM_WHOLE_SIGN).valid);
  }
}

TEST(ChartCAbi, WrapsStationsAndNodeAntipodesPreserveSignedRates) {
  const auto observer = location(-33.87, 151.21);
  // Wrap/station epochs replay ephemeris_test.cpp through the C civil ingress.
  for (const double jde_tt : std::array { 2460754.876816418, 2460681.293070456, 2460749.7828680002,
                                        2460772.964472, 2460681.5899983719 }) {
    const auto dt = astro::delta_t::tt_to_ut1(astro::julian_day::jde_to_tt(jde_tt));
    const auto actual = snapshot_at(dt, CHART_CIVIL_SCALE_UT1, observer, CHART_HOUSE_SYSTEM_PLACIDUS);
    ASSERT_TRUE(actual.valid) << last_error();
    expect_recomposition(actual, observer, System::PLACIDUS);
    EXPECT_EQ(std::bit_cast<uint64_t>(actual.bodies[10].longitude_rate_deg_per_tt_day),
              std::bit_cast<uint64_t>(actual.bodies[11].longitude_rate_deg_per_tt_day));
    EXPECT_EQ(std::bit_cast<uint64_t>(actual.bodies[12].longitude_rate_deg_per_tt_day),
              std::bit_cast<uint64_t>(actual.bodies[13].longitude_rate_deg_per_tt_day));
  }
  const auto retrograde = chart_snapshot_v1(2025, 3, 25, 0.5, CHART_CIVIL_SCALE_UT1, -33.87, 151.21,
                                           CHART_HOUSE_SYSTEM_PLACIDUS, CHART_DELTA_T_MODEL_DEFAULT);
  ASSERT_TRUE(retrograde.valid);
  EXPECT_LT(retrograde.bodies[2].longitude_rate_deg_per_tt_day, 0.0);
  EXPECT_LT(retrograde.bodies[10].longitude_rate_deg_per_tt_day, 0.0);
}

TEST(ChartCAbi, OldExportsKeepUnitsAndShareLastErrorChannel) {
  const auto actual = chart_snapshot_v1(2026, 1, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                       CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT);
  ASSERT_TRUE(actual.valid);
  const auto jd = ut1_to_jd(2026, 1, 1, 0.5);
  ASSERT_TRUE(jd.valid);
  EXPECT_EQ(actual.jd_ut1, jd.value);
  const auto sun = sun_apparent_geocentric_coord(actual.jde_tt);
  const auto moon = moon_apparent_geocentric_coord(actual.jde_tt);
  ASSERT_TRUE(sun.valid);
  ASSERT_TRUE(moon.valid);
  EXPECT_EQ(actual.bodies[0].distance_au, sun.r);
  EXPECT_EQ(actual.bodies[1].longitude_deg, moon.lon);
  EXPECT_EQ(actual.bodies[1].latitude_deg, moon.lat);
  EXPECT_EQ(moon.r, astro::moon::geocentric_coord::apparent(actual.jde_tt).r.km());
  EXPECT_NE(actual.bodies[1].distance_au, moon.r);
  EXPECT_EQ(delta_t(2026.0).value, delta_t_algo5(2026.0).value);
  ASSERT_FALSE(ut1_to_jd(2026, 1, 1, NAN_VALUE).valid);
  EXPECT_NE(std::strstr(last_error(), "fraction"), nullptr);
  ASSERT_TRUE(chart_snapshot_v1(2026, 1, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                               CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_STREQ(last_error(), "");
}

TEST(ChartCAbi, LastErrorReplacesClearsAndIsThreadLocal) {
  ASSERT_FALSE(chart_snapshot_v1(2026, 1, 1, NAN_VALUE, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                 CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_NE(std::strstr(last_error(), "fraction"), nullptr);
  ASSERT_FALSE(chart_snapshot_v1(2026, 1, 1, 0.5, CHART_CIVIL_SCALE_UT1, 70.0, 0.0,
                                 CHART_HOUSE_SYSTEM_PLACIDUS, CHART_DELTA_T_MODEL_DEFAULT).valid);
  EXPECT_NE(std::strstr(last_error(), "Placidus"), nullptr);
  EXPECT_EQ(std::strstr(last_error(), "fraction"), nullptr);
  const std::string parent_error { last_error() };

  std::thread worker { [] {
    EXPECT_STREQ(last_error(), "");
    EXPECT_FALSE(chart_snapshot_v1(2026, 257, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                   CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_NE(std::strstr(last_error(), "month"), nullptr);
    EXPECT_TRUE(chart_snapshot_v1(2026, 1, 1, 0.5, CHART_CIVIL_SCALE_UT1, 0.0, 0.0,
                                  CHART_HOUSE_SYSTEM_EQUAL, CHART_DELTA_T_MODEL_DEFAULT).valid);
    EXPECT_STREQ(last_error(), "");
  } };
  worker.join();
  EXPECT_EQ(std::string { last_error() }, parent_error);
  EXPECT_TRUE(set_log_verbosity(0));
  EXPECT_STREQ(last_error(), "");
}

} // namespace lib::chart::test
