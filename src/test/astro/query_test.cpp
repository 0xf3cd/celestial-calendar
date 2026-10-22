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
#include <cstdint>

#include <gtest/gtest.h>

#include "chart.hpp"
#include "ephemeris.hpp"
#include "../test_helper.hpp"

namespace astro::chart::test {

using ephemeris::Target;

// #355: the independent position operation is the same dispatch the snapshot composes, and the
// house time conversion deliberately skips the snapshot joint window.

TEST(Query, PositionMatchesSnapshotBodiesOnTheJointWindow) {
  // For civil instants inside the snapshot common window, the independent position operation
  // must reproduce the snapshot body fields exactly (same dispatch, no rate involved).
  for (const auto& [year, month, day, fraction] : { std::tuple { 2024, 6, 1, 0.5 },
                                                    std::tuple { 1999, 12, 31, 0.999 },
                                                    std::tuple { 2050, 3, 15, 0.25 } }) {
    const auto civil = calendar::Datetime { util::to_ymd(year, month, day), fraction };
    const auto snapshot = astro::chart::calculate(
      civil,
      Scale::UTC,
      astro::GeoLocation { .latitude = astro::toolbox::AngleDeg { 51.5 }, .longitude = astro::toolbox::AngleDeg { 0.0 } },
      astro::house::System::WHOLE_SIGN
    );
    ASSERT_TRUE(snapshot.times.jde_tt >= detail::START_JDE_TT);

    for (const auto target : { Target::SUN, Target::MOON, Target::MARS, Target::PLUTO, Target::TRUE_ASCENDING }) {
      const auto coordinate = astro::chart::position(target, snapshot.times.jde_tt);
      const auto& body = snapshot.bodies.at(static_cast<uint8_t>(target));
      EXPECT_DOUBLE_EQ(coordinate.longitude.deg(), body.longitude.deg()) << "target " << static_cast<int>(target);
      EXPECT_DOUBLE_EQ(coordinate.latitude.deg(), body.latitude.deg()) << "target " << static_cast<int>(target);
      ASSERT_EQ(coordinate.distance.has_value(), body.distance.has_value());
      if (coordinate.distance.has_value()) {
        EXPECT_DOUBLE_EQ(util::test::req(coordinate.distance).au(), util::test::req(body.distance).au());
      }
    }
  }
}

TEST(Query, PositionAtProviderBoundaryNeedsNoRateStencil) {
  // Pluto's apparent window starts at 2409543.5 (1885-01-02 0h TT), below the rate center
  // domain and the snapshot joint window, which both start at 2409543.875 (1885-01-02 09:00
  // TT). A pure position call must succeed here — the "boundary position without a rate
  // template" case (#355).
  const auto pluto = astro::chart::position(Target::PLUTO, 2409543.5);
  EXPECT_GE(pluto.longitude.deg(), 0.0);
  // Nodes likewise: the node provider accepts 2409542.5, below any rate center.
  const auto node = astro::chart::position(Target::MEAN_ASCENDING, 2409542.5);
  EXPECT_FALSE(node.distance.has_value());
  // At the same instant (1885-01-02 0h, its JDE(TT) just below 2409543.875) a complete
  // snapshot rejects the query through the joint window.
  EXPECT_THROW(
    (void)astro::chart::calculate(
      calendar::Datetime { util::to_ymd(1885, 1, 2), 0.0 },
      Scale::UT1,
      astro::GeoLocation { .latitude = astro::toolbox::AngleDeg { 51.5 }, .longitude = astro::toolbox::AngleDeg { 0.0 } },
      astro::house::System::WHOLE_SIGN
    ),
    std::invalid_argument
  );
}

TEST(Query, HouseConversionCarriesNoJointWindow) {
  // The snapshot joint window starts at 2409543.875 (1885-01-02 09:00 TT); this civil instant
  // resolves below it while still passing the civil validation.
  const auto civil = calendar::Datetime { util::to_ymd(1885, 1, 1), 0.25 };

  // The house conversion resolves any civil instant the civil validation admits.
  const auto times = detail::resolve_times_for_houses(civil, Scale::UT1, astro::delta_t::Model::ALGO5);
  EXPECT_LT(times.jde_tt, detail::START_JDE_TT);  // before the snapshot joint window
  const auto houses = astro::house::calculate(
    times.jd_ut1, times.jde_tt,
    astro::GeoLocation { .latitude = astro::toolbox::AngleDeg { 51.5 }, .longitude = astro::toolbox::AngleDeg { 0.0 } },
    astro::house::System::PLACIDUS
  );
  EXPECT_GE(houses.ascendant.deg(), 0.0);

  // The snapshot path rejects the same instant through validate_domain.
  EXPECT_THROW(
    (void)detail::resolve_times(civil, Scale::UT1, astro::delta_t::Model::ALGO5),
    std::invalid_argument
  );
}

TEST(Query, HouseConversionMatchesSnapshotInsideTheWindow) {
  const auto civil = calendar::Datetime { util::to_ymd(2024, 6, 1), 0.5 };
  const auto both = detail::resolve_times_for_houses(civil, Scale::UTC, astro::delta_t::Model::ALGO5);
  const auto snapshot_times = detail::resolve_times(civil, Scale::UTC, astro::delta_t::Model::ALGO5);
  EXPECT_DOUBLE_EQ(both.jd_ut1, snapshot_times.jd_ut1);
  EXPECT_DOUBLE_EQ(both.jde_tt, snapshot_times.jde_tt);
}

} // namespace astro::chart::test
