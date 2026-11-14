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
#include <cstddef>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "altitude_event_search.hpp"

namespace astro::observer::detail::altitude_events::test {
namespace {

inline constexpr double EPOCH = 2451545.0;
inline constexpr Domain DOMAIN { .first = EPOCH - 1.0, .last = EPOCH + 1.0 };
inline constexpr search::Interval QUERY { .start_jde_tt = EPOCH - 0.125, .end_jde_tt = EPOCH + 0.125 };

/** @brief Scalar-search budget independent of the public query wrapper. */
auto budget() -> search::Budget { return { .evaluations_remaining = 8192, .candidates_remaining = 2048 }; }

/** @brief Forward Saemundsson formula, used to check inverse levels in degrees. */
auto apparent(const double geometric, const refraction::Params& params) -> double {
  return geometric + refraction::detail::apply_tp_correction(
    refraction::saemundsson(toolbox::AngleDeg { geometric }), params
  ).deg();
}

} // namespace

TEST(AltitudeEventSearch, ExactZeroSamplesRetainBothCrossingDirections) {
  for (const double slope : { -1.0, 1.0 }) {
    auto work = budget();
    const auto f = [slope](const double at) { return slope * (at - EPOCH); };
    const auto found = scan(f, grid(QUERY, DOMAIN, 8192), true, work);
    ASSERT_EQ(found.events.size(), 1U);
    const auto event = found.events.front();
    EXPECT_NEAR(event.jde_tt, EPOCH, TIME_RESOLUTION_TT_DAYS);
    EXPECT_EQ(event.change, slope > 0.0 ? 1 : -1);
    EXPECT_EQ(f(event.jde_tt) > 0.0, slope > 0.0);
    EXPECT_EQ(found.initially_above, slope < 0.0);
    EXPECT_EQ(found.finally_above, slope > 0.0);
  }
}

TEST(AltitudeEventSearch, TiedCoarseSamplesResolveAPairWithinOneGridInterval) {
  // Analytic extrema at half a grid step; roots at centre +/- 2^-10 day.
  const double centre = EPOCH + (SAMPLE_STEP_TT_DAYS / 2.0);
  constexpr double RADIUS = 0x1p-10;
  for (const double orientation : { -1.0, 1.0 }) {
    auto work = budget();
    const auto f = [=](const double at) { return orientation * ((RADIUS * RADIUS) - ((at - centre) * (at - centre))); };
    const auto found = scan(f, grid(QUERY, DOMAIN, 8192), true, work);
    ASSERT_EQ(found.events.size(), 2U);
    EXPECT_NEAR(found.events.at(0).jde_tt, centre - RADIUS, TIME_RESOLUTION_TT_DAYS);
    EXPECT_NEAR(found.events.at(1).jde_tt, centre + RADIUS, TIME_RESOLUTION_TT_DAYS);
    EXPECT_EQ(found.events.at(0).change, orientation > 0.0 ? 1 : -1);
    EXPECT_EQ(found.events.at(1).change, -found.events.at(0).change);
  }
}

TEST(AltitudeEventSearch, NearContactsFailOnEitherSideOfTheThreshold) {
  for (const double height : { -CONTACT_RESIDUAL / 2.0, 0.0, CONTACT_RESIDUAL / 2.0 }) {
    auto work = budget();
    const auto f = [height](const double at) { return height - ((at - EPOCH) * (at - EPOCH)); };
    EXPECT_THROW((void)scan(f, grid(QUERY, DOMAIN, 8192), true, work), std::runtime_error);
  }
  for (const double height : { -2.0 * CONTACT_RESIDUAL, 2.0 * CONTACT_RESIDUAL }) {
    auto work = budget();
    const auto f = [height](const double at) { return height - ((at - EPOCH) * (at - EPOCH)); };
    const auto found = scan(f, grid(QUERY, DOMAIN, 8192), true, work);
    EXPECT_EQ(found.events.size(), height > 0.0 ? 2U : 0U);
  }
}

TEST(AltitudeEventSearch, FixedDomainEdgeSearchFindsTurnsWithoutAnOuterNeighbour) {
  const Domain domain { .first = EPOCH, .last = EPOCH + 0.25 };
  const search::Interval query { .start_jde_tt = domain.first, .end_jde_tt = domain.last };
  for (const double centre : { domain.first + 0x1p-12, domain.last - 0x1p-12 }) {
    auto work = budget();
    const auto f = [centre](const double at) { return 0x1p-18 - ((at - centre) * (at - centre)); };
    const auto found = scan(f, grid(query, domain, 8192), true, work);
    ASSERT_EQ(found.events.size(), 1U);
    const double root = centre < EPOCH + 0.125 ? centre + 0x1p-9 : centre - 0x1p-9;
    EXPECT_NEAR(found.events.front().jde_tt, root, TIME_RESOLUTION_TT_DAYS);
  }
}

TEST(AltitudeEventSearch, QuerySplitsReuseTheSameExtremaAndRootBrackets) {
  const double centre = EPOCH + (SAMPLE_STEP_TT_DAYS / 2.0);
  const auto f = [centre](const double at) { return 0x1p-20 - ((at - centre) * (at - centre)); };
  auto work = budget();
  const auto whole = scan(f, grid(QUERY, DOMAIN, 8192), true, work);
  ASSERT_EQ(whole.events.size(), 2U);
  for (const double split : { EPOCH, centre, whole.events.front().jde_tt, EPOCH + SAMPLE_STEP_TT_DAYS }) {
    std::vector<Event> joined;
    for (const auto query : std::array {
      search::Interval { .start_jde_tt = QUERY.start_jde_tt, .end_jde_tt = split },
      search::Interval { .start_jde_tt = split, .end_jde_tt = QUERY.end_jde_tt },
    }) {
      auto part_work = budget();
      const auto part = scan(f, grid(query, DOMAIN, 8192), true, part_work);
      for (const auto& event : part.events) {
        if (query.start_jde_tt <= event.jde_tt and event.jde_tt < query.end_jde_tt) {
          joined.push_back(event);
        }
      }
    }
    ASSERT_EQ(joined.size(), whole.events.size());
    for (std::size_t index = 0; index < joined.size(); ++index) {
      EXPECT_DOUBLE_EQ(joined.at(index).jde_tt, whole.events.at(index).jde_tt);
      EXPECT_EQ(joined.at(index).change, whole.events.at(index).change);
    }
  }
}

TEST(AltitudeEventSearch, EveryObservationStaysInsideTheDomainAndConsumesWork) {
  const Domain domain { .first = EPOCH + 0.001, .last = EPOCH + 0.249 };
  std::size_t calls = 0;
  auto work = budget();
  const auto f = [&](const double at) {
    EXPECT_GE(at, domain.first);
    EXPECT_LE(at, domain.last);
    ++calls;
    return at - EPOCH - 0.125;
  };
  const auto found = scan(
    f,
    grid({ .start_jde_tt = domain.first, .end_jde_tt = domain.last }, domain, 8192),
    true,
    work
  );
  ASSERT_EQ(found.events.size(), 1U);
  EXPECT_EQ(calls + work.evaluations_remaining, 8192U);
}

TEST(AltitudeEventSearch, SamplePreflightAndBothWorkCeilingsAreEnforced) {
  const auto planned = grid(QUERY, DOMAIN, 8192);
  std::size_t calls = 0;
  const auto f = [&](const double at) { ++calls; return at - EPOCH; };
  search::Budget short_work { .evaluations_remaining = planned.times.size() - 1, .candidates_remaining = 2048 };
  EXPECT_THROW((void)scan(f, planned, true, short_work), std::runtime_error);
  EXPECT_EQ(calls, 0U);
  search::Budget no_candidates { .evaluations_remaining = 8192, .candidates_remaining = 0 };
  EXPECT_THROW((void)scan(f, planned, false, no_candidates), std::runtime_error);
  auto exhausted = budget();
  exhausted.evaluations_remaining = 0;
  EXPECT_THROW((void)sample(f, EPOCH, exhausted), std::runtime_error);
}

TEST(AltitudeEventSearch, GridPreflightAcceptsExactlyTheRequiredSamples) {
  for (const Domain domain : { DOMAIN, Domain { .first = EPOCH + 0.001, .last = EPOCH + 0.249 } }) {
    const search::Interval query { .start_jde_tt = domain.first, .end_jde_tt = domain.last };
    const auto planned = grid(query, domain, 8192);
    EXPECT_NO_THROW((void)grid(query, domain, planned.times.size()));
    EXPECT_THROW((void)grid(query, domain, planned.times.size() - 1), std::runtime_error);
  }
}

TEST(AltitudeEventSearch, NonFiniteSamplesAreChargedAndNeverBecomeEmptySuccess) {
  auto work = budget();
  EXPECT_THROW(
    (void)sample([](const double) { return std::numeric_limits<double>::quiet_NaN(); }, EPOCH, work),
    std::runtime_error
  );
  EXPECT_EQ(work.evaluations_remaining, 8191U);
}

TEST(AltitudeEventSearch, ConstantArcsUseTheSameTotalSidePredicate) {
  for (const double value : { -1.0, 0.0, 1.0 }) {
    auto work = budget();
    const auto found = scan([value](const double) { return value; }, grid(QUERY, DOMAIN, 8192), true, work);
    EXPECT_TRUE(found.events.empty());
    EXPECT_EQ(found.initially_above, value > 0.0);
    EXPECT_EQ(found.finally_above, value > 0.0);
  }
}

TEST(AltitudeEventSearch, RefractionInversesMatchTheirForwardFormulaAtWeatherCorners) {
  for (const double temperature : { -90.0, 15.0, 60.0 }) {
    for (const double pressure : { 300.0, 1013.25, 1100.0 }) {
      for (const auto model : { refraction::Model::BENNETT, refraction::Model::SAEMUNDSSON }) {
        const refraction::Params params { .temperature_c = temperature, .pressure_hpa = pressure, .model = model };
        for (const double requested : { 0.0, 10.0, 89.0 }) {
          auto work = budget();
          const double geometric = inverse(requested, params, work);
          if (model == refraction::Model::BENNETT) {
            const double expected = requested - refraction::detail::apply_tp_correction(
              refraction::bennett(toolbox::AngleDeg { requested }), params
            ).deg();
            EXPECT_DOUBLE_EQ(geometric, expected);
            EXPECT_EQ(work.evaluations_remaining, 8191U);
          } else {
            EXPECT_NEAR(apparent(geometric, params), requested, LEVEL_RESOLUTION_DEGREES);
            EXPECT_LT(work.evaluations_remaining, 8192U);
            EXPECT_GE(work.evaluations_remaining, 8192U - 66U);
          }
        }
      }
    }
  }
}

TEST(AltitudeEventSearch, UnsupportedWeatherAndUnattainableLevelsAreInvalidInputs) {
  auto work = budget();
  EXPECT_THROW((void)inverse(10.0, { .temperature_c = 100.0 }, work), std::invalid_argument);
  EXPECT_THROW((void)inverse(10.0, { .pressure_hpa = 299.0 }, work), std::invalid_argument);
  EXPECT_THROW((void)inverse(-1.0, {}, work), std::invalid_argument);
  EXPECT_THROW((void)inverse(90.0, {}, work), std::invalid_argument);
  EXPECT_THROW((void)inverse(90.0, { .model = refraction::Model::SAEMUNDSSON }, work), std::invalid_argument);
}

} // namespace astro::observer::detail::altitude_events::test
