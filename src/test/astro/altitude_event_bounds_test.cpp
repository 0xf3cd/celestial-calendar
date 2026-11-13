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
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "altitude_event_certificate.hpp"

namespace astro::detail::altitude_bounds::test {
namespace {

/** @brief Constant unit direction and AU distance with independently specified daily rotation. */
auto stationary_source() -> observer_c2::Source {
  return {
    .direction = { 1.0, 0.0, 0.0 }, .distance_au = { 1.0, 0.0, 0.0 }, .distance_lower_au = 1.0,
    .rotation_first_rad = 2.0, .rotation_second_rad = 3.0, .earth_radius_au = 0.0,
  };
}

} // namespace

TEST(AltitudeEventBounds, SignedByteFieldsDecodeAsExactPowersOfTwo) {
  EXPECT_DOUBLE_EQ(decode(-128), 0.0);
  for (int32_t exponent = -127; exponent <= 127; ++exponent) {
    EXPECT_DOUBLE_EQ(decode(static_cast<int8_t>(exponent)), std::ldexp(1.0, exponent));
  }
}

TEST(AltitudeEventBounds, DirectedArithmeticPreservesZeroAndRoundsPositiveResultsOutward) {
  namespace c2 = observer_c2;
  const double infinity = std::numeric_limits<double>::infinity();
  EXPECT_DOUBLE_EQ(c2::add(0.0, 0.0), 0.0);
  EXPECT_DOUBLE_EQ(c2::multiply(0.0, 1.0), 0.0);
  EXPECT_DOUBLE_EQ(c2::divide(0.0, 1.0), 0.0);
  EXPECT_DOUBLE_EQ(c2::add(1.0, 1.0), std::nextafter(2.0, infinity));
  EXPECT_DOUBLE_EQ(c2::multiply(2.0, 2.0), std::nextafter(4.0, infinity));
  EXPECT_DOUBLE_EQ(c2::divide(1.0, 2.0), std::nextafter(0.5, infinity));
  EXPECT_GT(c2::add(1.0, 1.0), 2.0);
  EXPECT_GT(c2::multiply(2.0, 2.0), 4.0);
  EXPECT_GT(c2::divide(1.0, 2.0), 0.5);
  const double tiny = std::numeric_limits<double>::denorm_min();
  EXPECT_DOUBLE_EQ(c2::multiply(tiny, tiny), tiny);
  EXPECT_GT(c2::multiply(tiny, tiny), 0.0);
}

TEST(AltitudeEventBounds, ParallaxCurvatureRetainsTheSquaredDistanceRate) {
  auto source = stationary_source();
  source.distance_au = { 2.0, 1.5, 2.0 };
  source.earth_radius_au = 0.5;
  // p'' <= radius * (R2/Rmin² + 2*R1²/Rmin³); this exact dyadic fixture gives 3.25.
  const auto bounds = observer_c2::parallax(source);
  EXPECT_GT(bounds.at(0), 0.5);
  EXPECT_GT(bounds.at(1), 0.75);
  EXPECT_GT(bounds.at(2), 3.25);
}

TEST(AltitudeEventBounds, AffinePacketsRetainBothTheConstantAndHeightTerm) {
  EXPECT_DOUBLE_EQ(affine(-128, -128, 1.0), 0.0);
  EXPECT_GT(affine(0, -1, 4.0), 3.0);
  EXPECT_GT(affine(0, -1, 4.0), affine(0, -1, 0.0));
}

TEST(AltitudeEventBounds, StationaryPolarDirectionHasNoDailyAltitudeDerivatives) {
  const observer_c2::Site site {
    .equatorial_upper = 0.0, .polar_upper = 1.0, .cos_latitude_upper = 0.0,
    .sin_latitude_upper = 1.0, .q_lower = 1.0,
  };
  const auto bounds = observer_c2::compose(stationary_source(), site, 1.0);
  EXPECT_DOUBLE_EQ(bounds.first, 0.0);
  EXPECT_DOUBLE_EQ(bounds.second, 0.0);
  EXPECT_GT(bounds.local_vector.at(1), 0.0);
  EXPECT_GT(bounds.local_vector.at(2), 0.0);
}

TEST(AltitudeEventBounds, InvalidSourceAndLostObserverSeparationFailNumerically) {
  const observer_c2::Site site {
    .equatorial_upper = 1.0, .polar_upper = 0.0, .cos_latitude_upper = 1.0,
    .sin_latitude_upper = 0.0, .q_lower = 1.0,
  };
  auto invalid = stationary_source();
  invalid.direction.at(0) = 2.0;
  EXPECT_THROW((void)observer_c2::compose(invalid, site, 0.0), std::runtime_error);
  invalid = stationary_source();
  invalid.distance_lower_au = 0.0;
  EXPECT_THROW((void)observer_c2::compose(invalid, site, 0.0), std::runtime_error);
  invalid = stationary_source();
  invalid.rotation_first_rad = std::numeric_limits<double>::quiet_NaN();
  EXPECT_THROW((void)observer_c2::compose(invalid, site, 0.0), std::runtime_error);
  auto unseparated = site;
  unseparated.q_lower = 0.0;
  EXPECT_THROW((void)observer_c2::compose(stationary_source(), unseparated, 0.0), std::runtime_error);
  unseparated.q_lower = 2.0;
  EXPECT_THROW((void)observer_c2::compose(stationary_source(), unseparated, 0.0), std::runtime_error);
}

TEST(AltitudeEventBounds, NonfiniteComposedMajorantsFailNumerically) {
  auto source = stationary_source();
  source.direction.at(1) = std::numeric_limits<double>::max();
  const observer_c2::Site site {
    .equatorial_upper = 1.0, .polar_upper = 0.0, .cos_latitude_upper = 1.0,
    .sin_latitude_upper = 0.0, .q_lower = 1.0,
  };
  EXPECT_THROW((void)observer_c2::compose(source, site, 1.0), std::runtime_error);
}

TEST(AltitudeEventBounds, Wgs84BoundsKeepExactPolesAndEquator) {
  const auto bound_source = source(RECORDS.at(0), ROTATION_FIRST, ROTATION_SECOND, EARTH_RADIUS_AU);
  for (const double latitude : std::array { -90.0, 90.0 }) {
    const auto site = site_bounds(bound_source, latitude, 0.0);
    EXPECT_DOUBLE_EQ(site.equatorial_upper, 0.0);
    EXPECT_DOUBLE_EQ(site.cos_latitude_upper, 0.0);
    EXPECT_DOUBLE_EQ(site.sin_latitude_upper, 1.0);
    EXPECT_GT(site.q_lower, 0.0);
  }
  const auto equator = site_bounds(bound_source, 0.0, 0.0);
  EXPECT_DOUBLE_EQ(equator.polar_upper, 0.0);
  EXPECT_DOUBLE_EQ(equator.sin_latitude_upper, 0.0);
  EXPECT_THROW((void)site_bounds(bound_source, 0.0, 1e20), std::runtime_error);
}

TEST(AltitudeEventBounds, OrdinaryCellsSelectAllTenPhysicalTargets) {
  const auto pieces = clock_pieces(2451545.0, 2451545.25, 8);
  ASSERT_EQ(pieces.size(), 1U);
  EXPECT_FALSE(pieces.front().collar);
  for (uint32_t target = 0; target < 10; ++target) {
    const auto input = observer_for_piece(target, 30.0, -1.0, pieces.front());
    EXPECT_GT(input.site.q_lower, 0.0);
    EXPECT_GT(input.value_error, 0.0);
    EXPECT_GT(input.q_upper, 0.0);
    const auto bounds = observer_c2::compose(input.source, input.site, 1.0);
    EXPECT_GT(bounds.first, 0.0);
    EXPECT_GT(bounds.second, 0.0);
  }
  EXPECT_THROW((void)observer_for_piece(10, 0.0, 0.0, pieces.front()), std::invalid_argument);
}

TEST(AltitudeEventBounds, ClockJoinsKeepCompleteCoverageAndModelJumpAllowance) {
  const double join = year_start(1900);
  const auto pieces = clock_pieces(join - 0.25, join + 0.25, 8);
  ASSERT_EQ(pieces.size(), 3U);
  EXPECT_DOUBLE_EQ(pieces.front().start_jde_tt, join - 0.25);
  EXPECT_DOUBLE_EQ(pieces.back().end_jde_tt, join + 0.25);
  EXPECT_DOUBLE_EQ(pieces.at(0).end_jde_tt, pieces.at(1).start_jde_tt);
  EXPECT_DOUBLE_EQ(pieces.at(1).end_jde_tt, pieces.at(2).start_jde_tt);
  EXPECT_FALSE(pieces.at(0).collar);
  EXPECT_TRUE(pieces.at(1).collar);
  EXPECT_FALSE(pieces.at(2).collar);
  EXPECT_GT(pieces.at(1).jump, 0.0);
  const auto ordinary = observer_for_piece(0, 0.0, 0.0, pieces.at(0));
  const auto collar = observer_for_piece(0, 0.0, 0.0, pieces.at(1));
  EXPECT_GT(collar.value_error, ordinary.value_error);
  EXPECT_DOUBLE_EQ(collar.source.direction.at(1), ordinary.source.direction.at(1));
  EXPECT_THROW((void)clock_pieces(join - 0.25, join + 0.25, 2), std::runtime_error);
}

TEST(AltitudeEventBounds, GregorianOnlyCollarsHaveNoInventedModelJump) {
  const double join = year_start(2001);
  const auto pieces = clock_pieces(join - 0.25, join + 0.25, 8);
  ASSERT_EQ(pieces.size(), 3U);
  EXPECT_TRUE(pieces.at(1).collar);
  EXPECT_DOUBLE_EQ(pieces.at(1).jump, 0.0);
  EXPECT_THROW((void)clock_pieces(2451545.0, 2451545.25, 0), std::runtime_error);
}

TEST(AltitudeEventBounds, CompleteLocalClockDoesNotImplyAllTargetsHaveSourceCertificates) {
  const auto& clock = LOCAL_CLOCKS.at(1);
  const auto pieces = clock_pieces(clock.start, clock.end, 1);
  ASSERT_EQ(pieces.size(), 1U);
  EXPECT_DOUBLE_EQ(pieces.front().start_jde_tt, clock.start);
  EXPECT_DOUBLE_EQ(pieces.front().end_jde_tt, clock.end);
  const auto mars = observer_for_piece(4, 0.0, 0.0, pieces.front());
  EXPECT_GT(mars.value_error, 0.0);
  EXPECT_DOUBLE_EQ(mars.source.distance_au.at(0), decode(LOCAL_SOURCES.at(3).packet.at(2)));
  EXPECT_GT(observer_for_piece(5, 0.0, 0.0, pieces.front()).value_error, 0.0);
  EXPECT_THROW((void)observer_for_piece(6, 0.0, 0.0, pieces.front()), std::runtime_error);
  auto collar = pieces.front();
  collar.collar = true;
  EXPECT_THROW((void)observer_for_piece(4, 0.0, 0.0, collar), std::runtime_error);
}

TEST(AltitudeEventBounds, LocalProofsCannotExtendPastTheirClosedEnvelope) {
  const auto& clock = LOCAL_CLOCKS.at(0);
  EXPECT_EQ(local_clock_index(clock.start, clock.end), 0U);
  EXPECT_EQ(local_clock_index(clock.end, clock.start), LOCAL_CLOCKS.size());
  const double after = std::nextafter(clock.end, std::numeric_limits<double>::infinity());
  EXPECT_EQ(local_clock_index(clock.start, after), LOCAL_CLOCKS.size());
  EXPECT_THROW((void)clock_pieces(clock.start, after, 8), std::runtime_error);
  EXPECT_THROW((void)clock_pieces(clock.start, clock.end, 0), std::runtime_error);
}

TEST(AltitudeEventBounds, MissingPlutoEndpointProofIsNumericalNotInvalidAdmission) {
  const ClockPiece piece {
    .start_jde_tt = PLUTO_END, .end_jde_tt = std::nextafter(PLUTO_END, std::numeric_limits<double>::infinity()),
    .collar = false, .jump = 0.0,
  };
  EXPECT_THROW((void)observer_for_piece(9, 0.0, 0.0, piece), std::runtime_error);
}

} // namespace astro::detail::altitude_bounds::test
