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

// #303: the ephemeris-event C exports driven across the real `extern "C"` boundary against
// the header-only core, covering the shared fixture set replayed by the Python/JavaScript
// binding consumers through toolbox/bindings_golden.json:
//   sun-zero / moon-fixed / mercury-triple / mercury-stations / pair-minus-60 / pair-plus-60.
// The Mercury and Sun windows and the Moon-Sun ±60° pair replay the core suite's PyMeeus
// anchors (src/test/astro/ephemeris_events_test.cpp).

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "celestial.h"
#include "ephemeris_events.hpp"
#include "toolbox.hpp"

namespace lib::events::test {
namespace {

using astro::ephemeris::EventDirection;
using astro::ephemeris::Motion;
using astro::ephemeris::Target;

constexpr double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
constexpr double INFINITY_VALUE = std::numeric_limits<double>::infinity();

// Fixture windows (see the file header): TT days, JDE(TT).
constexpr double MERCURY_WINDOW_START = 2460735.5;
constexpr double MERCURY_WINDOW_END = 2460785.5;
constexpr double PAIR_WINDOW_START = 2460676.5;
constexpr double PAIR_WINDOW_END = 2460707.5;
constexpr double MOON_FIXED_LEVEL = 137.5;

static_assert(std::is_standard_layout_v<LongitudeEventRecord> and std::is_trivially_copyable_v<LongitudeEventRecord>);
static_assert(std::is_standard_layout_v<StationEventRecord> and std::is_trivially_copyable_v<StationEventRecord>);
static_assert(std::is_standard_layout_v<LongitudeDifferenceEventRecord>
              and std::is_trivially_copyable_v<LongitudeDifferenceEventRecord>);
static_assert(sizeof(LongitudeEventRecord) == 32 and alignof(LongitudeEventRecord) == 8);
static_assert(sizeof(StationEventRecord) == 32 and alignof(StationEventRecord) == 8);
static_assert(sizeof(LongitudeDifferenceEventRecord) == 56 and alignof(LongitudeDifferenceEventRecord) == 8);

// The exported identities pin directly to the core targets — not only through the chart mirror.
static_assert(EVENT_TARGET_SUN == static_cast<uint32_t>(Target::SUN));
static_assert(EVENT_TARGET_MOON == static_cast<uint32_t>(Target::MOON));
static_assert(EVENT_TARGET_MERCURY == static_cast<uint32_t>(Target::MERCURY));
static_assert(EVENT_TARGET_VENUS == static_cast<uint32_t>(Target::VENUS));
static_assert(EVENT_TARGET_MARS == static_cast<uint32_t>(Target::MARS));
static_assert(EVENT_TARGET_JUPITER == static_cast<uint32_t>(Target::JUPITER));
static_assert(EVENT_TARGET_SATURN == static_cast<uint32_t>(Target::SATURN));
static_assert(EVENT_TARGET_URANUS == static_cast<uint32_t>(Target::URANUS));
static_assert(EVENT_TARGET_NEPTUNE == static_cast<uint32_t>(Target::NEPTUNE));
static_assert(EVENT_TARGET_PLUTO == static_cast<uint32_t>(Target::PLUTO));
static_assert(EVENT_TARGET_MEAN_ASCENDING == static_cast<uint32_t>(Target::MEAN_ASCENDING));
static_assert(EVENT_TARGET_MEAN_DESCENDING == static_cast<uint32_t>(Target::MEAN_DESCENDING));
static_assert(EVENT_TARGET_TRUE_ASCENDING == static_cast<uint32_t>(Target::TRUE_ASCENDING));
static_assert(EVENT_TARGET_TRUE_DESCENDING == static_cast<uint32_t>(Target::TRUE_DESCENDING));

static_assert(EVENT_DIRECTION_INCREASING == static_cast<uint32_t>(EventDirection::INCREASING));
static_assert(EVENT_DIRECTION_DECREASING == static_cast<uint32_t>(EventDirection::DECREASING));
static_assert(EVENT_DIRECTION_TANGENT == static_cast<uint32_t>(EventDirection::TANGENT));
static_assert(EVENT_MOTION_DIRECT == static_cast<uint32_t>(Motion::DIRECT));
static_assert(EVENT_MOTION_RETROGRADE == static_cast<uint32_t>(Motion::RETROGRADE));

static_assert(std::is_same_v<decltype(&longitude_events),
                             uint32_t (*)(uint32_t, double, double, double, uint32_t *,
                                          LongitudeEventRecord *, uint32_t)>);
static_assert(std::is_same_v<decltype(&longitude_stations),
                             uint32_t (*)(uint32_t, double, double, uint32_t *,
                                          StationEventRecord *, uint32_t)>);
static_assert(std::is_same_v<decltype(&longitude_difference_events),
                             uint32_t (*)(uint32_t, uint32_t, double, double, double, uint32_t *,
                                          LongitudeDifferenceEventRecord *, uint32_t)>);

auto expect_same_bits(const char * const label, const double actual, const double expected) -> void {
  EXPECT_EQ(std::bit_cast<uint64_t>(actual), std::bit_cast<uint64_t>(expected)) << label;
}

auto query_longitude_events(
  const uint32_t target_code, const double level,
  const double start_jde_tt, const double end_jde_tt
) -> std::pair<uint32_t, std::vector<LongitudeEventRecord>> {
  uint32_t total = 0;
  EXPECT_EQ(longitude_events(target_code, level, start_jde_tt, end_jde_tt, &total, nullptr, 0), 0U);
  EXPECT_STREQ(last_error(), ""); // a count-only pass is a legal success and clears the channel
  std::vector<LongitudeEventRecord> slots(total);
  EXPECT_EQ(longitude_events(target_code, level, start_jde_tt, end_jde_tt, &total, slots.data(), total), total);
  EXPECT_STREQ(last_error(), "");
  return { total, std::move(slots) };
}

auto expect_longitude_events_match_core(
  const uint32_t target_code, const double level,
  const double start_jde_tt, const double end_jde_tt
) -> void {
  const auto [total, slots] = query_longitude_events(target_code, level, start_jde_tt, end_jde_tt);
  const auto expected = astro::ephemeris::longitude_events(
    static_cast<Target>(target_code), astro::toolbox::AngleDeg { level }, start_jde_tt, end_jde_tt
  );
  ASSERT_EQ(total, expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    SCOPED_TRACE(index);
    const auto& record = slots.at(index);
    const auto& event = expected.at(index);
    expect_same_bits("jde_tt", record.jde_tt, event.jde_tt);
    expect_same_bits("longitude", record.longitude_deg, event.longitude.deg());
    expect_same_bits("rate", record.longitude_rate_deg_per_tt_day, event.longitude_rate.deg_per_tt_day);
    EXPECT_EQ(record.direction_code, static_cast<uint32_t>(event.direction));
    if (index > 0) {
      EXPECT_GT(record.jde_tt, slots.at(index - 1).jde_tt); // strict chronological ordering
    }
  }
}

} // namespace

TEST(EventCAbi, LongitudeEventsMatchCoreAcrossTargets) {
  expect_longitude_events_match_core(EVENT_TARGET_SUN, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END);
  expect_longitude_events_match_core(EVENT_TARGET_MOON, MOON_FIXED_LEVEL, MERCURY_WINDOW_START, MERCURY_WINDOW_END);
  expect_longitude_events_match_core(EVENT_TARGET_MERCURY, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END);
}

TEST(EventCAbi, StationsMatchCoreWithBothMotionDirections) {
  const auto [total, slots] = [] {
    uint32_t count = 0;
    EXPECT_EQ(longitude_stations(EVENT_TARGET_MERCURY, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &count, nullptr, 0), 0U);
    EXPECT_STREQ(last_error(), "");
    std::vector<StationEventRecord> records(count);
    EXPECT_EQ(
      longitude_stations(EVENT_TARGET_MERCURY, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &count, records.data(), count),
      count
    );
    EXPECT_STREQ(last_error(), "");
    return std::pair { count, std::move(records) };
  }();

  const auto expected = astro::ephemeris::stations(Target::MERCURY, MERCURY_WINDOW_START, MERCURY_WINDOW_END);
  ASSERT_EQ(total, expected.size());
  ASSERT_EQ(total, 2U); // one ingress into and one egress from retrograde motion
  bool saw_direct_to_retrograde = false;
  bool saw_retrograde_to_direct = false;
  for (std::size_t index = 0; index < expected.size(); ++index) {
    SCOPED_TRACE(index);
    const auto& record = slots.at(index);
    const auto& event = expected.at(index);
    expect_same_bits("jde_tt", record.jde_tt, event.jde_tt);
    expect_same_bits("longitude", record.longitude_deg, event.longitude.deg());
    expect_same_bits("rate", record.longitude_rate_deg_per_tt_day, event.longitude_rate.deg_per_tt_day);
    EXPECT_EQ(record.motion_before_code, static_cast<uint32_t>(event.motion_before));
    EXPECT_EQ(record.motion_after_code, static_cast<uint32_t>(event.motion_after));
    saw_direct_to_retrograde = saw_direct_to_retrograde
                               or (record.motion_before_code == EVENT_MOTION_DIRECT
                                   and record.motion_after_code == EVENT_MOTION_RETROGRADE);
    saw_retrograde_to_direct = saw_retrograde_to_direct
                               or (record.motion_before_code == EVENT_MOTION_RETROGRADE
                                   and record.motion_after_code == EVENT_MOTION_DIRECT);
  }
  EXPECT_TRUE(saw_direct_to_retrograde);
  EXPECT_TRUE(saw_retrograde_to_direct);
}

TEST(EventCAbi, DifferenceEventsMatchCoreForBothOrientations) {
  const auto query = [](const uint32_t first, const uint32_t second, const double level) {
    uint32_t count = 0;
    EXPECT_EQ(
      longitude_difference_events(first, second, level, PAIR_WINDOW_START, PAIR_WINDOW_END, &count, nullptr, 0), 0U
    );
    EXPECT_STREQ(last_error(), "");
    std::vector<LongitudeDifferenceEventRecord> records(count);
    EXPECT_EQ(
      longitude_difference_events(
        first, second, level, PAIR_WINDOW_START, PAIR_WINDOW_END, &count, records.data(), count
      ),
      count
    );
    EXPECT_STREQ(last_error(), "");
    return std::pair { count, std::move(records) };
  };

  for (const double level : { -60.0, 60.0 }) {
    SCOPED_TRACE(level);
    const auto [total, slots] = query(EVENT_TARGET_MOON, EVENT_TARGET_SUN, level);
    const auto expected = astro::ephemeris::longitude_difference_events(
      Target::MOON, Target::SUN, astro::toolbox::AngleDeg { level }, PAIR_WINDOW_START, PAIR_WINDOW_END
    );
    ASSERT_EQ(total, expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
      const auto& record = slots.at(index);
      const auto& event = expected.at(index);
      expect_same_bits("jde_tt", record.jde_tt, event.jde_tt);
      expect_same_bits("longitude_a", record.longitude_a_deg, event.longitude_a.deg());
      expect_same_bits("longitude_b", record.longitude_b_deg, event.longitude_b.deg());
      expect_same_bits("rate_a", record.longitude_rate_a_deg_per_tt_day, event.longitude_rate_a.deg_per_tt_day);
      expect_same_bits("rate_b", record.longitude_rate_b_deg_per_tt_day, event.longitude_rate_b.deg_per_tt_day);
      expect_same_bits("signed_difference", record.signed_difference_deg, event.signed_difference.deg());
      EXPECT_EQ(record.direction_code, static_cast<uint32_t>(event.direction));
    }
  }

  // Reversing the pair reverses the level and direction: the two requests select the same instants.
  const auto [forward_total, forward] = query(EVENT_TARGET_SUN, EVENT_TARGET_MOON, 60.0);
  const auto [backward_total, backward] = query(EVENT_TARGET_MOON, EVENT_TARGET_SUN, -60.0);
  ASSERT_EQ(forward_total, backward_total);
  for (std::size_t index = 0; index < backward.size(); ++index) {
    SCOPED_TRACE(index);
    expect_same_bits("reversed pair jde_tt", forward.at(index).jde_tt, backward.at(index).jde_tt);
    EXPECT_NE(forward.at(index).direction_code, EVENT_DIRECTION_TANGENT);
    EXPECT_NE(backward.at(index).direction_code, EVENT_DIRECTION_TANGENT);
    EXPECT_NE(forward.at(index).direction_code, backward.at(index).direction_code);
  }
}

TEST(EventCAbi, HalfOpenOwnershipAtReportedTimes) {
  const auto [total, slots] = query_longitude_events(
    EVENT_TARGET_MERCURY, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END
  );
  ASSERT_EQ(total, 3U);
  const double first = slots.at(0).jde_tt;

  // An event at `end` does not belong to the query; the next representable end admits it.
  const auto [excluded_total, excluded] = query_longitude_events(
    EVENT_TARGET_MERCURY, 0.0, MERCURY_WINDOW_START, first
  );
  EXPECT_EQ(excluded_total, 0U);
  const auto [included_total, included] = query_longitude_events(
    EVENT_TARGET_MERCURY, 0.0, MERCURY_WINDOW_START, std::nextafter(first, INFINITY_VALUE)
  );
  ASSERT_EQ(included_total, 1U);
  EXPECT_EQ(std::bit_cast<uint64_t>(included.at(0).jde_tt), std::bit_cast<uint64_t>(first));

  // An event at `start` belongs to the query.
  const auto [right_total, right] = query_longitude_events(EVENT_TARGET_MERCURY, 0.0, first, MERCURY_WINDOW_END);
  EXPECT_EQ(right_total, 3U);
  EXPECT_EQ(std::bit_cast<uint64_t>(right.at(0).jde_tt), std::bit_cast<uint64_t>(first));
}

TEST(EventCAbi, CapacityTruncationReportsFullTotal) {
  uint32_t total = 0;
  ASSERT_EQ(
    longitude_events(EVENT_TARGET_MERCURY, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &total, nullptr, 0), 0U
  );
  ASSERT_EQ(total, 3U);

  // Two slots for three events: the fill truncates, the total stays observable, the tail is untouched.
  std::array<LongitudeEventRecord, 3> slots {};
  for (auto& slot : slots) {
    slot.jde_tt = -777.0;
  }
  uint32_t truncated_total = 0;
  EXPECT_EQ(
    longitude_events(
      EVENT_TARGET_MERCURY, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &truncated_total, slots.data(), 2
    ),
    2U
  );
  EXPECT_EQ(truncated_total, 3U);
  EXPECT_EQ(slots.at(2).jde_tt, -777.0); // beyond the written count, the caller's storage is untouched
}

TEST(EventCAbi, ConstantRelationsAreEmptyOrInvalid) {
  uint32_t total = 0;
  // The antipodal node relation is constantly 180°: a 0° target misses throughout (legal empty)…
  EXPECT_EQ(
    longitude_difference_events(
      EVENT_TARGET_MEAN_ASCENDING, EVENT_TARGET_MEAN_DESCENDING, 0.0,
      PAIR_WINDOW_START, PAIR_WINDOW_END, &total, nullptr, 0
    ),
    0U
  );
  EXPECT_EQ(total, 0U);
  EXPECT_STREQ(last_error(), "");

  // …while ±180° selects the level itself and throws.
  for (const double level : { 180.0, -180.0 }) {
    total = 0xDEADBEEFU;
    EXPECT_EQ(
      longitude_difference_events(
        EVENT_TARGET_MEAN_ASCENDING, EVENT_TARGET_MEAN_DESCENDING, level,
        PAIR_WINDOW_START, PAIR_WINDOW_END, &total, nullptr, 0
      ),
      0U
    );
    EXPECT_EQ(total, 0U);
    EXPECT_NE(std::strstr(last_error(), "throughout"), nullptr);
  }

  // Targets whose certified rate bounds exclude a station return a legal empty list.
  total = 0xDEADBEEFU;
  EXPECT_EQ(longitude_stations(EVENT_TARGET_SUN, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &total, nullptr, 0), 0U);
  EXPECT_EQ(total, 0U);
  EXPECT_STREQ(last_error(), "");
}

TEST(EventCAbi, RejectsMalformedQueriesDeterministically) {
  // Every failure path resets *event_count to 0, leaves the caller's slots untouched, and records why.
  for (const int32_t run : { INT32_C(1), INT32_C(2) }) {
    SCOPED_TRACE(run);

    EXPECT_EQ(longitude_events(EVENT_TARGET_SUN, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END, nullptr, nullptr, 0), 0U);
    EXPECT_NE(std::strstr(last_error(), "event_count"), nullptr);

    uint32_t total = 0xDEADBEEFU;
    EXPECT_EQ(longitude_events(EVENT_TARGET_SUN, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &total, nullptr, 1), 0U);
    EXPECT_EQ(total, 0U);
    EXPECT_NE(std::strstr(last_error(), "slots"), nullptr);

    for (const uint32_t code : { 14U, UINT32_MAX }) {
      total = 0xDEADBEEFU;
      EXPECT_EQ(longitude_events(code, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &total, nullptr, 0), 0U);
      EXPECT_EQ(total, 0U);
      EXPECT_NE(std::strstr(last_error(), "EVENT_TARGET"), nullptr);
    }

    total = 0xDEADBEEFU;
    EXPECT_EQ(
      longitude_difference_events(
        EVENT_TARGET_SUN, EVENT_TARGET_SUN, 1.0, PAIR_WINDOW_START, PAIR_WINDOW_END, &total, nullptr, 0
      ),
      0U
    );
    EXPECT_EQ(total, 0U);
    EXPECT_NE(std::strstr(last_error(), "distinct"), nullptr);

    // The recorded error names the export's own parameter (#303 FFI diagnostics).
    total = 0xDEADBEEFU;
    EXPECT_EQ(
      longitude_difference_events(
        EVENT_TARGET_MOON, EVENT_TARGET_SUN, NAN_VALUE, PAIR_WINDOW_START, PAIR_WINDOW_END, &total, nullptr, 0
      ),
      0U
    );
    EXPECT_EQ(total, 0U);
    EXPECT_NE(std::strstr(last_error(), "target_difference_deg"), nullptr);
    EXPECT_EQ(std::strstr(last_error(), "target_longitude_deg"), nullptr);

    for (const double bad : { NAN_VALUE, INFINITY_VALUE }) {
      total = 0xDEADBEEFU;
      EXPECT_EQ(longitude_events(EVENT_TARGET_SUN, bad, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &total, nullptr, 0), 0U);
      EXPECT_EQ(total, 0U);
      total = 0xDEADBEEFU;
      EXPECT_EQ(longitude_events(EVENT_TARGET_SUN, 0.0, bad, MERCURY_WINDOW_END, &total, nullptr, 0), 0U);
      EXPECT_EQ(total, 0U);
    }

    // Nonempty, ordered, span-limited interval.
    total = 0xDEADBEEFU;
    EXPECT_EQ(longitude_events(EVENT_TARGET_SUN, 0.0, MERCURY_WINDOW_END, MERCURY_WINDOW_START, &total, nullptr, 0), 0U);
    EXPECT_EQ(total, 0U);
    EXPECT_NE(std::strstr(last_error(), "nonempty"), nullptr);
    total = 0xDEADBEEFU;
    EXPECT_EQ(longitude_events(EVENT_TARGET_SUN, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_START, &total, nullptr, 0), 0U);
    EXPECT_EQ(total, 0U);
    total = 0xDEADBEEFU;
    EXPECT_EQ(
      longitude_events(EVENT_TARGET_SUN, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_START + 3660.1, &total, nullptr, 0),
      0U
    );
    EXPECT_EQ(total, 0U);
    EXPECT_NE(std::strstr(last_error(), "3660"), nullptr);
  }

  // A rejected query leaves caller storage untouched, and a valid call recovers and clears the channel.
  std::array<LongitudeEventRecord, 3> slots {};
  for (auto& slot : slots) {
    slot.jde_tt = -777.0;
  }
  uint32_t total = 0;
  EXPECT_EQ(longitude_events(14U, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &total, slots.data(), 3), 0U);
  for (const auto& slot : slots) {
    EXPECT_EQ(slot.jde_tt, -777.0);
  }
  ASSERT_EQ(
    longitude_events(EVENT_TARGET_SUN, 0.0, MERCURY_WINDOW_START, MERCURY_WINDOW_END, &total, slots.data(), 3), 1U
  );
  EXPECT_STREQ(last_error(), "");
}

} // namespace lib::events::test
