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
#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "altitude_event_query.hpp"

namespace astro::observer::detail::altitude_events::test {
namespace {

/** @brief Fixed geodetic site in degrees and km. */
auto site(const double latitude = 90.0) -> Observer {
  return {
    .location = { .latitude = toolbox::AngleDeg { latitude }, .longitude = toolbox::AngleDeg { -77.0 } },
    .elevation = toolbox::DistanceKm { 0.0 },
  };
}

/** @brief Geometric centre condition without atmospheric work. */
auto geometric(const uint32_t target = 0, const double level = -45.0) -> Request {
  return {
    .target = target, .level = level, .above = true, .convention = 2,
    .params = { .temperature_c = 0.0, .pressure_hpa = 0.0, .model = earth::refraction::Model::BENNETT },
  };
}

inline constexpr search::Interval QUARTER { .start_jde_tt = 2451545.0, .end_jde_tt = 2451545.25 };

} // namespace

TEST(AltitudeEventQuery, EmptyConjunctionNeedsNeitherSourceCoverageNorEvaluationBudget) {
  const search::Interval query { .start_jde_tt = 13689325.25, .end_jde_tt = 13689325.5 };
  search::Budget budget { .evaluations_remaining = 0, .candidates_remaining = 0 };
  const auto result = compute(Kind::WINDOWS, site(), query, {}, 0, budget);
  ASSERT_EQ(result.composed.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.composed.windows.front().start_jde_tt, query.start_jde_tt);
  EXPECT_DOUBLE_EQ(result.composed.windows.front().end_jde_tt, query.end_jde_tt);
  EXPECT_TRUE(result.conditions.empty());
  EXPECT_EQ(budget.evaluations_remaining, 0U);
  EXPECT_EQ(budget.candidates_remaining, 0U);
}

TEST(AltitudeEventQuery, QueryKindsHaveDistinctConditionShapes) {
  const std::array requests { geometric() };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute(Kind::ALTITUDE, site(), QUARTER, {}, 0, budget), std::invalid_argument);
  EXPECT_THROW((void)compute(Kind::TRANSITS, site(), QUARTER, requests, 0, budget), std::invalid_argument);
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
}

TEST(AltitudeEventQuery, HalfOpenCalendarEndpointAdmissionIsSeparateFromClosedProofAvailability) {
  const search::Interval query { .start_jde_tt = 13689325.25, .end_jde_tt = 13689325.5 };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute(Kind::TRANSITS, site(), query, {}, 0, budget), std::runtime_error);
  const search::Interval outside { .start_jde_tt = 13689325.5, .end_jde_tt = 13689325.75 };
  EXPECT_THROW((void)compute(Kind::TRANSITS, site(), outside, {}, 0, budget), std::invalid_argument);
}

TEST(AltitudeEventQuery, UnsupportedPhysicalTargetAndProviderDomainAreInvalid) {
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute(Kind::TRANSITS, site(), QUARTER, {}, 10, budget), std::invalid_argument);
  const search::Interval outside { .start_jde_tt = 2409543.0, .end_jde_tt = 2409543.25 };
  EXPECT_THROW((void)compute(Kind::TRANSITS, site(), outside, {}, 9, budget), std::invalid_argument);
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
}

TEST(AltitudeEventQuery, ConditionLimitIsCheckedBeforeTraversingOversizedInputs) {
  const std::vector<Request> requests(MAX_CONDITIONS + 1, geometric(10));
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute(Kind::WINDOWS, site(), QUARTER, requests, 0, budget), std::runtime_error);
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
}

TEST(AltitudeEventQuery, GeometricEndpointsAndMismatchedAtmosphericModelsAreInvalid) {
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  for (const double level : std::array { -90.0, 90.0, std::numeric_limits<double>::quiet_NaN() }) {
    const std::array requests { geometric(0, level) };
    EXPECT_THROW((void)compute(Kind::ALTITUDE, site(), QUARTER, requests, 0, budget), std::invalid_argument);
  }
  const std::array requests { Request {
    .target = 0, .level = 10.0, .above = true, .convention = 0,
    .params = { .temperature_c = 15.0, .pressure_hpa = 1013.25, .model = earth::refraction::Model::SAEMUNDSSON },
  } };
  EXPECT_THROW((void)compute(Kind::ALTITUDE, site(), QUARTER, requests, 0, budget), std::invalid_argument);
}

TEST(AltitudeEventQuery, AlignedStartsRetainPrecedingAndContainingPieceOrigins) {
  const std::array requests { geometric() };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  const auto scheduled = schedule(Kind::ALTITUDE, QUARTER, requests, 0, budget);
  ASSERT_EQ(scheduled.pieces.size(), 2U);
  EXPECT_DOUBLE_EQ(scheduled.census.start_jde_tt, QUARTER.start_jde_tt);
  EXPECT_DOUBLE_EQ(scheduled.pieces.at(0).start_jde_tt, QUARTER.start_jde_tt - 0.25);
  EXPECT_DOUBLE_EQ(scheduled.pieces.at(1).start_jde_tt, QUARTER.start_jde_tt);
  EXPECT_EQ(budget.candidates_remaining, 2046U);
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
}

TEST(AltitudeEventQuery, ClippedStartsUseTheirContainingCanonicalQuarterWithoutLookbehind) {
  const std::array requests { geometric() };
  const search::Interval query { .start_jde_tt = 2451545.125, .end_jde_tt = 2451545.1875 };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  const auto scheduled = schedule(Kind::ALTITUDE, query, requests, 0, budget);
  ASSERT_EQ(scheduled.pieces.size(), 1U);
  EXPECT_DOUBLE_EQ(scheduled.pieces.front().start_jde_tt, 2451545.0);
  EXPECT_DOUBLE_EQ(scheduled.pieces.front().end_jde_tt, 2451545.25);
  EXPECT_EQ(budget.candidates_remaining, 2047U);
}

TEST(AltitudeEventQuery, SharedBudgetIsConsumedAcrossRepeatedCompleteQueries) {
  const std::array requests { geometric() };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  const auto first = compute(Kind::ALTITUDE, site(), QUARTER, requests, 0, budget);
  ASSERT_EQ(first.composed.states.size(), 1U);
  EXPECT_EQ(first.composed.states.front().initially_satisfied, 1U);
  const std::size_t remaining = budget.evaluations_remaining;
  EXPECT_LT(remaining, 8192U);
  const auto second = compute(Kind::ALTITUDE, site(), QUARTER, requests, 0, budget);
  EXPECT_EQ(second.composed.states.front().initially_satisfied, first.composed.states.front().initially_satisfied);
  EXPECT_LT(budget.evaluations_remaining, remaining);
  EXPECT_EQ(first.composed.companion_events, first.composed.reported_events.size());
}

TEST(AltitudeEventQuery, ActualInverseChargeComesFromTheSameQueryBudget) {
  const Request request {
    .target = 0, .level = 10.0, .above = true, .convention = 1,
    .params = { .temperature_c = 15.0, .pressure_hpa = 1013.25, .model = earth::refraction::Model::SAEMUNDSSON },
  };
  search::Budget short_budget { .evaluations_remaining = SAEMUNDSSON_INVERSE_MAX_CHARGE - 1, .candidates_remaining = 2048 };
  EXPECT_THROW((void)resolve_level(request, short_budget), std::runtime_error);
  EXPECT_EQ(short_budget.evaluations_remaining, SAEMUNDSSON_INVERSE_MAX_CHARGE - 1);
  search::Budget budget { .evaluations_remaining = SAEMUNDSSON_INVERSE_MAX_CHARGE, .candidates_remaining = 2048 };
  const auto level = resolve_level(request, budget);
  EXPECT_EQ(budget.evaluations_remaining + level.evaluation_charge, SAEMUNDSSON_INVERSE_MAX_CHARGE);
  EXPECT_EQ(budget.candidates_remaining, 2048U);
}

TEST(AltitudeEventQuery, RealPlutoUnresolvedControlCannotBecomeSuccessfulEmpty) {
  const std::array requests { geometric(9, 48.605252016244656) };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute(Kind::ALTITUDE, site(30.0), QUARTER, requests, 0, budget), std::runtime_error);
}

TEST(AltitudeEventQuery, NonNearestEnvironmentFailsBeforeConsumingQueryWork) {
  const auto original = std::fegetround();
  if (std::fesetround(FE_DOWNWARD) != 0) {
    GTEST_SKIP() << "The platform cannot select the controlled rounding mode";
  }
  const std::array requests { geometric() };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute(Kind::ALTITUDE, site(), QUARTER, requests, 0, budget), std::runtime_error);
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
  EXPECT_EQ(budget.candidates_remaining, 2048U);
  ASSERT_EQ(std::fesetround(original), 0);
}

} // namespace astro::observer::detail::altitude_events::test
