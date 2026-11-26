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
#include <format>
#include <limits>
#include <gtest/gtest.h>

#include "apparent_diameter.hpp"
#include "diameter_reference_data.hpp"
#include "../test_helper.hpp"

namespace astro::apparent_diameter::test {

using toolbox::AngleDeg;
using toolbox::DistanceAu;
using toolbox::DistanceKm;
using ephemeris::Target;

// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): Deliberately exercise admission of an unnamed fixed-underlying enum value.
inline constexpr auto UNKNOWN_TARGET = static_cast<Target>(255);

TEST(ApparentDiameter, IndependentRadiusFactsAndConventions) {
  using enum RadiusDefinition;
  // Independent IAU/JPL source columns, collected 2026-11-08; see provenance/diameter/radii.json.
  // Retained numeric facts remain under their source terms, outside the project MIT grant.
  constexpr std::array radii { 695700.0, 1737.4, 2440.53, 6051.8, 3396.19, 71492.0, 60268.0, 25559.0, 24764.0, 1188.3 };
  constexpr std::array targets { Target::SUN, Target::MOON, Target::MERCURY, Target::VENUS, Target::MARS,
    Target::JUPITER, Target::SATURN, Target::URANUS, Target::NEPTUNE, Target::PLUTO };
  for (std::size_t i = 0; i < targets.size(); ++i) {
    const auto& facts = radius_facts(targets.at(i));
    EXPECT_EQ(facts.target, targets.at(i));
    EXPECT_DOUBLE_EQ(facts.radius.km(), radii.at(i));
    EXPECT_EQ(facts.definition, i == 0 ? IAU_2015_SOLAR_NOMINAL : i == 1 ? JPL_SSD_SATELLITE_MEAN : JPL_SSD_PLANET_EQUATORIAL);
  }
  EXPECT_EQ(RADIUS_FACTS.size(), targets.size());
  for (const auto target : { Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING, UNKNOWN_TARGET }) {
    EXPECT_THROW(static_cast<void>(radius_facts(target)), std::invalid_argument);
  }
}

TEST(ApparentDiameter, AnalyticTangentTriangleAndExactAuConversion) {
  // sin(30 degrees)=1/2, so a unit radius at two units' centre distance subtends 60 degrees.
  // The second value is an independent high-precision 2 asin(0.1) evaluation, in degrees.
  EXPECT_NEAR(diameter(DistanceKm { 149597870.7 }, DistanceAu { 2.0 }).deg(), 60.0, 2e-14);
  EXPECT_NEAR(diameter(DistanceKm { 14959787.07 }, DistanceAu { 1.0 }).deg(), 11.478340954533573, 4e-15);
  EXPECT_NEAR(diameter(DistanceKm { 1.0 }, DistanceAu { 2.0 / 149597870.7 }).deg() * 3600.0, 216000.0, 6e-11);
}

TEST(ApparentDiameter, SourceOwnGeometryUsesPrintedPrecisionNotProviderGap) {
  for (const auto& row : reference::ROWS) {
    SCOPED_TRACE(std::format("target {} JDE {}", row.target, row.jde_tt));
    EXPECT_NEAR(
      diameter(DistanceKm { row.radius_km }, DistanceAu { row.distance_au }).deg() * 3600.0,
      row.printed_arcsec,
      row.rounding_arcsec
    );
  }
}

TEST(ApparentDiameter, FullProvidersAndSelectedObserversUseCalibratedModelGaps) {
  for (const auto& row : reference::ROWS) {
    SCOPED_TRACE(std::format("target {} JDE {} height {}", row.target, row.jde_tt, row.height_km));
    const auto target = static_cast<Target>(row.target);
    const auto size = row.height_km == -9999.0 ? geocentric(row.jde_tt, target) : topocentric(
      { .jd_ut1 = row.jd_ut1, .jde_tt = row.jde_tt },
      { .location = { .latitude = AngleDeg { row.latitude_deg }, .longitude = AngleDeg { row.longitude_deg } },
        .elevation = DistanceKm { row.height_km } },
      target
    );
    EXPECT_NEAR(size.diameter.deg() * 3600.0, row.printed_arcsec, row.provider_arcsec);
    // Frozen pre-feature provider ranges: binary replay only, not an astronomical accuracy bound.
    // Allow about 15 mm for platform FMA-contraction rounding.
    EXPECT_NEAR(size.distance.au(), row.baseline_distance_au, 1e-13);
  }
}

TEST(ApparentDiameter, ExteriorBoundaryAndMalformedGeometry) {
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (const double bad : { nan, infinity, -infinity, 0.0, -0.0, -1.0 }) {
    EXPECT_THROW(static_cast<void>(diameter(DistanceKm { bad }, DistanceAu { 1.0 })), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(diameter(DistanceKm { 1.0 }, DistanceAu { bad })), std::invalid_argument);
  }
  for (const auto& facts : RADIUS_FACTS) {
    const double surface = facts.radius.au();
    EXPECT_THROW(static_cast<void>(diameter(facts.radius, DistanceAu { surface })), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(diameter(facts.radius, DistanceAu { std::nextafter(surface, 0.0) })), std::invalid_argument);
    const auto just_outside = diameter(facts.radius, DistanceAu { std::nextafter(surface, infinity) });
    EXPECT_GT(just_outside.deg() * 3600.0, 0.0);
    EXPECT_LT(just_outside.deg() * 3600.0, 648000.0);
  }
}

TEST(ApparentDiameter, HugeDistancesSubnormalRatiosAndZeroUnderflow) {
  const double huge = std::numeric_limits<double>::max();
  for (const auto& facts : RADIUS_FACTS) {
    const auto angle = diameter(facts.radius, DistanceAu { huge });
    EXPECT_TRUE(std::isfinite(angle.deg() * 3600.0));
    EXPECT_GT(angle.deg() * 3600.0, 0.0);
    EXPECT_LT(angle.deg() * 3600.0, 648000.0);
  }
  EXPECT_GT(diameter(DistanceKm { 1e-6 }, DistanceAu { huge }).deg(), 0.0);
  EXPECT_THROW(static_cast<void>(diameter(DistanceKm { 1e-300 }, DistanceAu { huge })), std::runtime_error);
  EXPECT_THROW(static_cast<void>(diameter(DistanceKm { 1e-300 }, DistanceAu { 1e300 })), std::runtime_error);
}

TEST(ApparentDiameter, ProviderErrorsAreNotTranslatedWithGeometry) {
  for (const double tt : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
    EXPECT_THROW(static_cast<void>(geocentric(tt, Target::SUN)), std::invalid_argument);
  }
  const auto domain = util::test::req(ephemeris::position_domain(Target::PLUTO));
  EXPECT_NO_THROW(static_cast<void>(geocentric(domain.front(), Target::PLUTO)));
  EXPECT_NO_THROW(static_cast<void>(geocentric(std::nextafter(domain.back(), domain.front()), Target::PLUTO)));
  EXPECT_THROW(static_cast<void>(geocentric(std::nextafter(domain.front(), 0.0), Target::PLUTO)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(geocentric(domain.back(), Target::PLUTO)), std::invalid_argument);
  // Existing finite remote provider evaluation fails as a computation error, not a window rejection.
  EXPECT_THROW(static_cast<void>(ephemeris::position(Target::SUN, 1e300)), std::runtime_error);
  EXPECT_THROW(static_cast<void>(geocentric(1e300, Target::SUN)), std::runtime_error);
  for (const auto& facts : RADIUS_FACTS) {
    if (facts.target == Target::PLUTO) { continue; }
    for (const double tt : ephemeris::target_facts(facts.target).rate_provider_domain) {
      EXPECT_NO_THROW(static_cast<void>(geocentric(tt, facts.target)));
      EXPECT_NO_THROW(static_cast<void>(geocentric(std::nextafter(tt, 0.0), facts.target)));
    }
  }
}

TEST(ApparentDiameter, ResolvedInteriorObserverIsNewQueryInvalidInput) {
  const julian_day::Times times { .jd_ut1 = 2451545.0, .jde_tt = 2451545.0 };
  const observer::Observer site {
    .location = { .latitude = AngleDeg { -10.9 }, .longitude = AngleDeg { -58.0 } },
    .elevation = DistanceKm { 396045.0 },
  };
  const auto position = observer::observe(times, site, Target::MOON);
  ASSERT_TRUE(std::isfinite(position.distance.km()));
  ASSERT_GT(position.distance.km(), 0.0);
  ASSERT_LE(position.distance.km(), 1737.4);
  EXPECT_THROW(static_cast<void>(topocentric(times, site, Target::MOON)), std::invalid_argument);
}

TEST(ApparentDiameter, ConstructedUnresolvedObserverPreservesUpstreamError) {
  const julian_day::Times times { .jd_ut1 = 2451545.0, .jde_tt = 2451545.0 };
  const auto moon = ephemeris::equatorial_position(Target::MOON, times.jde_tt);
  const double distance = util::test::req(moon.distance).km();
  const double equatorial = distance * std::cos(moon.declination.rad());
  const double polar = distance * std::sin(moon.declination.rad());
  const double e2 = 1.0 - observer::WGS84_POLAR_RATIO_SQUARED;
  // Invert WGS84 geocentric coordinates at the Moon's centre; no rounded site elevation is used.
  double φ = moon.declination.rad();
  for (uint32_t i = 0; i < 32; ++i) {
    const double normal = observer::WGS84_EQUATORIAL_RADIUS_KM / std::sqrt(1.0 - (e2 * std::sin(φ) * std::sin(φ)));
    φ = std::atan2(polar + (e2 * normal * std::sin(φ)), equatorial);
  }
  const double normal = observer::WGS84_EQUATORIAL_RADIUS_KM / std::sqrt(1.0 - (e2 * std::sin(φ) * std::sin(φ)));
  const double height = (equatorial / std::cos(φ)) - normal;
  const auto gast = sidereal::greenwich_apparent(times.jd_ut1, times.jde_tt, earth::nutation::Model::IAU_1980);
  const double longitude = std::remainder(moon.right_ascension.deg() - gast.deg(), 360.0);
  const observer::Observer site {
    .location = { .latitude = AngleDeg { toolbox::rad_to_deg(φ) }, .longitude = AngleDeg { longitude } },
    .elevation = DistanceKm { height },
  };
  SCOPED_TRACE(std::format("lat {:.17g} lon {:.17g} height_km {:.17g}", site.location.latitude.deg(), longitude, height));
  RecordProperty("constructed_site", std::format("lat={:.17g},lon={:.17g},height_km={:.17g}", site.location.latitude.deg(), longitude, height));
  EXPECT_THROW(static_cast<void>(observer::observe(times, site, Target::MOON)), std::runtime_error);
  EXPECT_THROW(static_cast<void>(topocentric(times, site, Target::MOON)), std::runtime_error);
}

TEST(ApparentDiameter, CivilCompositionPolesAndLargeElevation) {
  const calendar::Datetime civil { util::to_ymd(2026, 11, 7), 0.0 };
  for (const auto scale : { julian_day::Scale::UTC, julian_day::Scale::UT1 }) {
    for (const auto model : { delta_t::Model::ALGO2, delta_t::Model::ALGO5 }) {
      const auto times = julian_day::resolve_times(civil, scale, model);
      for (const auto& facts : RADIUS_FACTS) {
        const auto result = query(civil, scale, facts.target, std::nullopt, model);
        EXPECT_DOUBLE_EQ(result.times.jd_ut1, times.jd_ut1);
        EXPECT_DOUBLE_EQ(result.times.jde_tt, times.jde_tt);
        EXPECT_EQ(result.target, facts.target);
        EXPECT_EQ(result.source, ephemeris::source_of(facts.target));
        EXPECT_EQ(result.centre, Centre::GEOCENTRIC);
        EXPECT_EQ(result.delta_t_model, model);
        EXPECT_EQ(result.radius_definition, facts.definition);
        EXPECT_DOUBLE_EQ(result.radius.km(), facts.radius.km());
        EXPECT_DOUBLE_EQ(result.distance.au(), util::test::req(ephemeris::position(facts.target, times.jde_tt).distance).au());
        for (const double latitude : { -90.0, -33.85, 90.0 }) {
          const observer::Observer site {
            .location = { .latitude = AngleDeg { latitude }, .longitude = AngleDeg { 151.2 } },
            .elevation = DistanceKm { 1.706 },
          };
          const auto topo = query(civil, scale, facts.target, site, model);
          EXPECT_EQ(topo.centre, Centre::TOPOCENTRIC);
          EXPECT_EQ(topo.delta_t_model, model);
          EXPECT_DOUBLE_EQ(topo.distance.au(), observer::observe(times, site, facts.target).distance.au());
          EXPECT_DOUBLE_EQ(topo.diameter.deg(), diameter(facts.radius, topo.distance).deg());
        }
      }
    }
  }
  const auto huge = query(
    civil,
    julian_day::Scale::UTC,
    Target::MOON,
    observer::Observer {
      .location = { .latitude = AngleDeg { 0.0 }, .longitude = AngleDeg { 0.0 } },
      .elevation = DistanceKm { 1e300 },
    }
  );
  EXPECT_GT(huge.diameter.deg() * 3600.0, 0.0);
  EXPECT_TRUE(std::isfinite(huge.diameter.deg() * 3600.0));
}

TEST(ApparentDiameter, CivilAdmissionAndPhysicalTargetGuards) {
  const calendar::Datetime old { util::to_ymd(1000, 1, 1), 0.0 };
  EXPECT_NO_THROW(static_cast<void>(query(old, julian_day::Scale::UT1, Target::SUN)));
  EXPECT_THROW(static_cast<void>(query(old, julian_day::Scale::UTC, Target::SUN)), std::invalid_argument);
  const calendar::Datetime future { util::to_ymd(2036, 1, 1), 0.0 };
  EXPECT_THROW(static_cast<void>(query(future, julian_day::Scale::UT1, Target::SUN, std::nullopt, delta_t::Model::ALGO4)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(query(future, julian_day::Scale::UT1, Target::SUN, std::nullopt, static_cast<delta_t::Model>(255))), std::invalid_argument);
  for (const auto target : { Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING, UNKNOWN_TARGET }) {
    EXPECT_THROW(static_cast<void>(geocentric(2451545.0, target)), std::invalid_argument);
  }
}

} // namespace astro::apparent_diameter::test
