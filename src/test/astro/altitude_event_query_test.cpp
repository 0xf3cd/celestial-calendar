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
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "altitude_events.hpp"

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
auto geometric(const uint32_t target = 0, const double level = -45.0) -> AltitudeCondition {
  return {
    .target = static_cast<ephemeris::Target>(target),
    .level = GeometricLevel { .altitude = toolbox::AngleDeg { level } },
    .sense = AltitudeSense::ABOVE,
  };
}

/** @brief East-positive longitude placing the Sun on the upper meridian at a TT instant. */
auto solar_meridian_longitude(const double jde_tt) -> toolbox::AngleDeg {
  const auto position = ephemeris::equatorial_position(ephemeris::Target::SUN, jde_tt);
  const auto ut1 = delta_t::tt_to_ut1(delta_t::Model::ALGO5, julian_day::jde_to_tt(jde_tt));
  const auto theta = sidereal::greenwich_apparent(
    julian_day::ut1_to_jd(ut1),
    jde_tt,
    earth::nutation::Model::IAU_1980
  );
  return toolbox::AngleDeg { std::remainder((position.right_ascension - theta).deg(), 360.0) };
}

inline constexpr search::Interval QUARTER { .start_jde_tt = 2451545.0, .end_jde_tt = 2451545.25 };

} // namespace

TEST(AltitudeEventQuery, EmptyConjunctionNeedsNeitherSourceCoverageNorEvaluationBudget) {
  const search::Interval query { .start_jde_tt = 13689325.25, .end_jde_tt = 13689325.5 };
  search::Budget budget { .evaluations_remaining = 0, .candidates_remaining = 0 };
  const auto result = compute_conditions(site(), query, {}, budget);
  ASSERT_EQ(result.composed.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.composed.windows.front().start_jde_tt, query.start_jde_tt);
  EXPECT_DOUBLE_EQ(result.composed.windows.front().end_jde_tt, query.end_jde_tt);
  EXPECT_TRUE(result.conditions.empty());
  EXPECT_EQ(budget.evaluations_remaining, 0U);
  EXPECT_EQ(budget.candidates_remaining, 0U);
}

TEST(AltitudeEventQuery, QueryKindsHaveDistinctConditionShapes) {
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  constexpr bool altitude_accepts_a_list = std::is_invocable_v<
    decltype(astro::observer::altitude_events), Observer, std::span<const AltitudeCondition>, double, double
  >;
  constexpr bool transit_accepts_conditions = std::is_invocable_v<
    decltype(compute_transits), Observer, search::Interval, std::span<const AltitudeCondition>, search::Budget&
  >;
  static_assert(not altitude_accepts_a_list and not transit_accepts_conditions);
  EXPECT_FALSE(altitude_accepts_a_list);
  EXPECT_FALSE(transit_accepts_conditions);
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
}

TEST(AltitudeEventQuery, HalfOpenCalendarEndUsesOnlyEvaluableSamples) {
  const search::Interval query { .start_jde_tt = 13689325.25, .end_jde_tt = 13689325.5 };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_NO_THROW((void)compute_transits(site(), query, ephemeris::Target::SUN, budget));
  const search::Interval outside { .start_jde_tt = 13689325.5, .end_jde_tt = 13689325.75 };
  EXPECT_THROW((void)compute_transits(site(), outside, ephemeris::Target::SUN, budget), std::invalid_argument);
}

TEST(AltitudeEventQuery, UnsupportedPhysicalTargetAndProviderDomainAreInvalid) {
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute_transits(site(), QUARTER, static_cast<ephemeris::Target>(10), budget), std::invalid_argument);
  const search::Interval outside { .start_jde_tt = 2409543.0, .end_jde_tt = 2409543.25 };
  EXPECT_THROW((void)compute_transits(site(), outside, ephemeris::Target::PLUTO, budget), std::invalid_argument);
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
}

TEST(AltitudeEventQuery, ConditionLimitIsCheckedBeforeTraversingOversizedInputs) {
  const std::vector<AltitudeCondition> requests(MAX_CONDITIONS + 1, geometric(10));
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute_conditions(site(), QUARTER, requests, budget), std::runtime_error);
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
}

TEST(AltitudeEventQuery, GeometricEndpointsAndUnknownAtmosphericModelsAreInvalid) {
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  for (const double level : std::array { -90.0, 90.0, std::numeric_limits<double>::quiet_NaN() }) {
    const std::array requests { geometric(0, level) };
    EXPECT_THROW((void)compute_conditions(site(), QUARTER, requests, budget), std::invalid_argument);
  }
  const std::array requests { AltitudeCondition {
    .target = ephemeris::Target::SUN,
    .level = RefractedLevel {
      .altitude = toolbox::AngleDeg { 10.0 },
      .params = { .temperature_c = 15.0, .pressure_hpa = 1013.25, .model = static_cast<refraction::Model>(2) },
    },
    .sense = AltitudeSense::ABOVE,
  } };
  EXPECT_THROW((void)compute_conditions(site(), QUARTER, requests, budget), std::invalid_argument);
}

TEST(AltitudeEventQuery, AlignedStartsUseThePrecedingGridPointAndGlobalNeighbours) {
  const auto scheduled = schedule(ephemeris::Target::SUN, QUARTER, 8192);
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
  const auto scheduled = schedule(ephemeris::Target::SUN, query, 8192);
  ASSERT_EQ(scheduled.pieces.size(), 1U);
  EXPECT_DOUBLE_EQ(scheduled.pieces.front().start, 2451545.0);
  EXPECT_DOUBLE_EQ(scheduled.pieces.front().end, 2451545.0 + SAMPLE_STEP_TT_DAYS);
}

TEST(AltitudeEventQuery, SharedBudgetIsConsumedAcrossRepeatedQueries) {
  const std::array requests { geometric() };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  const auto first = compute_conditions(site(), QUARTER, requests, budget);
  ASSERT_EQ(first.composed.states.size(), 1U);
  EXPECT_EQ(first.composed.states.front().initially_satisfied, 1U);
  const std::size_t remaining = budget.evaluations_remaining;
  EXPECT_LT(remaining, 8192U);
  const auto second = compute_conditions(site(), QUARTER, requests, budget);
  EXPECT_EQ(second.composed.states.front().initially_satisfied, first.composed.states.front().initially_satisfied);
  EXPECT_LT(budget.evaluations_remaining, remaining);
  EXPECT_EQ(first.composed.companion_events, first.composed.reported_events.size());
}

TEST(AltitudeEventQuery, ActualInverseChargeComesFromTheSameQueryBudget) {
  const RefractedLevel request {
    .altitude = toolbox::AngleDeg { 10.0 },
    .params = { .temperature_c = 15.0, .pressure_hpa = 1013.25, .model = earth::refraction::Model::SAEMUNDSSON },
  };
  search::Budget short_budget { .evaluations_remaining = 2, .candidates_remaining = 2048 };
  EXPECT_THROW((void)resolve_level(request, short_budget), std::runtime_error);
  EXPECT_EQ(short_budget.evaluations_remaining, 0U);
  auto budget = query_budget();
  const auto level = resolve_level(request, budget);
  EXPECT_LT(level, request.altitude.deg());
  EXPECT_LT(budget.evaluations_remaining, 8192U);
  EXPECT_EQ(budget.candidates_remaining, 2048U);
}

TEST(AltitudeEventQuery, DetectedPlutoContactCannotBecomeSuccessfulEmpty) {
  const std::array requests { geometric(9, 48.605252016244656) };
  search::Budget budget { .evaluations_remaining = 8192, .candidates_remaining = 2048 };
  EXPECT_THROW((void)compute_conditions(site(30.0), QUARTER, requests, budget), std::runtime_error);
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
    const auto domain = sample_domain(static_cast<ephemeris::Target>(input.target));
    const auto planned = schedule(static_cast<ephemeris::Target>(input.target), query, 8192);
    for (const auto& piece : planned.pieces) {
      for (const double at : piece.times) {
        EXPECT_GE(at, domain.first);
        EXPECT_LE(at, domain.last);
      }
    }
    const std::array requests { geometric(input.target, 0.0) };
    auto work = query_budget();
    EXPECT_NO_THROW((void)compute_conditions(site(30.0), query, requests, work));
  }
}

TEST(AltitudeEventQuery, ModelJoinSideChangesFailButOrdinaryCrossingsAcrossTheDateSucceed) {
  for (const int32_t year : { 500, 1600, 1860, 2005 }) {
    const double join = julian_day::tt_to_jde(calendar::Datetime { std::chrono::year { year } / std::chrono::January / 1, 0.0 });
    const Altitude provider {
      .target = ephemeris::Target::SUN, .observer = site(30.0), .level_sine = 0.0,
      .domain = sample_domain(ephemeris::Target::SUN),
    };
    const auto left = observe(provider.target, provider.observer, provider.domain, search::lower(join));
    const auto right = observe(provider.target, provider.observer, provider.domain, join);
    const double sine = std::midpoint(left.up / left.q, right.up / right.q);
    const double level = toolbox::AngleRad { std::asin(sine) }.deg();
    const search::Interval query { .start_jde_tt = join - 0.125, .end_jde_tt = join + 0.125 };
    const std::array crossing { geometric(0, level) };
    auto work = query_budget();
    EXPECT_THROW((void)compute_conditions(site(30.0), query, crossing, work), std::runtime_error);
    const std::array ordinary { geometric(0, 0.0) };
    work = query_budget();
    EXPECT_NO_THROW((void)compute_conditions(site(30.0), query, ordinary, work));
  }
}

TEST(AltitudeEventQuery, TransitModelJoinSideChangesFailButNearbyPassagesSucceed) {
  for (const int32_t year : { 1600, 2005 }) {
    const double join = julian_day::tt_to_jde(
      calendar::Datetime { std::chrono::year { year } / std::chrono::January / 1, 0.0 }
    );
    const double longitude = std::midpoint(
      solar_meridian_longitude(search::lower(join)).deg(),
      solar_meridian_longitude(join).deg()
    );
    const Observer observer {
      .location = { .latitude = toolbox::AngleDeg { 30.0 }, .longitude = toolbox::AngleDeg { longitude } },
      .elevation = toolbox::DistanceKm { 0.0 },
    };
    const Meridian provider {
      .target = ephemeris::Target::SUN,
      .observer = observer,
      .domain = sample_domain(ephemeris::Target::SUN),
    };
    const auto before = observe(provider.target, provider.observer, provider.domain, search::lower(join));
    const auto after = observe(provider.target, provider.observer, provider.domain, join);
    ASSERT_NE(before.meridian > 0.0, after.meridian > 0.0);
    ASSERT_GT(before.forward, 0.0);
    ASSERT_GT(after.forward, 0.0);

    const search::Interval query { .start_jde_tt = join - 0.125, .end_jde_tt = join + 0.125 };
    auto work = query_budget();
    try {
      (void)compute_transits(observer, query, ephemeris::Target::SUN, work);
      FAIL() << "A model jump cannot become a transit";
    } catch (const std::runtime_error& error) {
      EXPECT_STREQ(error.what(), "Meridian crossing coincides with a Delta T model join");
    }

    const Observer ordinary {
      .location = { .latitude = observer.location.latitude, .longitude = toolbox::AngleDeg { longitude + 1.0 } },
      .elevation = observer.elevation,
    };
    work = query_budget();
    const auto result = compute_transits(ordinary, query, ephemeris::Target::SUN, work);
    EXPECT_EQ(result.size(), 1U);
  }
}

TEST(AltitudeEventQuery, UnresolvedTransitForwardSideFailsAtAnAdmittedDistantObserver) {
  constexpr double EPOCH = 2451545.0;
  constexpr double PROBE_HEIGHT_KM = 1e8;
  const auto longitude = solar_meridian_longitude(EPOCH);
  const auto provider_at = [longitude](const double height_km) {
    return Meridian {
      .target = ephemeris::Target::SUN,
      .observer = {
        .location = { .latitude = toolbox::AngleDeg { 30.0 }, .longitude = longitude },
        .elevation = toolbox::DistanceKm { height_km },
      },
      .domain = sample_domain(ephemeris::Target::SUN),
    };
  };
  const auto forward = [&](const Meridian& provider, const double at) {
    return observe(provider.target, provider.observer, provider.domain, at).forward;
  };
  const double surface = forward(provider_at(0.0), EPOCH);
  const double distant = forward(provider_at(PROBE_HEIGHT_KM), EPOCH);
  // The forward component is affine in observer height; its zero need not make the full direction zero.
  const double height = PROBE_HEIGHT_KM * surface / (surface - distant);
  const auto provider = provider_at(height);
  const search::Interval query { .start_jde_tt = EPOCH - 0.125, .end_jde_tt = EPOCH + 0.125 };
  const auto planned = schedule(ephemeris::Target::SUN, query, 8192);
  auto work = query_budget();
  const auto found = scan(provider, planned.pieces.front(), TurnSearch::OMIT, work);
  ASSERT_EQ(found.events.size(), 1U);
  EXPECT_LE(std::abs(forward(provider, found.events.front().jde_tt)), CONTACT_RESIDUAL);

  work = query_budget();
  try {
    (void)compute_transits(provider.observer, query, ephemeris::Target::SUN, work);
    FAIL() << "An unresolved forward side cannot become an empty or nonempty success";
  } catch (const std::runtime_error& error) {
    EXPECT_STREQ(error.what(), "The meridian forward side is unresolved");
  }
  work = query_budget();
  const auto control = compute_transits(
    provider_at(height * 0.99).observer,
    query,
    ephemeris::Target::SUN,
    work
  );
  EXPECT_EQ(control.size(), 1U);
}

TEST(AltitudeEventQuery, TransitAtAnIntervalSplitIsOwnedOnlyByTheRightQuery) {
  const search::Interval query { .start_jde_tt = 2451545.0, .end_jde_tt = 2451546.0 };
  auto work = query_budget();
  const auto whole = compute_transits(site(30.0), query, ephemeris::Target::SUN, work);
  ASSERT_EQ(whole.size(), 1U);
  const double split = whole.front();
  ASSERT_GT(split, query.start_jde_tt);
  ASSERT_LT(split, query.end_jde_tt);

  work = query_budget();
  const auto left = compute_transits(
    site(30.0),
    { .start_jde_tt = query.start_jde_tt, .end_jde_tt = split },
    ephemeris::Target::SUN,
    work
  );
  EXPECT_TRUE(left.empty());
  work = query_budget();
  const auto right = compute_transits(
    site(30.0),
    { .start_jde_tt = split, .end_jde_tt = query.end_jde_tt },
    ephemeris::Target::SUN,
    work
  );
  EXPECT_EQ(right, whole);
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
  EXPECT_THROW((void)compute_conditions(site(), QUARTER, requests, budget), std::runtime_error);
  EXPECT_EQ(budget.evaluations_remaining, 8192U);
  EXPECT_EQ(budget.candidates_remaining, 2048U);
  ASSERT_EQ(std::fesetround(original), 0);
}

} // namespace astro::observer::detail::altitude_events::test
