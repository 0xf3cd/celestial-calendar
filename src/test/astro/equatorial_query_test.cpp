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
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "ephemeris.hpp"

namespace astro::ephemeris::test {

TEST(EquatorialQuery, ExistingSunAndMoonResultsAreUnchanged) {
  for (const double jde_tt : std::array { 2409544.0, 2415020.5, 2448724.5, 2451545.0, 2460681.293070456, 2488068.0 }) {
    const auto sun = equatorial_position(Target::SUN, jde_tt);
    const auto old_sun = astro::sun::equatorial_coord::apparent(jde_tt);
    EXPECT_EQ(sun.right_ascension.deg(), old_sun.α.deg());
    EXPECT_EQ(sun.declination.deg(), old_sun.δ.deg());

    const auto moon = equatorial_position(Target::MOON, jde_tt);
    const auto old_moon = astro::moon::equatorial_coord::apparent(jde_tt);
    EXPECT_EQ(moon.right_ascension.deg(), old_moon.α.deg());
    EXPECT_EQ(moon.declination.deg(), old_moon.δ.deg());
  }
}

TEST(EquatorialQuery, EveryTargetUsesTheFixedOfDateProjection) {
  for (const double jde_tt : std::array { 2409544.0, 2451545.0, 2460749.7828680002, 2488068.0 }) {
    for (const auto target : TARGETS) {
      const auto ecliptic = position(target, jde_tt);
      const auto ε = earth::obliquity::true_obliquity(jde_tt, earth::nutation::Model::IAU_1980);
      const auto expected = coords::ecliptic_to_equatorial(ecliptic.longitude, ecliptic.latitude, ε);
      const auto result = equatorial_position(target, jde_tt);
      EXPECT_EQ(result.right_ascension.deg(), expected.α.deg());
      EXPECT_EQ(result.declination.deg(), expected.δ.deg());
      EXPECT_EQ(result.distance.has_value(), ecliptic.distance.has_value());
      if (result.distance.has_value() and ecliptic.distance.has_value()) {
        EXPECT_EQ(result.distance->au(), ecliptic.distance->au());
      }
      EXPECT_GE(result.right_ascension.deg(), 0.0);
      EXPECT_LT(result.right_ascension.deg(), 360.0);
      EXPECT_GE(result.declination.deg(), -90.0);
      EXPECT_LE(result.declination.deg(), 90.0);
    }
  }
}

TEST(EquatorialQuery, PositionWindowsDoNotInheritRateStencilReach) {
  for (const auto target : { Target::PLUTO, Target::MEAN_ASCENDING, Target::TRUE_DESCENDING }) {
    const auto& facts = target_facts(target);
    if (not facts.position_domain.has_value()) {
      FAIL() << "Expected a declared provider domain";
    }
    const auto domain = *facts.position_domain;
    const double last = std::nextafter(domain.back(), -std::numeric_limits<double>::infinity());
    EXPECT_NO_THROW((void)equatorial_position(target, domain.front()));
    EXPECT_NO_THROW((void)equatorial_position(target, last));
    EXPECT_THROW((void)longitude_rate(target, domain.front()), std::invalid_argument);
    EXPECT_THROW((void)longitude_rate(target, last), std::invalid_argument);
    EXPECT_THROW((void)equatorial_position(target, std::nextafter(domain.front(), 0.0)), std::invalid_argument);
    EXPECT_THROW((void)equatorial_position(target, domain.back()), std::invalid_argument);
  }

  EXPECT_NO_THROW((void)equatorial_position(Target::SUN, 2300000.0));
  EXPECT_THROW((void)equatorial_position(Target::PLUTO, 2300000.0), std::invalid_argument);
}

TEST(EquatorialQuery, MalformedInputRemainsAnInputError) {
  for (const double jde_tt : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
    EXPECT_THROW((void)equatorial_position(Target::SUN, jde_tt), std::invalid_argument);
  }
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Deliberately forge an unknown target to test rejection.
  EXPECT_THROW((void)equatorial_position(static_cast<Target>(255), 2451545.0), std::invalid_argument);
}

} // namespace astro::ephemeris::test
