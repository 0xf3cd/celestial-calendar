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

#include <algorithm>
#include <array>
#include <cfenv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
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

TEST(AltitudeEventQuery, HalfOpenCalendarEndUsesOnlyEvaluableSamples) {
  const search::Interval query { .start_jde_tt = 13689325.25, .end_jde_tt = 13689325.5 };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_NO_THROW((void)compute(Kind::TRANSITS, site(), query, {}, 0, budget));
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

TEST(AltitudeEventQuery, AlignedStartsUseThePrecedingGridPointAndGlobalNeighbours) {
  const auto scheduled = schedule(0, QUARTER, 8192);
  ASSERT_EQ(scheduled.pieces.size(), 1U);
  const auto& piece = scheduled.pieces.front();
  EXPECT_DOUBLE_EQ(piece.start, QUARTER.start_jde_tt - SAMPLE_STEP_TT_DAYS);
  EXPECT_DOUBLE_EQ(piece.end, QUARTER.end_jde_tt);
  EXPECT_DOUBLE_EQ(piece.times.front(), piece.start - SAMPLE_STEP_TT_DAYS);
  EXPECT_DOUBLE_EQ(piece.times.back(), piece.end + SAMPLE_STEP_TT_DAYS);
  EXPECT_EQ(scheduled.sample_count, piece.times.size());
}

TEST(AltitudeEventQuery, UnalignedStartsUseTheContainingGlobalGridInterval) {
  const search::Interval query { .start_jde_tt = 2451545.001, .end_jde_tt = 2451545.005 };
  const auto scheduled = schedule(0, query, 8192);
  ASSERT_EQ(scheduled.pieces.size(), 1U);
  EXPECT_DOUBLE_EQ(scheduled.pieces.front().start, 2451545.0);
  EXPECT_DOUBLE_EQ(scheduled.pieces.front().end, 2451545.0 + SAMPLE_STEP_TT_DAYS);
}

TEST(AltitudeEventQuery, SharedBudgetIsConsumedAcrossRepeatedQueries) {
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
  search::Budget short_budget { .evaluations_remaining = 2, .candidates_remaining = 2048 };
  EXPECT_THROW((void)resolve_level(request, short_budget), std::runtime_error);
  EXPECT_EQ(short_budget.evaluations_remaining, 0U);
  auto budget = query_budget();
  const auto level = resolve_level(request, budget);
  EXPECT_LT(level, request.level);
  EXPECT_LT(budget.evaluations_remaining, 8192U);
  EXPECT_EQ(budget.candidates_remaining, 2048U);
}

TEST(AltitudeEventQuery, DetectedPlutoContactCannotBecomeSuccessfulEmpty) {
  const std::array requests { geometric(9, 48.605252016244656) };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute(Kind::ALTITUDE, site(30.0), QUARTER, requests, 0, budget), std::runtime_error);
}

TEST(AltitudeEventQuery, AllProviderAndCalendarEdgesKeepEverySampleInsideTheSelectedDomain) {
  struct Case { uint32_t target; double start; double end; };
  for (const auto input : std::array {
    Case { .target = 0, .start = 1867522.5, .end = 1867522.75 },
    Case { .target = 0, .start = 13689325.25, .end = 13689325.5 },
    Case { .target = 9, .start = 2409543.5, .end = 2409543.75 },
    Case { .target = 9, .start = 2488069.25, .end = 2488069.5 },
  }) {
    const search::Interval query { .start_jde_tt = input.start, .end_jde_tt = input.end };
    const auto domain = sample_domain(input.target);
    const auto planned = schedule(input.target, query, 8192);
    for (const auto& piece : planned.pieces) {
      for (const double at : piece.times) {
        EXPECT_GE(at, domain.first);
        EXPECT_LE(at, domain.last);
      }
    }
    const std::array requests { geometric(input.target, 0.0) };
    auto work = query_budget();
    EXPECT_NO_THROW((void)compute(Kind::ALTITUDE, site(30.0), query, requests, 0, work));
  }
}

TEST(AltitudeEventQuery, ModelJoinSideChangesFailButOrdinaryCrossingsAcrossTheDateSucceed) {
  for (const int32_t year : { 500, 1600, 1860, 2005 }) {
    const double join = julian_day::tt_to_jde(calendar::Datetime { std::chrono::year { year } / std::chrono::January / 1, 0.0 });
    const Altitude provider {
      .target = ephemeris::Target::SUN, .observer = site(30.0), .level_sine = 0.0,
      .domain = sample_domain(0), .meridian = false,
    };
    const auto left = provider.observe(search::lower(join));
    const auto right = provider.observe(join);
    const double sine = std::midpoint(left.up / left.q, right.up / right.q);
    const double level = toolbox::AngleRad { std::asin(sine) }.deg();
    const search::Interval query { .start_jde_tt = join - 0.125, .end_jde_tt = join + 0.125 };
    const std::array crossing { geometric(0, level) };
    auto work = query_budget();
    EXPECT_THROW((void)compute(Kind::ALTITUDE, site(30.0), query, crossing, 0, work), std::runtime_error);
    const std::array ordinary { geometric(0, 0.0) };
    work = query_budget();
    EXPECT_NO_THROW((void)compute(Kind::ALTITUDE, site(30.0), query, ordinary, 0, work));
  }
}

TEST(AltitudeEventQuery, ModelJoinListMatchesTheInheritedPolynomialDiscontinuities) {
  for (const int32_t year : delta_t::algo5::DISCONTINUITY_YEARS) {
    const auto at = static_cast<double>(year);
    EXPECT_GT(std::abs(delta_t::algo5::compute(at) - delta_t::algo5::compute(search::lower(at))), 1e-5);
  }
  for (int32_t year = 401; year < 2100; ++year) {
    if (std::ranges::find(delta_t::algo5::DISCONTINUITY_YEARS, year) != delta_t::algo5::DISCONTINUITY_YEARS.end()) {
      continue;
    }
    const auto at = static_cast<double>(year);
    EXPECT_NEAR(delta_t::algo5::compute(at), delta_t::algo5::compute(search::lower(at)), 1e-7);
  }
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
