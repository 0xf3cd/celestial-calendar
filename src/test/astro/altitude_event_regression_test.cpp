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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>
#include <gtest/gtest.h>
#include "altitude_events.hpp"
#include "planet_rise_set_reference_data.hpp"
#include "ymd.hpp"
#include "../random.hpp"

namespace astro::observer::test {
namespace {

/** @brief Fixed WGS84 sea-level site, with east-positive longitude in degrees. */
auto make_site(const double latitude = 30.0, const double longitude = -77.0) -> Observer {
  return { .location = { .latitude = toolbox::AngleDeg { latitude }, .longitude = toolbox::AngleDeg { longitude } },
           .elevation = toolbox::DistanceKm { 0.0 } };
}

/** @brief A geometric centre-altitude condition without weather or limb corrections. */
auto geometric_condition(const ephemeris::Target target, const double level) -> AltitudeCondition {
  return { .target = target, .level = GeometricLevel { .altitude = toolbox::AngleDeg { level } }, .sense = AltitudeSense::ABOVE };
}

/** @brief Observe the same target with the interval query's fixed ALGO5 clock. */
auto instantaneous_altitude(const Observer& site, const ephemeris::Target target, const double jde_tt) -> double {
  const auto ut1 = delta_t::tt_to_ut1(delta_t::Model::ALGO5, julian_day::jde_to_tt(jde_tt));
  return observe({ .jd_ut1 = julian_day::ut1_to_jd(ut1), .jde_tt = jde_tt }, site, target).altitude.deg();
}
} // namespace

TEST(AltitudeEventRegression, BothExactPolesUseTheSuppliedLongitudeForUpperPassage) {
  for (const double latitude : { -90.0, 90.0 }) {
    const auto west = upper_transits(make_site(latitude, -77.0), ephemeris::Target::MARS, 2451545.0, 2451546.0);
    const auto east = upper_transits(make_site(latitude, 13.0), ephemeris::Target::MARS, 2451545.0, 2451546.0);
    ASSERT_EQ(west.size(), 1U);
    ASSERT_EQ(east.size(), 1U);
    EXPECT_GT(west.front().jde_tt - east.front().jde_tt, 0.24);
    EXPECT_LT(west.front().jde_tt - east.front().jde_tt, 0.26);
    for (const auto& event : west) {
      const auto ut1 = delta_t::tt_to_ut1(delta_t::Model::ALGO5, julian_day::jde_to_tt(event.jde_tt));
      const auto theta = sidereal::local_apparent(julian_day::ut1_to_jd(ut1), event.jde_tt, toolbox::AngleDeg { 77.0 });
      const auto position = ephemeris::equatorial_position(ephemeris::Target::MARS, event.jde_tt);
      const auto local = detail::local_geometry(position, theta, make_site(latitude));
      EXPECT_GT(local.B, 0.0);
      // A daily angular rate near 2*pi transports 2^-20 day into less than 2^-16.
      EXPECT_NEAR(local.A, 0.0, 0x1p-16);
    }
  }
}

TEST(AltitudeEventRegression, SouthernHemisphereCrossingsMatchTheirTopocentricSides) {
  const auto site = make_site(-35.0, 151.0);
  const auto result = altitude_events(site, geometric_condition(ephemeris::Target::SUN, 0.0), 2451545.0, 2451546.0);
  ASSERT_EQ(result.events.size(), 2U);
  for (const auto& event : result.events) {
    const double before = instantaneous_altitude(site, ephemeris::Target::SUN, event.jde_tt - 0x1p-18);
    const double after = instantaneous_altitude(site, ephemeris::Target::SUN, event.jde_tt + 0x1p-18);
    EXPECT_LT(before * after, 0.0);
    EXPECT_EQ(event.direction, before < after ? ephemeris::EventDirection::INCREASING : ephemeris::EventDirection::DECREASING);
  }
}

TEST(AltitudeEventRegression, ClosePairsNoneAndDetectedContactRemainDistinct) {
  // Pluto over [2451545,2451545.25], site (30, -77): stored-series replay at 80/112 decimal digits,
  // 2026-11-11, no random seed. The rounded probe level is 48.605252016244656 degrees;
  // offsets -0.04/-0.01 give pairs, +0.01 gives none, and the rounded maximum is a near contact.
  const auto pair = altitude_events(make_site(), geometric_condition(ephemeris::Target::PLUTO, 48.565252016244656), 2451545.0, 2451545.25);
  ASSERT_EQ(pair.events.size(), 2U);
  EXPECT_EQ(pair.events.at(0).direction, ephemeris::EventDirection::INCREASING);
  EXPECT_EQ(pair.events.at(1).direction, ephemeris::EventDirection::DECREASING);
  EXPECT_LT(pair.events.at(0).jde_tt, pair.events.at(1).jde_tt);
  const auto none = altitude_events(make_site(), geometric_condition(ephemeris::Target::PLUTO, 48.615252016244656), 2451545.0, 2451545.25);
  EXPECT_TRUE(none.events.empty());
  EXPECT_FALSE(none.condition.initially_satisfied);
  const auto closer = altitude_events(make_site(), geometric_condition(ephemeris::Target::PLUTO, 48.595252016244656), 2451545.0, 2451545.25);
  ASSERT_EQ(closer.events.size(), 2U);
  EXPECT_LT(closer.events.back().jde_tt - closer.events.front().jde_tt, detail::altitude_events::SAMPLE_STEP_TT_DAYS);
  EXPECT_THROW(
    (void)altitude_events(make_site(), geometric_condition(ephemeris::Target::PLUTO, 48.605252016244656), 2451545.0, 2451545.25),
    std::runtime_error
  );
}

TEST(AltitudeEventRegression, FalseConditionDoesNotHideAnUnresolvedCompanion) {
  const auto false_condition = geometric_condition(ephemeris::Target::SUN, 89.0);
  const auto proved = altitude_events(make_site(), false_condition, 2451545.0, 2451545.25);
  EXPECT_FALSE(proved.condition.initially_satisfied);
  EXPECT_TRUE(proved.events.empty());
  const std::array conditions { false_condition, geometric_condition(ephemeris::Target::PLUTO, 48.605252016244656) };
  EXPECT_THROW((void)observing_windows(make_site(), conditions, 2451545.0, 2451545.25), std::runtime_error);
}

TEST(AltitudeEventRegression, MultiDayCensusEqualsTheConcatenatedDailyCensuses) {
  const auto site = make_site(-35.0, 151.0);
  const auto condition = geometric_condition(ephemeris::Target::SUN, 0.0);
  const auto complete = altitude_events(site, condition, 2451545.0, 2451548.0);
  ASSERT_EQ(complete.events.size(), 6U);
  std::size_t index = 0;
  for (uint32_t day = 0; day < 3; ++day) {
    const double start = 2451545.0 + day;
    const auto part = altitude_events(site, condition, start, start + 1.0);
    ASSERT_EQ(part.events.size(), 2U);
    for (const auto& event : part.events) {
      ASSERT_LT(index, complete.events.size());
      EXPECT_DOUBLE_EQ(event.jde_tt, complete.events.at(index).jde_tt);
      EXPECT_EQ(event.direction, complete.events.at(index).direction);
      EXPECT_GE(event.jde_tt, start);
      EXPECT_LT(event.jde_tt, start + 1.0);
      ++index;
    }
  }
  EXPECT_EQ(index, complete.events.size());
}

TEST(AltitudeEventRegression, LunarThresholdIsTopocentricAndHasNoGeocentricHorizonOffset) {
  const auto site = make_site(-35.0, 151.0);
  const auto result = altitude_events(site, geometric_condition(ephemeris::Target::MOON, 0.0), 2451545.0, 2451546.0);
  ASSERT_EQ(result.events.size(), 2U);
  for (const auto& event : result.events) {
    // 0.001 degrees covers 0.0824 s of ordinary lunar motion, below a geocentric horizon offset.
    EXPECT_NEAR(instantaneous_altitude(site, ephemeris::Target::MOON, event.jde_tt), 0.0, 0.001);
  }
}

TEST(AltitudeEventRegression, IntervalCensusMatchesTheRetainedHorizonsEventBrackets) {
  // JPL Horizons, collected 2026-11-09; source inputs and frozen per-target allowances
  // are in planet_rise_set_reference_data.hpp. The same -34 arcmin centre threshold
  // applies here; each UT1 midnight is converted separately with ALGO5.
  namespace reference = rise_set::planet::test::reference;
  for (std::size_t index = 0; index < reference::DAYS.size(); ++index) {
    const auto& input = reference::DAYS.at(index);
    const Observer site {
      .location = { .latitude = toolbox::AngleDeg { input.latitude_deg }, .longitude = toolbox::AngleDeg { input.longitude_deg } },
      .elevation = toolbox::DistanceKm { input.height_km },
    };
    const auto target = static_cast<ephemeris::Target>(input.target);
    const calendar::Datetime midnight { util::to_ymd(input.year, input.month, input.day), 0.0 };
    const double jd_ut1 = julian_day::ut1_to_jd(midnight);
    const double start = julian_day::tt_to_jde(delta_t::ut1_to_tt(delta_t::Model::ALGO5, midnight));
    const double end = julian_day::tt_to_jde(delta_t::ut1_to_tt(delta_t::Model::ALGO5, julian_day::jd_to_ut1(jd_ut1 + 1.0)));
    const auto query = altitude_events(site, geometric_condition(target, -34.0 / 60.0), start, end);
    const auto transits = upper_transits(site, target, start, end);
    std::size_t expected_crossings = 0;
    std::size_t expected_transits = 0;
    for (const auto& expected : reference::EVENTS) {
      if (expected.day_index != index) { continue; }
      if (expected.event_index == 1) {
        ++expected_transits;
        ASSERT_EQ(transits.size(), 1U);
        EXPECT_NEAR(transits.front().jde_tt, expected.source_lower_jde_tt, expected.allowance_seconds / 86400.0);
        EXPECT_NEAR(transits.front().jde_tt, expected.source_upper_jde_tt, expected.allowance_seconds / 86400.0);
      } else {
        ++expected_crossings;
        const auto direction = expected.event_index == 0 ? ephemeris::EventDirection::INCREASING : ephemeris::EventDirection::DECREASING;
        const auto found = std::ranges::find(query.events, direction, &AltitudeEvent::direction);
        ASSERT_NE(found, query.events.end());
        EXPECT_NEAR(found->jde_tt, expected.source_lower_jde_tt, expected.allowance_seconds / 86400.0);
        EXPECT_NEAR(found->jde_tt, expected.source_upper_jde_tt, expected.allowance_seconds / 86400.0);
      }
    }
    EXPECT_EQ(query.events.size(), expected_crossings);
    EXPECT_EQ(transits.size(), expected_transits);
  }
}

TEST(AltitudeEventRegression, SplitsAtReturnedClosePairEventsPreserveTimesAndInitialSides) {
  const auto site = make_site();
  const auto condition = geometric_condition(ephemeris::Target::PLUTO, 48.595252016244656);
  const auto whole = altitude_events(site, condition, 2451545.0, 2451545.25);
  ASSERT_EQ(whole.events.size(), 2U);
  for (std::size_t index = 0; index < whole.events.size(); ++index) {
    const auto event = whole.events.at(index);
    const auto part = altitude_events(site, condition, event.jde_tt, 2451545.25);
    ASSERT_EQ(part.events.size(), whole.events.size() - index);
    EXPECT_DOUBLE_EQ(part.events.front().jde_tt, event.jde_tt);
    EXPECT_EQ(part.condition.initially_satisfied, event.direction == ephemeris::EventDirection::INCREASING);
    EXPECT_EQ(instantaneous_altitude(site, ephemeris::Target::PLUTO, event.jde_tt) > 48.595252016244656,
              part.condition.initially_satisfied);
  }
  const std::array conditions { condition };
  const auto windows = observing_windows(site, conditions, 2451545.0, 2451545.25);
  ASSERT_EQ(windows.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(windows.windows.front().start_jde_tt, whole.events.front().jde_tt);
  EXPECT_DOUBLE_EQ(windows.windows.front().end_jde_tt, whole.events.back().jde_tt);
}

TEST(AltitudeEventRegression, SeededQueriesMatchFinerGridCrossingsAndWindowTruth) {
  // Same ephemerides, independent one-minute sign scan through observe(); this checks the search,
  // not physical accuracy or completeness. Seed 42 by default. Refine reference roots to 2^-23 day,
  // then allow one production bracket width plus that reference width in the time comparison.
  constexpr double REFERENCE_RESOLUTION = 0x1p-23;
  for (uint32_t trial = 0; trial < 30; ++trial) {
    const auto target = static_cast<ephemeris::Target>(trial % 10);
    const auto site = make_site(util::random(-75.0, 75.0), util::random(-180.0, 180.0));
    const double start = util::random(2451545.0, 2460000.0);
    const double end = start + 1.0;
    const double level = util::random(-20.0, 40.0);
    const auto condition = geometric_condition(target, level);
    const auto result = altitude_events(site, condition, start, end);
    const std::array conditions { condition };
    const auto windows = observing_windows(site, conditions, start, end);
    const auto above = [&](const double at) { return instantaneous_altitude(site, target, at) > level; };
    SCOPED_TRACE(testing::Message() << "trial=" << trial << " target=" << trial % 10 << " start=" << start);

    std::vector<AltitudeEvent> reference;
    double previous = start;
    bool previous_above = above(start);
    EXPECT_EQ(result.condition.initially_satisfied, previous_above);
    for (uint32_t minute = 1; minute <= 1440; ++minute) {
      const double at = start + (static_cast<double>(minute) / 1440.0);
      const bool current_above = above(at);
      if (previous_above != current_above) {
        double left = previous;
        double right = at;
        while (right - left > REFERENCE_RESOLUTION) {
          const double middle = std::midpoint(left, right);
          if (above(middle) == previous_above) {
            left = middle;
          } else {
            right = middle;
          }
        }
        reference.push_back({
          .jde_tt = right, .condition_index = 0,
          .direction = current_above ? ephemeris::EventDirection::INCREASING : ephemeris::EventDirection::DECREASING,
        });
      }
      const bool near_event = std::ranges::any_of(result.events, [&](const AltitudeEvent& event) {
        return std::abs(event.jde_tt - at) <= detail::altitude_events::TIME_RESOLUTION_TT_DAYS;
      });
      if (at < end and not near_event) {
        const bool in_window = std::ranges::any_of(windows.windows, [&](const ObservingWindow& window) {
          return window.start_jde_tt <= at and at < window.end_jde_tt;
        });
        EXPECT_EQ(in_window, current_above);
      }
      previous = at;
      previous_above = current_above;
    }
    ASSERT_EQ(result.events.size(), reference.size());
    for (std::size_t index = 0; index < reference.size(); ++index) {
      EXPECT_EQ(result.events.at(index).direction, reference.at(index).direction);
      EXPECT_NEAR(result.events.at(index).jde_tt, reference.at(index).jde_tt,
                  detail::altitude_events::TIME_RESOLUTION_TT_DAYS + REFERENCE_RESOLUTION);
    }
  }
}
} // namespace astro::observer::test
