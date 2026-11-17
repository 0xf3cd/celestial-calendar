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

#include "altitude_events.hpp"

namespace astro::observer::detail::altitude_events::test {
namespace {

using Direction = ephemeris::EventDirection;

/** @brief Exact dyadic crossings for operational window-composition fixtures. */
auto condition(const bool seed_above, const bool above, const std::initializer_list<Event> events) -> Condition {
  return {
    .seed_above = seed_above,
    .sense = above ? AltitudeSense::ABOVE : AltitudeSense::BELOW,
    .events = events,
    .geometric_degrees = 0.0,
  };
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
    { .jde_tt = 1.25, .direction = Direction::INCREASING },
    { .jde_tt = 1.75, .direction = Direction::DECREASING },
  }) };
  const auto result = windows(conditions, 1.5, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.5);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 1.75);
  EXPECT_TRUE(result.states.front().satisfied);
  auto working = result.states;
  (void)reconstruct_windows(conditions, result.reported_events, { .start_jde_tt = 1.5, .end_jde_tt = 2.0 }, working);
  EXPECT_FALSE(working.front().satisfied);
  EXPECT_EQ(result.companion_events, 1U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, SimultaneousOppositeConditionsNeverCreateAnIntermediateWindow) {
  const std::array conditions {
    condition(false, true, { { .jde_tt = 1.5, .direction = Direction::INCREASING } }),
    condition(false, false, { { .jde_tt = 1.5, .direction = Direction::INCREASING } }),
  };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  EXPECT_TRUE(result.windows.empty());
  ASSERT_EQ(result.reported_events.size(), 2U);
  EXPECT_EQ(result.reported_events.at(0).condition_index, 0U);
  EXPECT_EQ(result.reported_events.at(1).condition_index, 1U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, IrrelevantCrossingsDoNotFragmentAMaximalWindow) {
  const std::array conditions { condition(true, true, {
    { .jde_tt = 1.25, .direction = Direction::INCREASING },
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
    { .jde_tt = 1.25, .direction = Direction::INCREASING },
    { .jde_tt = 1.75, .direction = Direction::DECREASING },
  }) };
  const auto result = windows(conditions, 1.25, 2.0, 1.0, 2.0);
  EXPECT_TRUE(result.states.front().satisfied);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.25);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 1.75);
  EXPECT_EQ(result.companion_events, 2U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, AnAlignedStartCrossingOverridesTheEarlierSeed) {
  const std::array conditions { condition(true, true, {
    { .jde_tt = 1.25, .direction = Direction::DECREASING },
    { .jde_tt = 1.75, .direction = Direction::INCREASING },
  }) };
  const auto result = windows(conditions, 1.25, 2.0, 1.0, 2.0);
  EXPECT_FALSE(result.states.front().satisfied);
  ASSERT_EQ(result.reported_events.size(), 2U);
  EXPECT_DOUBLE_EQ(result.reported_events.front().jde_tt, 1.25);
  EXPECT_EQ(result.reported_events.front().direction, Direction::DECREASING);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().start_jde_tt, 1.75);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, ExclusiveEndEventsAreValidatedButNotReportedOrReplayed) {
  const std::array conditions { condition(true, true, { { .jde_tt = 2.0, .direction = Direction::DECREASING } }) };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt, 2.0);
  auto working = result.states;
  (void)reconstruct_windows(conditions, result.reported_events, { .start_jde_tt = 1.0, .end_jde_tt = 2.0 }, working);
  EXPECT_TRUE(working.front().satisfied);
  EXPECT_EQ(result.companion_events, 0U);
  EXPECT_EQ(result.companion_events, result.reported_events.size());
}

TEST(AltitudeEventWindows, ShortPositiveWindowsAreNotMergedOrDiscarded) {
  const std::array conditions { condition(false, true, {
    { .jde_tt = 1.25, .direction = Direction::INCREASING },
    { .jde_tt = 1.25 + 0x1p-18, .direction = Direction::DECREASING },
  }) };
  const auto result = windows(conditions, 1.0, 2.0, 1.0, 2.0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.windows.front().end_jde_tt - result.windows.front().start_jde_tt, 0x1p-18);
}

TEST(AltitudeEventWindows, MalformedCensusesAndUncoveredQueriesFailWithoutPartialResults) {
  for (const auto& invalid : std::array {
      condition(false, true, {
        { .jde_tt = 1.5, .direction = Direction::INCREASING },
        { .jde_tt = 1.25, .direction = Direction::DECREASING },
      }),
      condition(false, true, {
        { .jde_tt = 1.5, .direction = Direction::INCREASING },
        { .jde_tt = 1.5, .direction = Direction::DECREASING },
      }),
      condition(false, true, { { .jde_tt = 1.5, .direction = Direction::TANGENT } }),
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Deliberately reject an unnamed crossing direction.
      condition(false, true, { { .jde_tt = 1.5, .direction = static_cast<Direction>(255) } }),
      condition(false, true, { { .jde_tt = std::numeric_limits<double>::quiet_NaN(), .direction = Direction::INCREASING } }),
  }) {
    const std::array conditions { invalid };
    EXPECT_THROW((void)windows(conditions, 1.0, 2.0, 1.0, 2.0), std::runtime_error);
  }
  EXPECT_THROW((void)windows({}, 0.75, 2.0, 1.0, 2.0), std::invalid_argument);
  EXPECT_THROW((void)windows({}, 2.0, 1.0, 1.0, 2.0), std::invalid_argument);
}

} // namespace astro::observer::detail::altitude_events::test
