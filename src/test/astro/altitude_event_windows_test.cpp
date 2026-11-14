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

/** @brief Exact dyadic crossings for operational window-composition fixtures. */
auto condition(const bool seed_above, const bool above, const std::initializer_list<Event> events) -> Condition {
  return { .seed_above = seed_above, .above = above, .events = events, .geometric_degrees = 0.0 };
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

TEST(AltitudeEventWindows, DetectedPrefixIsReplayedBeforeClipping) {
  const std::array conditions { condition(false, true, {
    { .jde_tt = 1.25, .change = 1 },
    { .jde_tt = 1.75, .change = -1 },
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
    condition(false, true, { { .jde_tt = 1.5, .change = 1 } }),
    condition(false, false, { { .jde_tt = 1.5, .change = 1 } }),
  };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  EXPECT_TRUE(result.windows.empty());
  ASSERT_EQ(result.reported_events.size(), 2U);
  EXPECT_EQ(result.reported_events.at(0).condition, 0U);
  EXPECT_EQ(result.reported_events.at(1).condition, 1U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, IrrelevantCrossingsDoNotFragmentAMaximalWindow) {
  const std::array conditions { condition(true, true, {
    { .jde_tt = 1.25, .change = 1 },
  }) };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.0);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 2.0);
  EXPECT_EQ(result.companion_events, 1U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, StartEventIsReportedAndInTheInitialStateExactlyOnce) {
  const std::array conditions { condition(false, true, {
    { .jde_tt = 1.25, .change = 1 },
    { .jde_tt = 1.75, .change = -1 },
  }) };
  const auto result = windows(conditions, 1.25, 2.0, 1.0, 2.0);
  EXPECT_EQ(result.states.front().initially_satisfied, 1U);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.25);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 1.75);
  EXPECT_EQ(result.companion_events, 2U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, AnAlignedStartCrossingOverridesTheEarlierSeed) {
  const std::array conditions { condition(true, true, {
    { .jde_tt = 1.25, .change = -1 },
    { .jde_tt = 1.75, .change = 1 },
  }) };
  const auto result = windows(conditions, 1.25, 2.0, 1.0, 2.0);
  EXPECT_EQ(result.states.front().initially_satisfied, 0U);
  ASSERT_EQ(result.reported_events.size(), 2U);
  EXPECT_DOUBLE_EQ(result.reported_events.front().jde_tt, 1.25);
  EXPECT_EQ(result.reported_events.front().change, -1);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.75);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, ExclusiveEndEventsAreValidatedButNotReportedOrReplayed) {
  const std::array conditions { condition(true, true, { { .jde_tt = 2.0, .change = -1 } }) };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 2.0);
  EXPECT_EQ(result.states.front().currently_satisfied, 1U);
  EXPECT_EQ(result.companion_events, 0U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, ShortPositiveWindowsAreNotMergedOrDiscarded) {
  const std::array conditions { condition(false, true, {
    { .jde_tt = 1.25, .change = 1 },
    { .jde_tt = 1.25 + 0x1p-18, .change = -1 },
  }) };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt - result.windows.front().start_jde_tt, 0x1p-18);
}

TEST(AltitudeEventWindows, MalformedCensusesAndUncoveredQueriesFailWithoutPartialResults) {
  for (const auto& invalid : std::array {
      condition(false, true, {
        { .jde_tt = 1.5, .change = 1 },
        { .jde_tt = 1.25, .change = -1 },
      }),
      condition(false, true, {
        { .jde_tt = 1.5, .change = 1 },
        { .jde_tt = 1.5, .change = -1 },
      }),
      condition(false, true, { { .jde_tt = 1.5, .change = 2 } }),
      condition(false, true, { { .jde_tt = 1.5, .change = 0 } }),
      condition(false, true, { { .jde_tt = std::numeric_limits<double>::quiet_NaN(), .change = 1 } }),
  }) {
    const std::array conditions { invalid };
    EXPECT_THROW((void)windows(conditions, 1.0, 2.0, 1.0, 2.0), std::runtime_error);
  }
  EXPECT_THROW((void)windows({}, 0.75, 2.0, 1.0, 2.0), std::invalid_argument);
  EXPECT_THROW((void)windows({}, 2.0, 1.0, 1.0, 2.0), std::invalid_argument);
}

} // namespace astro::observer::detail::altitude_events::test
