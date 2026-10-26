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
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ephemeris.hpp"
#include "ephemeris_events.hpp"
#include "jieqi.hpp"
#include "julian_day.hpp"
#include "moon_phase.hpp"
#include "planet.hpp"
#include "planet_phenomena.hpp"
#include "sun.hpp"
#include "toolbox.hpp"

namespace astro::ephemeris::test {

namespace {

// Library-to-library comparisons, sampled 2026-10-26 without randomness; not external goldens.
constexpr std::array<int32_t, 3> YEARS { 2000, 2024, 2033 };
constexpr double SECONDS_PER_DAY = 86400.0;
// Annual longitude extrema and seven-point rate zeros differ; allow the 84.375 s interval certificate.
constexpr double STATION_TOLERANCE_DAYS = 120.0 / SECONDS_PER_DAY;
// These samples differ by less than 1 ms; 5 ms is a regression limit, not a physical accuracy bound.
constexpr double CROSSING_TOLERANCE_DAYS = 0.005 / SECONDS_PER_DAY;

/** @brief Directed planet-minus-Sun longitude for a position-only direction witness. */
auto planet_sun_difference(const Target target, const double jde_tt) -> double {
  return toolbox::remainder_deg(
    position(target, jde_tt).longitude.deg() - position(Target::SUN, jde_tt).longitude.deg()
  );
}

} // namespace

TEST(EphemerisCrossApi, AnnualStationsMatchLocalRateZeros) {
  using planet::phenomena::Kind;

  std::size_t planet_year_count = 0;
  std::size_t station_count = 0;
  for (const auto& facts : TARGET_FACTS) {
    if (not facts.planet.has_value()) {
      continue;
    }
    const auto planet_id = *facts.planet;

    for (const int32_t year : YEARS) {
      ++planet_year_count;
      for (const auto& annual : planet::phenomena::events(planet_id, year)) {
        if (annual.kind != Kind::STATION_RETROGRADE and annual.kind != Kind::STATION_DIRECT) {
          continue;
        }
        SCOPED_TRACE(::testing::Message {} << year << ' ' << static_cast<uint32_t>(facts.target)
                                           << ' ' << annual.jde_tt);
        const auto local = stations(facts.target, annual.jde_tt - 1.0, annual.jde_tt + 1.0);
        ASSERT_EQ(local.size(), 1);
        EXPECT_NEAR(local.front().jde_tt, annual.jde_tt, STATION_TOLERANCE_DAYS);
        const bool turns_retrograde = annual.kind == Kind::STATION_RETROGRADE;
        EXPECT_EQ(local.front().motion_before, turns_retrograde ? Motion::DIRECT : Motion::RETROGRADE);
        EXPECT_EQ(local.front().motion_after, turns_retrograde ? Motion::RETROGRADE : Motion::DIRECT);
        ++station_count;
      }
    }
  }
  EXPECT_EQ(planet_year_count, 24);
  EXPECT_EQ(station_count, 52);
}

TEST(EphemerisCrossApi, AnnualPlanetCrossingsMatchLocalDifferences) {
  using planet::phenomena::Kind;

  constexpr std::array<std::pair<Kind, double>, 6> levels {{
    { Kind::INFERIOR_CONJUNCTION,   0.0 },
    { Kind::SUPERIOR_CONJUNCTION,   0.0 },
    { Kind::CONJUNCTION,            0.0 },
    { Kind::OPPOSITION,          180.0 },
    { Kind::EASTERN_QUADRATURE,   90.0 },
    { Kind::WESTERN_QUADRATURE,  270.0 },
  }};

  std::size_t crossing_count = 0;
  for (const auto& facts : TARGET_FACTS) {
    if (not facts.planet.has_value()) {
      continue;
    }
    const auto planet_id = *facts.planet;

    for (const int32_t year : YEARS) {
      const auto annual_events = planet::phenomena::events(planet_id, year);
      for (const auto& [kind, longitude] : levels) {
        for (const auto& annual : annual_events | std::views::filter([kind](const auto& event) {
          return event.kind == kind;
        })) {
          SCOPED_TRACE(::testing::Message {} << year << ' ' << static_cast<uint32_t>(facts.target)
                                             << ' ' << static_cast<uint32_t>(kind) << ' ' << annual.jde_tt);
          const auto local = longitude_difference_events(
            facts.target,
            Target::SUN,
            toolbox::AngleDeg { longitude },
            annual.jde_tt - 1.0,
            annual.jde_tt + 1.0
          );
          ASSERT_EQ(local.size(), 1);
          EXPECT_NEAR(local.front().jde_tt, annual.jde_tt, CROSSING_TOLERANCE_DAYS);

          const double before = planet_sun_difference(facts.target, annual.jde_tt - 0.01);
          const double after = planet_sun_difference(facts.target, annual.jde_tt + 0.01);
          const bool increasing = toolbox::remainder_deg(after - before) > 0.0;
          EXPECT_EQ(local.front().direction, increasing ? EventDirection::INCREASING : EventDirection::DECREASING);
          if (kind == Kind::INFERIOR_CONJUNCTION or kind == Kind::SUPERIOR_CONJUNCTION) {
            const auto geometry = planet::observation::geometry(planet_id, local.front().jde_tt);
            EXPECT_EQ(geometry.phase_angle.deg() >= 90.0, kind == Kind::INFERIOR_CONJUNCTION);
          }
          ++crossing_count;
        }
      }
    }
  }
  EXPECT_EQ(crossing_count, 87);
}

TEST(EphemerisCrossApi, Venus2033FullYearStillRefusesUncertifiedResult) {
  // The local conjunction comparison above does not certify a complete annual interval census.
  const auto [start_jde_tt, end_jde_tt] = julian_day::utc_year_jde_window(2033);
  EXPECT_THROW(
    static_cast<void>(longitude_difference_events(
      Target::VENUS,
      Target::SUN,
      toolbox::AngleDeg { 0.0 },
      start_jde_tt,
      end_jde_tt
    )),
    std::runtime_error
  );
}

TEST(EphemerisCrossApi, FourPhasesMatchLocalMoonSunCrossings) {
  using moon_phase::phase_moments::PhaseKind;

  std::size_t phase_count = 0;
  for (const int32_t year : YEARS) {
    for (uint32_t code = 0; code < 4; ++code) {
      for (const double annual_jde_tt : moon_phase::phase_moments::moments(year, static_cast<PhaseKind>(code))) {
        SCOPED_TRACE(::testing::Message {} << year << ' ' << code << ' ' << annual_jde_tt);
        const auto local = longitude_difference_events(
          Target::MOON,
          Target::SUN,
          toolbox::AngleDeg { 90.0 * code },
          annual_jde_tt - 1.0,
          annual_jde_tt + 1.0
        );
        ASSERT_EQ(local.size(), 1);
        EXPECT_NEAR(local.front().jde_tt, annual_jde_tt, CROSSING_TOLERANCE_DAYS);
        EXPECT_EQ(local.front().direction, EventDirection::INCREASING);
        ++phase_count;
      }
    }
  }
  EXPECT_EQ(phase_count, 149);
}

TEST(EphemerisCrossApi, MoonSunAcrossJan1MatchesBothAnnualOwners) {
  using moon_phase::phase_moments::PhaseKind;

  std::size_t boundary_count = 0;
  for (const int32_t year : YEARS) {
    const double jan1_jde_tt = julian_day::utc_year_jde_window(year).front();
    const double start_jde_tt = jan1_jde_tt - 20.0;
    const double end_jde_tt = jan1_jde_tt + 20.0;
    for (uint32_t code = 0; code < 4; ++code) {
      SCOPED_TRACE(::testing::Message {} << year << ' ' << code);
      std::vector<double> annual;
      for (const int32_t owner : { year - 1, year }) {
        for (const double root : moon_phase::phase_moments::moments(owner, static_cast<PhaseKind>(code))) {
          if (root >= start_jde_tt and root < end_jde_tt) {
            EXPECT_EQ(root >= jan1_jde_tt, owner == year);
            annual.push_back(root);
          }
        }
      }

      const auto interval = longitude_difference_events(
        Target::MOON,
        Target::SUN,
        toolbox::AngleDeg { 90.0 * code },
        start_jde_tt,
        end_jde_tt
      );
      ASSERT_EQ(interval.size(), annual.size());
      for (std::size_t index = 0; index < annual.size(); ++index) {
        EXPECT_NEAR(interval.at(index).jde_tt, annual.at(index), CROSSING_TOLERANCE_DAYS);
        EXPECT_EQ(interval.at(index).direction, EventDirection::INCREASING);
        EXPECT_EQ(interval.at(index).jde_tt >= jan1_jde_tt, annual.at(index) >= jan1_jde_tt);
        ++boundary_count;
      }
    }
  }
  EXPECT_EQ(boundary_count, 16);
}

TEST(EphemerisCrossApi, JieqiMatchLocalSolarLongitudeCrossings) {
  namespace jq = calendar::jieqi;

  std::size_t jieqi_count = 0;
  for (const int32_t year : YEARS) {
    for (const auto jieqi : jq::JIEQI_LIST) {
      SCOPED_TRACE(::testing::Message {} << year << ' ' << static_cast<uint32_t>(jq::to_index(jieqi)));
      const double annual_jde_tt = jq::calc_jieqi_jde(year, jieqi);
      const auto local = longitude_events(
        Target::SUN,
        toolbox::AngleDeg { jq::longitude_of(jieqi) },
        annual_jde_tt - 1.0,
        annual_jde_tt + 1.0
      );
      ASSERT_EQ(local.size(), 1);
      EXPECT_NEAR(local.front().jde_tt, annual_jde_tt, CROSSING_TOLERANCE_DAYS);
      EXPECT_EQ(local.front().direction, EventDirection::INCREASING);
      ++jieqi_count;
    }
  }
  EXPECT_EQ(jieqi_count, 72);
}

TEST(EphemerisCrossApi, SolarLevelsNearWrapMatchAnnualRoots) {
  std::size_t crossing_count = 0;
  for (const int32_t year : YEARS) {
    for (const double longitude : { 1.0, 45.123, 359.999 }) {
      SCOPED_TRACE(::testing::Message {} << year << ' ' << longitude);
      const auto annual = sun::geocentric_coord::math::find_roots(year, longitude);
      ASSERT_EQ(annual.size(), 1);
      const auto local = longitude_events(
        Target::SUN,
        toolbox::AngleDeg { longitude },
        annual.front() - 1.0,
        annual.front() + 1.0
      );
      ASSERT_EQ(local.size(), 1);
      EXPECT_NEAR(local.front().jde_tt, annual.front(), CROSSING_TOLERANCE_DAYS);
      EXPECT_EQ(local.front().direction, EventDirection::INCREASING);
      ++crossing_count;
    }
  }
  EXPECT_EQ(crossing_count, 9);
}

} // namespace astro::ephemeris::test
