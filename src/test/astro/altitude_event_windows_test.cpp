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
#include <initializer_list>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "altitude_event_windows.hpp"

namespace astro::observer::detail::altitude_events::test {
namespace {

/** @brief Complete declared census for exact dyadic operational-composition fixtures. */
auto condition(const bool seed_above, const bool above, const std::initializer_list<Event> events) -> Condition {
  return { .complete = true, .seed_above = seed_above, .above = above, .events = events,
           .level = {}, .residual_error = 0.0 };
}

} // namespace

TEST(AltitudeEventWindows, EmptyConjunctionCoversTheWholeQuery) {
  const auto result = windows({}, 1.0, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.0);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 2.0);
  EXPECT_TRUE(result.states.empty());
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, CompletePrefixIsReplayedBeforeClipping) {
  const std::array conditions { condition(false, true, {
    { .jde_tt = 1.25, .change = 1, .origin_start_jde_tt = 1.0 },
    { .jde_tt = 1.75, .change = -1, .origin_start_jde_tt = 1.0 },
  }) };
  const auto result = windows(conditions, 1.5, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.5);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 1.75);
  EXPECT_EQ(result.states.front().initially_satisfied, 1U);
  EXPECT_EQ(result.states.front().currently_satisfied, 0U);
  EXPECT_EQ(result.companion_events, 1U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, SimultaneousOppositeConditionsNeverCreateAnIntermediateWindow) {
  const std::array conditions {
    condition(false, true, { { .jde_tt = 1.5, .change = 1, .origin_start_jde_tt = 1.0 } }),
    condition(false, false, { { .jde_tt = 1.5, .change = 1, .origin_start_jde_tt = 1.0 } }),
  };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  EXPECT_TRUE(result.windows.empty());
  ASSERT_EQ(result.reported_events.size(), 2U);
  EXPECT_EQ(result.reported_events.at(0).condition, 0U);
  EXPECT_EQ(result.reported_events.at(1).condition, 1U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, TangentsAndIrrelevantCrossingsDoNotFragmentAMaximalWindow) {
  const std::array conditions { condition(true, true, {
    { .jde_tt = 1.25, .change = 1, .origin_start_jde_tt = 1.0 },
    { .jde_tt = 1.5, .change = 0, .origin_start_jde_tt = 1.0 },
  }) };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.0);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 2.0);
  EXPECT_EQ(result.companion_events, 2U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, StartEventFromTheContainingPieceIsInTheInitialStateExactlyOnce) {
  const std::array conditions { condition(false, true, {
    { .jde_tt = 1.25, .change = 1, .origin_start_jde_tt = 1.0 },
    { .jde_tt = 1.75, .change = -1, .origin_start_jde_tt = 1.0 },
  }) };
  const auto result = windows(conditions, 1.25, 2.0, 1.0, 2.0);
  EXPECT_EQ(result.states.front().initially_satisfied, 1U);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.25);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 1.75);
  EXPECT_EQ(result.companion_events, 2U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, PrecedingPieceCompanionIsReportedAlongsideTheContainingSeed) {
  const std::array conditions { condition(true, true, {
    { .jde_tt = 1.25, .change = 1, .origin_start_jde_tt = 1.0 },
    { .jde_tt = 1.75, .change = -1, .origin_start_jde_tt = 1.25 },
  }) };
  const auto result = windows(conditions, 1.25, 2.0, 1.25, 2.0);
  EXPECT_EQ(result.states.front().initially_satisfied, 1U);
  ASSERT_EQ(result.reported_events.size(), 2U);
  EXPECT_DOUBLE_EQ(result.reported_events.front().jde_tt, 1.25);
  EXPECT_EQ(result.reported_events.front().change, 1);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, ExclusiveEndEventsAreValidatedButNotReportedOrReplayed) {
  const std::array conditions { condition(true, true, { { .jde_tt = 2.0, .change = -1, .origin_start_jde_tt = 1.75 } }) };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 2.0);
  EXPECT_EQ(result.states.front().currently_satisfied, 1U);
  EXPECT_EQ(result.companion_events, 0U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, UnresolvedPrefixCannotBecomeSuccessfulEmptyAfterClipping) {
  auto incomplete = condition(false, true, {});
  incomplete.complete = false;
  const std::array conditions { incomplete };
  EXPECT_THROW((void)windows(conditions, 1.75, 2.0, 1.0, 2.0), std::runtime_error);
}

TEST(AltitudeEventWindows, MalformedCensusesAndUncoveredQueriesFailWithoutPartialResults) {
  for (const auto& invalid : std::array {
      condition(false, true, {
        { .jde_tt = 1.5, .change = 1, .origin_start_jde_tt = 1.0 },
        { .jde_tt = 1.25, .change = -1, .origin_start_jde_tt = 1.0 },
      }),
      condition(false, true, {
        { .jde_tt = 1.5, .change = 1, .origin_start_jde_tt = 1.0 },
        { .jde_tt = 1.5, .change = -1, .origin_start_jde_tt = 1.0 },
      }),
      condition(false, true, { { .jde_tt = 1.5, .change = 2, .origin_start_jde_tt = 1.0 } }),
      condition(false, true, { { .jde_tt = std::numeric_limits<double>::quiet_NaN(), .change = 1, .origin_start_jde_tt = 1.0 } }),
  }) {
    const std::array conditions { invalid };
    EXPECT_THROW((void)windows(conditions, 1.0, 2.0, 1.0, 2.0), std::runtime_error);
  }
  EXPECT_THROW((void)windows({}, 0.75, 2.0, 1.0, 2.0), std::invalid_argument);
  EXPECT_THROW((void)windows({}, 2.0, 1.0, 1.0, 2.0), std::invalid_argument);
}

} // namespace astro::observer::detail::altitude_events::test
