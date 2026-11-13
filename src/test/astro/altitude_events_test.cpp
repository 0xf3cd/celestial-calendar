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
#include <cstdint>
#include <stdexcept>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "altitude_events.hpp"

namespace astro::observer::test {
namespace {

template <typename T>
concept HasWeather = requires(T value) { value.params; };

static_assert(not HasWeather<GeometricLevel>);
static_assert(HasWeather<RefractedLevel>);

/** @brief Fixed J2000 north-pole observer; thresholds refer to body centres. */
auto site() -> Observer {
  return {
    .location = { .latitude = toolbox::AngleDeg { 90.0 }, .longitude = toolbox::AngleDeg { -77.0 } },
    .elevation = toolbox::DistanceKm { 0.0 },
  };
}

/** @brief Sun centre above a geometric threshold that is safely below its polar declination. */
auto condition(const AltitudeSense sense = AltitudeSense::ABOVE) -> AltitudeCondition {
  return { .target = ephemeris::Target::SUN, .level = GeometricLevel { .altitude = toolbox::AngleDeg { -45.0 } }, .sense = sense };
}

/** @brief Read geometric altitude through the instantaneous observer API with the same fixed clock. */
auto instantaneous_altitude(const Observer& observer, const double jde_tt) -> double {
  const auto ut1 = delta_t::tt_to_ut1(delta_t::Model::ALGO5, julian_day::jde_to_tt(jde_tt));
  const julian_day::Times times { .jd_ut1 = julian_day::ut1_to_jd(ut1), .jde_tt = jde_tt };
  return astro::observer::observe(times, observer, ephemeris::Target::SUN).altitude.deg();
}

} // namespace

TEST(AltitudeEvents, GeometricLevelHasNoIgnoredWeatherAndPreservesRequestedMetadata) {
  const auto query = altitude_events(site(), condition(), 2451545.0, 2451545.25);
  EXPECT_TRUE(query.events.empty());
  EXPECT_TRUE(query.condition.initially_satisfied);
  EXPECT_EQ(query.condition.condition.target, ephemeris::Target::SUN);
  EXPECT_EQ(query.condition.condition.sense, AltitudeSense::ABOVE);
  EXPECT_EQ(query.condition.source, ephemeris::PositionSource::SUN_VSOP87D);
  EXPECT_EQ(query.condition.delta_t_model, delta_t::Model::ALGO5);
  EXPECT_DOUBLE_EQ(query.condition.geometric_level.deg(), -45.0);
  EXPECT_TRUE(std::holds_alternative<GeometricLevel>(query.condition.condition.level));
}

TEST(AltitudeEvents, BelowReversesTheConditionTruthWithoutChangingTheResolvedLevel) {
  const auto above = altitude_events(site(), condition(), 2451545.0, 2451545.25);
  const auto below = altitude_events(site(), condition(AltitudeSense::BELOW), 2451545.0, 2451545.25);
  EXPECT_NE(above.condition.initially_satisfied, below.condition.initially_satisfied);
  EXPECT_DOUBLE_EQ(above.condition.geometric_level.deg(), below.condition.geometric_level.deg());
}

TEST(AltitudeEvents, BothAtmosphericModelsPreserveTheirOwnWeatherAndConvention) {
  for (const auto model : { earth::refraction::Model::BENNETT, earth::refraction::Model::SAEMUNDSSON }) {
    const AltitudeCondition requested {
      .target = ephemeris::Target::SUN,
      .level = RefractedLevel {
        .altitude = toolbox::AngleDeg { 10.0 }, .params = { .temperature_c = 15.0, .pressure_hpa = 1013.25, .model = model },
      },
      .sense = AltitudeSense::ABOVE,
    };
    const auto query = altitude_events(site(), requested, 2451545.0, 2451545.25);
    ASSERT_TRUE(std::holds_alternative<RefractedLevel>(query.condition.condition.level));
    const auto& level = std::get<RefractedLevel>(query.condition.condition.level);
    EXPECT_DOUBLE_EQ(level.altitude.deg(), 10.0);
    EXPECT_DOUBLE_EQ(level.params.temperature_c, 15.0);
    EXPECT_DOUBLE_EQ(level.params.pressure_hpa, 1013.25);
    EXPECT_EQ(level.params.model, model);
    EXPECT_GT(query.condition.geometric_level.deg(), 9.0);
    EXPECT_LT(query.condition.geometric_level.deg(), 10.0);
    EXPECT_FALSE(query.condition.initially_satisfied);
  }
}

TEST(AltitudeEvents, NamedPhysicalTargetsRetainTheirOwnPositionSources) {
  for (const auto target : std::array {
      ephemeris::Target::SUN, ephemeris::Target::MOON, ephemeris::Target::MERCURY, ephemeris::Target::VENUS,
      ephemeris::Target::MARS, ephemeris::Target::JUPITER, ephemeris::Target::SATURN, ephemeris::Target::URANUS,
      ephemeris::Target::NEPTUNE, ephemeris::Target::PLUTO,
  }) {
    const AltitudeCondition requested {
      .target = target, .level = GeometricLevel { .altitude = toolbox::AngleDeg { -89.0 } }, .sense = AltitudeSense::ABOVE,
    };
    const auto query = altitude_events(site(), requested, 2451545.0, 2451545.25);
    EXPECT_EQ(query.condition.condition.target, target);
    EXPECT_EQ(query.condition.source, ephemeris::source_of(target));
    EXPECT_EQ(query.condition.delta_t_model, delta_t::Model::ALGO5);
    EXPECT_TRUE(query.condition.initially_satisfied);
  }
}

TEST(AltitudeEvents, ContradictoryConditionsHaveNoIntersectedWindow) {
  const std::array requested { condition(), condition(AltitudeSense::BELOW) };
  const auto query = observing_windows(site(), requested, 2451545.0, 2451545.25);
  ASSERT_EQ(query.conditions.size(), 2U);
  EXPECT_TRUE(query.conditions.at(0).initially_satisfied);
  EXPECT_FALSE(query.conditions.at(1).initially_satisfied);
  EXPECT_TRUE(query.events.empty());
  EXPECT_TRUE(query.windows.empty());
}

TEST(AltitudeEvents, EmptyConjunctionRetainsTheCalendarExclusiveUpperEnd) {
  const auto query = observing_windows(site(), {}, 13689325.25, 13689325.5);
  EXPECT_TRUE(query.conditions.empty());
  EXPECT_TRUE(query.events.empty());
  ASSERT_EQ(query.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(query.windows.front().start_jde_tt, 13689325.25);
  EXPECT_DOUBLE_EQ(query.windows.front().end_jde_tt, 13689325.5);
}

TEST(AltitudeEvents, UpperTransitsCarryPhysicalSourceAndClockWithoutAnAltitudeCondition) {
  auto observer = site();
  observer.location.latitude = toolbox::AngleDeg { 0.0 };
  const auto query = upper_transits(observer, ephemeris::Target::MARS, 2451545.0, 2451546.0);
  ASSERT_EQ(query.size(), 1U);
  EXPECT_EQ(query.front().target, ephemeris::Target::MARS);
  EXPECT_EQ(query.front().source, ephemeris::PositionSource::PLANET_VSOP87D);
  EXPECT_EQ(query.front().delta_t_model, delta_t::Model::ALGO5);
  EXPECT_GE(query.front().jde_tt, 2451545.0);
  EXPECT_LT(query.front().jde_tt, 2451546.0);
}

TEST(AltitudeEvents, CrossingDirectionsAndWindowCompanionsMatchInstantaneousSideObservations) {
  auto observer = site();
  observer.location.latitude = toolbox::AngleDeg { 0.0 };
  const AltitudeCondition requested {
    .target = ephemeris::Target::SUN, .level = GeometricLevel { .altitude = toolbox::AngleDeg { 0.0 } },
    .sense = AltitudeSense::ABOVE,
  };
  const auto query = altitude_events(observer, requested, 2451545.0, 2451546.0);
  const std::array conditions { requested };
  const auto windows = observing_windows(observer, conditions, 2451545.0, 2451546.0);
  ASSERT_EQ(query.events.size(), 2U);
  ASSERT_EQ(windows.events.size(), query.events.size());
  ASSERT_EQ(windows.windows.size(), 2U);
  EXPECT_TRUE(query.condition.initially_satisfied);
  for (std::size_t index = 0; index < query.events.size(); ++index) {
    const auto& event = query.events.at(index);
    const double left = instantaneous_altitude(observer, event.jde_tt - 0x1p-18);
    const double right = instantaneous_altitude(observer, event.jde_tt + 0x1p-18);
    ASSERT_LT(left * right, 0.0);
    const auto expected = left < right ? ephemeris::EventDirection::INCREASING : ephemeris::EventDirection::DECREASING;
    EXPECT_EQ(event.direction, expected);
    EXPECT_EQ(event.condition_index, 0U);
    EXPECT_DOUBLE_EQ(event.jde_tt, windows.events.at(index).jde_tt);
    EXPECT_EQ(event.direction, windows.events.at(index).direction);
    EXPECT_EQ(event.condition_index, windows.events.at(index).condition_index);
  }
}

TEST(AltitudeEvents, ANodeIsNotAPhysicalAltitudeOrTransitTarget) {
  auto requested = condition();
  requested.target = ephemeris::Target::MEAN_ASCENDING;
  EXPECT_THROW((void)altitude_events(site(), requested, 2451545.0, 2451545.25), std::invalid_argument);
  EXPECT_THROW((void)upper_transits(site(), ephemeris::Target::MEAN_ASCENDING, 2451545.0, 2451545.25), std::invalid_argument);
}

TEST(AltitudeEvents, UnresolvedPhysicalCensusFailsWithoutAResult) {
  auto observer = site();
  observer.location.latitude = toolbox::AngleDeg { 30.0 };
  const AltitudeCondition requested {
    .target = ephemeris::Target::PLUTO,
    .level = GeometricLevel { .altitude = toolbox::AngleDeg { 48.605252016244656 } }, .sense = AltitudeSense::ABOVE,
  };
  EXPECT_THROW((void)altitude_events(observer, requested, 2451545.0, 2451545.25), std::runtime_error);
}

TEST(AltitudeEvents, AtmosphericCodeTwoCannotMasqueradeAsAGeometricLevel) {
  const AltitudeCondition requested {
    .target = ephemeris::Target::SUN,
    .level = RefractedLevel {
      .altitude = toolbox::AngleDeg { 10.0 },
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) -- Unknown atmospheric identity must not become the internal geometric convention 2.
      .params = { .temperature_c = 15.0, .pressure_hpa = 1013.25, .model = static_cast<earth::refraction::Model>(2) },
    },
    .sense = AltitudeSense::ABOVE,
  };
  EXPECT_THROW((void)altitude_events(site(), requested, 2451545.0, 2451545.25), std::invalid_argument);
}

TEST(AltitudeEvents, OversizedConditionListFailsBeforeDecodingItsElements) {
  auto invalid = condition();
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) -- Size admission must precede even a forged condition selector.
  invalid.sense = static_cast<AltitudeSense>(2);
  const std::vector<AltitudeCondition> requested(detail::altitude_events::MAX_CONDITIONS + 1, invalid);
  EXPECT_THROW((void)observing_windows(site(), requested, 2451545.0, 2451545.25), std::runtime_error);
  EXPECT_THROW((void)altitude_events(site(), invalid, 2451545.0, 2451545.25), std::invalid_argument);
}

TEST(AltitudeEvents, DefaultWorkAndResolutionLimitsAreFixed) {
  const auto budget = detail::altitude_events::query_budget();
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
  EXPECT_EQ(budget.candidates_remaining, 2048U);
  EXPECT_EQ(detail::altitude_events::MAX_CONDITIONS, 2048U);
  EXPECT_DOUBLE_EQ(detail::altitude_events::TIME_RESOLUTION_TT_DAYS, 0x1p-20);
  EXPECT_DOUBLE_EQ(detail::altitude_events::LEVEL_RESOLUTION_DEGREES, 0x1p-30);
}

TEST(AltitudeEvents, SelectedSourcesDoNotInheritPlutoOrChartAdmission) {
  for (const auto target : std::array {
    ephemeris::Target::SUN, ephemeris::Target::MOON, ephemeris::Target::MARS,
    ephemeris::Target::JUPITER, ephemeris::Target::SATURN,
  }) {
    const AltitudeCondition requested {
      .target = target, .level = GeometricLevel { .altitude = toolbox::AngleDeg { 10.0 } }, .sense = AltitudeSense::ABOVE,
    };
    const auto result = altitude_events(site(), requested, 2488070.125, 2488070.25);
    EXPECT_EQ(result.condition.condition.target, target);
    EXPECT_EQ(result.condition.source, ephemeris::source_of(target));
    EXPECT_TRUE(result.events.empty());
  }
  auto requested = condition();
  requested.target = ephemeris::Target::PLUTO;
  EXPECT_THROW((void)altitude_events(site(), requested, 2488070.125, 2488070.25), std::invalid_argument);
}

TEST(AltitudeEvents, AdmittedConditionAndSpanShapesMayExhaustSharedWork) {
  const std::vector<AltitudeCondition> requested(2048, condition());
  EXPECT_THROW((void)observing_windows(site(), requested, 2451545.0, 2451545.25), std::runtime_error);
  EXPECT_THROW((void)altitude_events(site(), condition(), 2451545.0, 2452057.0), std::runtime_error);
}

} // namespace astro::observer::test
