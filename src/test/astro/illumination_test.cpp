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

#include "illumination.hpp"
#include "moon_phase.hpp"
#include "planet.hpp"

// Retained material boundary: V06 identifies the JPL Horizons rows. They remain under their
// source terms and outside the project MIT grant.

namespace astro::illumination::test {

namespace {

struct GeometryRow {
  astro::planet::Planet planet;
  double jde_tt;
  double angular_separation_deg;
  double phase_angle_deg;
  double illuminated_fraction;
};

struct GeometryTolerance {
  double angular_separation_deg;
  double phase_angle_deg;
  double illuminated_fraction;
};

// JPL Horizons API v1.2 observer quantities 23, 31, 43, and 10, collected 2026-09-26 and
// 2026-09-28 by `statistics/planet_horizons_crawler.py` and
// `statistics/pluto_horizons_crawler.py`. Center is 500@399, epochs are TT, and quantity 31 is
// IAU76/80 apparent true-ecliptic-of-date longitude/latitude. Separation is independently derived
// from the Sun and planet quantity-31 vectors, then checked against quantity 23. The epochs are the
// fixed, source-selected dates used by the planetary-position dataset. Pluto reuses its 11
// position-dataset audit witnesses: domain, book, J2000, and wrap anchors; its 2025 conjunction; and
// residual maxima.
// NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
constexpr std::array HORIZONS_ROWS {
  // Planet           JDE             Separation    Phase       Fraction
  GeometryRow { astro::planet::Planet::MERCURY, 2415385.500000,  12.2425872,  26.7915, 0.94632630 },
  GeometryRow { astro::planet::Planet::MERCURY, 2433651.500000,   7.9421004, 155.0714, 0.04658300 },
  GeometryRow { astro::planet::Planet::MERCURY, 2448976.500000,  18.4154311,  47.7536, 0.83616000 },
  GeometryRow { astro::planet::Planet::MERCURY, 2451545.000000,   8.5377870,  18.2451, 0.97486280 },
  GeometryRow { astro::planet::Planet::MERCURY, 2461050.500000,   7.2545079,  15.4981, 0.98181970 },
  GeometryRow { astro::planet::Planet::MERCURY, 2486166.250000,  18.0239818,  80.8130, 0.57982880 },
  GeometryRow { astro::planet::Planet::VENUS,   2415385.500000,  29.1823892,  41.5894, 0.87396030 },
  GeometryRow { astro::planet::Planet::VENUS,   2433651.500000,  12.6129231,  17.1413, 0.97779050 },
  GeometryRow { astro::planet::Planet::VENUS,   2448976.500000,  44.7637694,  72.9484, 0.64661650 },
  GeometryRow { astro::planet::Planet::VENUS,   2451545.000000,  38.8495679,  58.9287, 0.75805220 },
  GeometryRow { astro::planet::Planet::VENUS,   2461050.500000,   1.1409498,   1.5366, 0.99982020 },
  GeometryRow { astro::planet::Planet::VENUS,   2486166.250000,   3.8845986,   5.3605, 0.99781330 },
  GeometryRow { astro::planet::Planet::MARS,    2415385.500000, 118.2168921,  31.6577, 0.92559950 },
  GeometryRow { astro::planet::Planet::MARS,    2433651.500000,  32.3658996,  22.3901, 0.96230590 },
  GeometryRow { astro::planet::Planet::MARS,    2448976.500000, 153.5875777,  15.9655, 0.98071360 },
  GeometryRow { astro::planet::Planet::MARS,    2451545.000000,  47.6036562,  31.4594, 0.92650520 },
  GeometryRow { astro::planet::Planet::MARS,    2461050.500000,   0.9525826,   0.6604, 0.99996680 },
  GeometryRow { astro::planet::Planet::MARS,    2486166.250000,  38.5004054,  21.9680, 0.96369660 },
  GeometryRow { astro::planet::Planet::JUPITER, 2415385.500000,  13.9541586,   2.5790, 0.99949360 },
  GeometryRow { astro::planet::Planet::JUPITER, 2433651.500000,  51.6265395,   8.9058, 0.99397210 },
  GeometryRow { astro::planet::Planet::JUPITER, 2448976.500000,  76.0735017,  10.0996, 0.99225220 },
  GeometryRow { astro::planet::Planet::JUPITER, 2451545.000000, 104.8811966,  11.0320, 0.99076030 },
  GeometryRow { astro::planet::Planet::JUPITER, 2461050.500000, 179.5063934,   0.0934, 0.99999930 },
  GeometryRow { astro::planet::Planet::JUPITER, 2486166.250000, 166.1554418,   2.7561, 0.99942170 },
  GeometryRow { astro::planet::Planet::SATURN,  2415385.500000,   2.2868101,   0.2250, 0.99999610 },
  GeometryRow { astro::planet::Planet::SATURN,  2433651.500000, 101.5027950,   5.8466, 0.99739910 },
  GeometryRow { astro::planet::Planet::SATURN,  2448976.500000,  46.8013784,   4.1664, 0.99867860 },
  GeometryRow { astro::planet::Planet::SATURN,  2451545.000000, 119.9973712,   5.3192, 0.99784690 },
  GeometryRow { astro::planet::Planet::SATURN,  2461050.500000,  67.0335410,   5.4582, 0.99773290 },
  GeometryRow { astro::planet::Planet::SATURN,  2486166.250000,  63.3063747,   5.6036, 0.99761060 },
  GeometryRow { astro::planet::Planet::URANUS,  2415385.500000,  25.6013359,   1.2786, 0.99987550 },
  GeometryRow { astro::planet::Planet::URANUS,  2433651.500000, 173.3401884,   0.3453, 0.99999090 },
  GeometryRow { astro::planet::Planet::URANUS,  2448976.500000,  18.6178675,   0.9183, 0.99993580 },
  GeometryRow { astro::planet::Planet::URANUS,  2451545.000000,  34.4464982,   1.5984, 0.99980540 },
  GeometryRow { astro::planet::Planet::URANUS,  2461050.500000, 127.9924132,   2.2782, 0.99960480 },
  GeometryRow { astro::planet::Planet::URANUS,  2486166.250000, 155.9468060,   1.1587, 0.99989780 },
  GeometryRow { astro::planet::Planet::NEPTUNE, 2415385.500000, 167.5480868,   0.4059, 0.99998750 },
  GeometryRow { astro::planet::Planet::NEPTUNE, 2433651.500000,  84.3730939,   1.8519, 0.99973880 },
  GeometryRow { astro::planet::Planet::NEPTUNE, 2448976.500000,  19.5668631,   0.6243, 0.99997030 },
  GeometryRow { astro::planet::Planet::NEPTUNE, 2451545.000000,  22.8259649,   0.7246, 0.99996000 },
  GeometryRow { astro::planet::Planet::NEPTUNE, 2461050.500000,  69.9087509,   1.7699, 0.99976150 },
  GeometryRow { astro::planet::Planet::NEPTUNE, 2486166.250000,  47.3449665,   1.3949, 0.99985180 },
  GeometryRow { astro::planet::Planet::PLUTO,   2409543.500000, 137.0239544,   0.7901, 0.99995250 },
  GeometryRow { astro::planet::Planet::PLUTO,   2448908.500000,  34.4155197,   1.0864, 0.99991010 },
  GeometryRow { astro::planet::Planet::PLUTO,   2451545.000000,  30.7183554,   0.9534, 0.99993080 },
  GeometryRow { astro::planet::Planet::PLUTO,   2460697.020714,   3.2963727,   0.0919, 0.99999940 },
  GeometryRow { astro::planet::Planet::PLUTO,   2470066.864649, 159.7100810,   0.4798, 0.99998250 },
  GeometryRow { astro::planet::Planet::PLUTO,   2475818.500000,  86.3172609,   1.2952, 0.99987230 },
  GeometryRow { astro::planet::Planet::PLUTO,   2475819.500000,  87.2314829,   1.2964, 0.99987200 },
  GeometryRow { astro::planet::Planet::PLUTO,   2475842.500000, 108.2904379,   1.2328, 0.99988430 },
  GeometryRow { astro::planet::Planet::PLUTO,   2475843.500000, 109.2059919,   1.2261, 0.99988550 },
  GeometryRow { astro::planet::Planet::PLUTO,   2487903.349095,  82.5919347,   1.1815, 0.99989370 },
  GeometryRow { astro::planet::Planet::PLUTO,   2488068.500000, 111.7833940,   1.0686, 0.99991300 },
};
// NOLINTEND(modernize-use-designated-initializers) - Dense golden rows read by column.

// Measured maxima by planet (separation deg / phase deg / fraction): Mercury
// .000008/.010967/.000095, Venus .000049/.007003/.000049, Mars .000082/.005459/.000025,
// Jupiter .000095/.002827/.000004, Saturn .000055/.002065/.000002,
// Uranus .000355/.001343/.000001, Neptune .000637/.001110/.000001, and Pluto
// .000877/.001115/.00000017 over the 267-point seed-298 crawl.
// Tolerances are about 3x those independent model gaps, rounded outward.
constexpr std::array HORIZONS_TOLERANCES {
  GeometryTolerance { .angular_separation_deg = 0.00003, .phase_angle_deg = 0.0350, .illuminated_fraction = 0.00030 },
  GeometryTolerance { .angular_separation_deg = 0.00015, .phase_angle_deg = 0.0220, .illuminated_fraction = 0.00015 },
  GeometryTolerance { .angular_separation_deg = 0.00025, .phase_angle_deg = 0.0170, .illuminated_fraction = 0.00008 },
  GeometryTolerance { .angular_separation_deg = 0.00030, .phase_angle_deg = 0.0090, .illuminated_fraction = 0.000012 },
  GeometryTolerance { .angular_separation_deg = 0.00020, .phase_angle_deg = 0.0065, .illuminated_fraction = 0.000006 },
  GeometryTolerance { .angular_separation_deg = 0.00110, .phase_angle_deg = 0.0041, .illuminated_fraction = 0.000002 },
  GeometryTolerance { .angular_separation_deg = 0.00200, .phase_angle_deg = 0.0035, .illuminated_fraction = 0.000002 },
  GeometryTolerance { .angular_separation_deg = 0.00300, .phase_angle_deg = 0.0040, .illuminated_fraction = 0.0000005 },
};

} // namespace

TEST(Illumination, MoonWrapperPreservesFormulaPath) {
  const astro::toolbox::SphericalCoordinate sun_pos {
    .λ = astro::toolbox::AngleDeg { 20.6579 },
    .β = astro::toolbox::AngleDeg { 8.6964 },
    .r = astro::toolbox::DistanceAu { 1.0 },
  };
  const astro::toolbox::SphericalCoordinate moon_pos {
    .λ = astro::toolbox::AngleDeg { 134.6885 },
    .β = astro::toolbox::AngleDeg { 13.7684 },
    .r = astro::toolbox::DistanceAu { astro::toolbox::DistanceKm { 368410.0 } },
  };

  const auto result = geometry(sun_pos, moon_pos);
  const auto moon_phase_angle = astro::moon_phase::illumination::phase_angle(sun_pos, moon_pos);
  ASSERT_DOUBLE_EQ(result.phase_angle.deg(), moon_phase_angle.deg());
  ASSERT_DOUBLE_EQ(
    result.illuminated_fraction,
    astro::moon_phase::illumination::fraction(moon_phase_angle)
  );
}

TEST(Illumination, IdenticalDirectionsRemainFinite) {
  const astro::toolbox::SphericalCoordinate source_pos {
    .λ = astro::toolbox::AngleDeg { 100.0 },
    .β = astro::toolbox::AngleDeg { 0.100008 },
    .r = astro::toolbox::DistanceAu { 1.0 },
  };
  const astro::toolbox::SphericalCoordinate target_pos {
    .λ = astro::toolbox::AngleDeg { 100.0 },
    .β = astro::toolbox::AngleDeg { 0.100008 },
    .r = astro::toolbox::DistanceAu { 0.5 },
  };

  const auto result = geometry(source_pos, target_pos);
  ASSERT_TRUE(std::isfinite(result.angular_separation.deg()));
  ASSERT_TRUE(std::isfinite(result.phase_angle.deg()));
  ASSERT_DOUBLE_EQ(result.angular_separation.deg(), 0.0);
  ASSERT_DOUBLE_EQ(result.illuminated_fraction, 0.0);
}

TEST(Illumination, NearCoincidentDirectionsPreserveGeometry) {
  const astro::toolbox::SphericalCoordinate source_pos {
    .λ = astro::toolbox::AngleDeg { 0.0 },
    .β = astro::toolbox::AngleDeg { 0.0 },
    .r = astro::toolbox::DistanceAu { 1.0 },
  };
  const astro::toolbox::SphericalCoordinate target_pos {
    .λ = astro::toolbox::AngleDeg { 1e-7 },
    .β = astro::toolbox::AngleDeg { 0.0 },
    .r = astro::toolbox::DistanceAu { 1.0 },
  };

  // Equal radii form an isosceles triangle: i = (180° - ψ) / 2 and k = (1 + cos i) / 2.
  const auto result = geometry(source_pos, target_pos);
  ASSERT_NEAR(result.angular_separation.deg(), 1e-7, 1e-15);
  ASSERT_NEAR(result.phase_angle.deg(), 89.99999995, 1e-9);
  ASSERT_NEAR(result.illuminated_fraction, 0.5000000004363323, 1e-12);
}

TEST(Illumination, HorizonsPlanetaryGeometry) {
  for (const auto& row : HORIZONS_ROWS) {
    const auto result = astro::planet::observation::geometry(row.planet, row.jde_tt);
    const auto& tolerance = HORIZONS_TOLERANCES.at(static_cast<std::size_t>(row.planet));
    ASSERT_NEAR(
      result.angular_separation.deg(),
      row.angular_separation_deg,
      tolerance.angular_separation_deg
    );
    ASSERT_NEAR(result.phase_angle.deg(), row.phase_angle_deg, tolerance.phase_angle_deg);
    ASSERT_NEAR(result.illuminated_fraction, row.illuminated_fraction, tolerance.illuminated_fraction);
  }
}

TEST(Illumination, PlutoGeometryDomain) {
  const double before = std::nextafter(
    astro::planet::detail::pluto::APPARENT_START_JDE_TT,
    -std::numeric_limits<double>::infinity()
  );
  const double last = std::nextafter(
    astro::planet::detail::pluto::APPARENT_END_JDE_TT,
    astro::planet::detail::pluto::APPARENT_START_JDE_TT
  );
  EXPECT_THROW(
    static_cast<void>(astro::planet::observation::geometry(astro::planet::Planet::PLUTO, before)),
    std::invalid_argument
  );
  EXPECT_NO_THROW(static_cast<void>(astro::planet::observation::geometry(
    astro::planet::Planet::PLUTO,
    astro::planet::detail::pluto::APPARENT_START_JDE_TT
  )));
  EXPECT_NO_THROW(static_cast<void>(
    astro::planet::observation::geometry(astro::planet::Planet::PLUTO, last)
  ));
  EXPECT_THROW(
    static_cast<void>(astro::planet::observation::geometry(
      astro::planet::Planet::PLUTO,
      astro::planet::detail::pluto::APPARENT_END_JDE_TT
    )),
    std::invalid_argument
  );
}

TEST(Illumination, RejectsInvalidPlanetInput) {
  EXPECT_THROW(
    static_cast<void>(astro::planet::observation::geometry(
      astro::planet::Planet::PLUTO,
      std::numeric_limits<double>::quiet_NaN()
    )),
    std::invalid_argument
  );
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) - Exercises the invalid-enumerator contract.
  const auto invalid_planet = static_cast<astro::planet::Planet>(255);
  EXPECT_THROW(
    static_cast<void>(astro::planet::observation::geometry(invalid_planet, 2451545.0)),
    std::invalid_argument
  );
}

} // namespace astro::illumination::test
