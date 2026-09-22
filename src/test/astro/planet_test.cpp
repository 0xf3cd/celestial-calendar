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
#include <string_view>

#include <gtest/gtest.h>

#include "coord_transform.hpp"
#include "planet.hpp"

// Retained material boundaries: V06 identifies the JPL Horizons rows, V15 the PyMeeus output rows,
// and V32 the Meeus worked-example values. They remain under their source terms and outside the
// project MIT grant.

namespace astro::planet::test {

namespace {

struct HorizonsRow {
  Planet planet;
  double jde;
  double lon_deg;
  double lat_deg;
  double r_au;
  double elongation_deg;
};

struct Tolerance {
  double lon_deg;
  double lat_deg;
  double r_au;
};

struct EquatorialRow {
  Planet planet;
  double jde;
  double right_ascension_deg;
  double declination_deg;
};

[[nodiscard]] auto wrapped_diff_deg(const double lhs, const double rhs) -> double {
  return std::fabs(std::remainder(lhs - rhs, 360.0));
}

// JPL Horizons API v1.2 apparent geocentric positions, collected 2026-09-22 by
// `statistics/planet_horizons_crawler.py`. Target sources are DE441 (Mercury/Venus), mar099,
// jup365_merged, sat441l, ura184_merged, and nep098_merged; the Earth center uses DE441 except
// for Neptune's nep098_merged response. Inputs are echoed JD(TT), center is 500@399, and quantity
// 31 is IAU76/80 apparent true-ecliptic-of-date longitude/latitude.
// Quantity 20 supplies apparent range. Quantity 23 supplies elongation, retained here to expose
// the conjunction rows where Horizons' relativistic light deflection is intentionally absent from
// this library. Six fixed epochs span 1901-2094 and include Meeus Example 33.a. For each planet, a
// source-only scan over every 0h TT day in 2026 selects the earliest minimum-elongation row.
//
// Measured worst fixed-epoch residuals (longitude arcsec, latitude arcsec, range km), by planet:
// Mercury 0.124 / 0.018 / 18; Venus 0.074 / 0.136 / 7; Mars 0.132 / 0.301 / 48;
// Jupiter 0.438 / 0.088 / 190; Saturn 0.285 / 0.036 / 364; Uranus 1.179 / 0.063 / 8627;
// Neptune 2.380 / 0.106 / 10013. The tolerances below are about 3x those measured model gaps,
// rounded outward. The conjunction set's largest latitude residual is 0.546 arcsec; its 1.8-arcsec
// latitude tolerance isolates the omitted light-deflection envelope without widening this table.
constexpr std::array HORIZONS_TOLERANCES {
  Tolerance { .lon_deg = 0.00012, .lat_deg = 0.00002, .r_au = 0.0000004 },
  Tolerance { .lon_deg = 0.00008, .lat_deg = 0.00012, .r_au = 0.0000002 },
  Tolerance { .lon_deg = 0.00012, .lat_deg = 0.00030, .r_au = 0.0000010 },
  Tolerance { .lon_deg = 0.00040, .lat_deg = 0.00008, .r_au = 0.0000040 },
  Tolerance { .lon_deg = 0.00030, .lat_deg = 0.00003, .r_au = 0.0000080 },
  Tolerance { .lon_deg = 0.00100, .lat_deg = 0.00006, .r_au = 0.0002000 },
  Tolerance { .lon_deg = 0.00200, .lat_deg = 0.00010, .r_au = 0.0003000 },
};
inline constexpr double HORIZONS_CONJUNCTION_LAT_TOLERANCE_DEG = 0.00050;

// NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
constexpr std::array HORIZONS_ROWS {
  // Planet           JDE             Longitude     Latitude     Range (AU)       Elongation
  HorizonsRow { Planet::MERCURY, 2415385.500000, 267.6792943, -0.5633804,  1.373869763054,  12.2426 },
  HorizonsRow { Planet::MERCURY, 2433651.500000, 276.5324109,  3.1400738,  0.681695297755,   7.9421 },
  HorizonsRow { Planet::MERCURY, 2448976.500000, 249.9701648,  1.0647727,  1.215797841064,  18.4154 },
  HorizonsRow { Planet::MERCURY, 2451545.000000, 271.8881138, -0.9947466,  1.415466038869,   8.5378 },
  HorizonsRow { Planet::MERCURY, 2461050.500000, 282.6257364, -1.4356425,  1.423626050263,   7.2545 },
  HorizonsRow { Planet::MERCURY, 2486166.250000, 184.9738312,  1.7885395,  0.998364743709,  18.0240 },
  HorizonsRow { Planet::VENUS,   2415385.500000, 250.7521091,  1.2743156,  1.398702846186,  29.1824 },
  HorizonsRow { Planet::VENUS,   2433651.500000, 296.3822625, -1.2556521,  1.655459568843,  12.6129 },
  HorizonsRow { Planet::VENUS,   2448976.500000, 313.0813433, -2.0848243,  0.910947737526,  44.7638 },
  HorizonsRow { Planet::VENUS,   2451545.000000, 241.5648812,  2.0663757,  1.137574425488,  38.8496 },
  HorizonsRow { Planet::VENUS,   2461050.500000, 290.5282414, -0.8225966,  1.710901928289,   1.1409 },
  HorizonsRow { Planet::VENUS,   2486166.250000, 206.6591465,  1.0241120,  1.714258623861,   3.8846 },
  HorizonsRow { Planet::MARS,    2415385.500000, 161.6438583,  3.2111597,  0.940204223428, 118.2169 },
  HorizonsRow { Planet::MARS,    2433651.500000, 316.1788525, -1.1438259,  2.107951589922,  32.3659 },
  HorizonsRow { Planet::MARS,    2448976.500000, 114.5595228,  3.4380850,  0.648327163255, 153.5876 },
  HorizonsRow { Planet::MARS,    2451545.000000, 327.9627159, -1.0677844,  1.849683834398,  47.6037 },
  HorizonsRow { Planet::MARS,    2461050.500000, 289.6110366, -0.9441998,  2.402937645969,   0.9526 },
  HorizonsRow { Planet::MARS,    2486166.250000, 164.4305612,  1.3220035,  2.320043393461,  38.5004 },
  HorizonsRow { Planet::JUPITER, 2415385.500000, 265.9582610,  0.3070135,  6.223068653381,  13.9542 },
  HorizonsRow { Planet::JUPITER, 2433651.500000, 335.4497273, -1.0612613,  5.528430331707,  51.6265 },
  HorizonsRow { Planet::JUPITER, 2448976.500000, 192.2857614,  1.2561848,  5.599110169546,  76.0735 },
  HorizonsRow { Planet::JUPITER, 2451545.000000,  25.2530382, -1.2621907,  4.621163711791, 104.8812 },
  HorizonsRow { Planet::JUPITER, 2461050.500000, 110.1568749,  0.2604282,  4.231754677875, 179.5064 },
  HorizonsRow { Planet::JUPITER, 2486166.250000,  36.6771567, -1.4930685,  3.989841443065, 166.1554 },
  HorizonsRow { Planet::SATURN,  2415385.500000, 277.7024090,  0.6001373, 11.049641867050,   2.2868 },
  HorizonsRow { Planet::SATURN,  2433651.500000, 182.3189304,  2.2815110,  9.216601597809, 101.5028 },
  HorizonsRow { Planet::SATURN,  2448976.500000, 315.1488052, -1.0136389, 10.514400202089,  46.8014 },
  HorizonsRow { Planet::SATURN,  2451545.000000,  40.3956514, -2.4448568,  8.652786072703, 119.9974 },
  HorizonsRow { Planet::SATURN,  2461050.500000, 356.7526587, -2.2304522,  9.856388585093,  67.0335 },
  HorizonsRow { Planet::SATURN,  2486166.250000, 139.6081875,  0.7956755,  9.533069780770,  63.3064 },
  HorizonsRow { Planet::URANUS,  2415385.500000, 254.3077740,  0.0069670, 19.941485975685,  25.6013 },
  HorizonsRow { Planet::URANUS,  2433651.500000,  97.1791847,  0.3278908, 17.903829221759, 173.3402 },
  HorizonsRow { Planet::URANUS,  2448976.500000, 286.9693359, -0.4111395, 20.500030417201,  18.6179 },
  HorizonsRow { Planet::URANUS,  2451545.000000, 314.8091306, -0.6583242, 20.727163790695,  34.4465 },
  HorizonsRow { Planet::URANUS,  2461050.500000,  57.7301994, -0.1950302, 18.867911516279, 127.9924 },
  HorizonsRow { Planet::URANUS,  2486166.250000, 358.8705108, -0.7806962, 19.174308071051, 155.9468 },
  HorizonsRow { Planet::NEPTUNE, 2415385.500000,  87.5190099, -1.2490013, 28.916376362683, 167.5481 },
  HorizonsRow { Planet::NEPTUNE, 2433651.500000, 199.4601398,  1.6212547, 30.378899683260,  84.3731 },
  HorizonsRow { Planet::NEPTUNE, 2448976.500000, 287.9114767,  0.6760477, 31.113313922977,  19.5669 },
  HorizonsRow { Planet::NEPTUNE, 2451545.000000, 303.1929742,  0.2350028, 31.024493240796,  22.8260 },
  HorizonsRow { Planet::NEPTUNE, 2461050.500000, 359.6406579, -1.3268558, 30.208190398996,  69.9088 },
  HorizonsRow { Planet::NEPTUNE, 2486166.250000, 155.5699856,  0.6277009, 30.820423423735,  47.3450 },
};

constexpr std::array HORIZONS_CONJUNCTION_ROWS {
  // Planet           JDE             Longitude     Latitude     Range (AU)       Elongation
  HorizonsRow { Planet::MERCURY, 2461175.500000,  54.6767325,  0.2197041,  1.321784397107, 0.5328 },
  HorizonsRow { Planet::VENUS,   2461046.500000, 285.4963796, -0.6865525,  1.710859919919, 0.7063 },
  HorizonsRow { Planet::MARS,    2461049.500000, 288.8392111, -0.9386427,  2.403865932231, 0.9463 },
  HorizonsRow { Planet::JUPITER, 2461251.500000, 126.5185991,  0.4742878,  6.301183302295, 0.5940 },
  HorizonsRow { Planet::SATURN,  2461124.500000,   4.6745508, -2.1247938, 10.489300817457, 2.1493 },
  HorizonsRow { Planet::URANUS,  2461183.500000,  61.5394414, -0.1606305, 20.477226502891, 0.3937 },
  HorizonsRow { Planet::NEPTUNE, 2461121.500000,   1.8259642, -1.3062454, 30.878786041244, 1.3820 },
};

// PyMeeus 0.5.12 output collected 2026-09-22 by the same script and epochs. The pinned call is
// `<Planet>.geocentric_position(Epoch(jde))`, returning apparent equatorial coordinates. PyMeeus
// independently transcribes the complete VSOP87D/Meeus formulas, but evaluates corrections at the
// retarded epoch and uses the target's heliocentric latitude in the FK5 longitude term. This layer
// therefore diagnoses coarse formula/coefficient wiring rather than observation-epoch semantics or
// absolute physical accuracy.
constexpr std::array PYMEEUS_ROWS {
  // Planet           JDE          Right ascension  Declination
  EquatorialRow { Planet::MERCURY, 2415385.50, 267.4597670259, -23.9936396660 },
  EquatorialRow { Planet::MERCURY, 2433651.50, 276.9498438629, -20.1504214403 },
  EquatorialRow { Planet::MERCURY, 2448976.50, 248.4970265344, -20.8921057320 },
  EquatorialRow { Planet::MERCURY, 2451545.00, 272.0733859689, -24.4188450611 },
  EquatorialRow { Planet::MERCURY, 2461050.50, 283.8685292711, -24.2680794491 },
  EquatorialRow { Planet::MERCURY, 2486166.25, 185.2744038464,  -0.3332296516 },
  EquatorialRow { Planet::VENUS,   2415385.50, 249.3555294153, -20.8069016997 },
  EquatorialRow { Planet::VENUS,   2433651.50, 298.6550400586, -22.1165462975 },
  EquatorialRow { Planet::VENUS,   2448976.50, 316.1727244604, -18.8880110113 },
  EquatorialRow { Planet::VENUS,   2451545.00, 239.8918496700, -18.4487219986 },
  EquatorialRow { Planet::VENUS,   2461050.50, 292.3353777995, -22.6831675577 },
  EquatorialRow { Planet::VENUS,   2486166.25, 205.1088057251,  -9.3206151554 },
  EquatorialRow { Planet::MARS,    2415385.50, 164.3112378685,  10.1670762274 },
  EquatorialRow { Planet::MARS,    2433651.50, 318.9958186021, -17.0844079765 },
  EquatorialRow { Planet::MARS,    2448976.50, 117.1473204965,  24.5927449328 },
  EquatorialRow { Planet::MARS,    2451545.00, 330.5162566390, -13.1826810606 },
  EquatorialRow { Planet::MARS,    2461050.50, 291.3710062754, -22.9391780785 },
  EquatorialRow { Planet::MARS,    2486166.25, 166.1722529240,   7.3455893973 },
  EquatorialRow { Planet::JUPITER, 2415385.50, 265.6059038673, -23.0820638528 },
  EquatorialRow { Planet::JUPITER, 2433651.50, 337.6591855005, -10.5034371113 },
  EquatorialRow { Planet::JUPITER, 2448976.50, 191.7901487851,  -3.6987072813 },
  EquatorialRow { Planet::JUPITER, 2451545.00,  23.8678581092,   8.5942904812 },
  EquatorialRow { Planet::JUPITER, 2461050.50, 111.8472614974,  22.1828994735 },
  EquatorialRow { Planet::JUPITER, 2486166.25,  34.8490573182,  12.3269031301 },
  EquatorialRow { Planet::SATURN,  2415385.50, 278.3487153773, -22.6275509509 },
  EquatorialRow { Planet::SATURN,  2433651.50, 183.0348275786,   1.1708747168 },
  EquatorialRow { Planet::SATURN,  2448976.50, 317.9243618376, -17.2613258091 },
  EquatorialRow { Planet::SATURN,  2451545.00,  38.7654196350,  12.6147658495 },
  EquatorialRow { Planet::SATURN,  2461050.50, 357.9074471680,  -3.3377599793 },
  EquatorialRow { Planet::SATURN,  2486166.25, 142.2804456434,  15.6852818821 },
  EquatorialRow { Planet::URANUS,  2415385.50, 252.9745305317, -22.5206328770 },
  EquatorialRow { Planet::URANUS,  2433651.50,  97.8370364129,  23.5807177360 },
  EquatorialRow { Planet::URANUS,  2448976.50, 288.4527420847, -22.7702876526 },
  EquatorialRow { Planet::URANUS,  2451545.00, 317.4748421514, -17.0203072658 },
  EquatorialRow { Planet::URANUS,  2461050.50,  55.5097156939,  19.4629289945 },
  EquatorialRow { Planet::URANUS,  2486166.25, 359.2740413431,  -1.1653609835 },
  EquatorialRow { Planet::NEPTUNE, 2415385.50,  87.3211910095,  22.1786568177 },
  EquatorialRow { Planet::NEPTUNE, 2433651.50, 198.5776170870,  -6.1169117658 },
  EquatorialRow { Planet::NEPTUNE, 2448976.50, 289.3105475041, -21.5708738417 },
  EquatorialRow { Planet::NEPTUNE, 2451545.00, 305.4329851057, -19.2131991442 },
  EquatorialRow { Planet::NEPTUNE, 2461050.50,   0.1985387754,  -1.3601094287 },
  EquatorialRow { Planet::NEPTUNE, 2486166.25, 157.6078514891,  10.0478070928 },
};
// NOLINTEND(modernize-use-designated-initializers) - Dense golden rows read by column.

} // namespace

TEST(Planet, MeeusExample33a) {
  // Meeus Example 33.a, Venus at JDE 2448976.5. The book's final values from the complete VSOP87
  // theory are α = 21h04m41.454s and δ = -18°53'16.84". Tolerances are half a printed unit:
  // 0.0075" in right ascension and 0.005" in declination.
  const auto coordinate = geocentric_coord::apparent(Planet::VENUS, 2448976.5);
  const auto equatorial = astro::coords::ecliptic_to_equatorial(
    coordinate.λ,
    coordinate.β,
    astro::earth::obliquity::true_obliquity(2448976.5)
  );
  constexpr double expected_right_ascension_deg = (21.0 + (4.0 / 60.0) + (41.454 / 3600.0)) * 15.0;
  constexpr double expected_declination_deg = -(18.0 + (53.0 / 60.0) + (16.84 / 3600.0));

  ASSERT_NEAR(equatorial.α.deg(), expected_right_ascension_deg, 0.0075 / 3600.0);
  ASSERT_NEAR(equatorial.δ.deg(), expected_declination_deg, 0.005 / 3600.0);
}

TEST(Planet, PymeeusCrossDataset) {
  // Measured maxima are 0.029" in α and 0.011" in δ. The tolerances are 0.072" and 0.036",
  // leaving 2.5x/3.3x margins while remaining well below the Horizons model-error envelope.
  for (const auto& row : PYMEEUS_ROWS) {
    const auto coordinate = geocentric_coord::apparent(row.planet, row.jde);
    const auto equatorial = astro::coords::ecliptic_to_equatorial(
      coordinate.λ,
      coordinate.β,
      astro::earth::obliquity::true_obliquity(row.jde)
    );

    ASSERT_LE(wrapped_diff_deg(equatorial.α.deg(), row.right_ascension_deg), 2e-5)
      << "right ascension at JDE " << row.jde;
    ASSERT_NEAR(equatorial.δ.deg(), row.declination_deg, 1e-5)
      << "declination at JDE " << row.jde;
  }
}

TEST(Planet, HorizonsGoldenDataset) {
  for (const auto& row : HORIZONS_ROWS) {
    const auto coordinate = geocentric_coord::apparent(row.planet, row.jde);
    const auto& tolerance = HORIZONS_TOLERANCES.at(static_cast<std::size_t>(row.planet));

    ASSERT_LE(wrapped_diff_deg(coordinate.λ.deg(), row.lon_deg), tolerance.lon_deg)
      << "longitude at JDE " << row.jde << ", elongation " << row.elongation_deg;
    ASSERT_NEAR(coordinate.β.deg(), row.lat_deg, tolerance.lat_deg)
      << "latitude at JDE " << row.jde << ", elongation " << row.elongation_deg;
    ASSERT_NEAR(coordinate.r.au(), row.r_au, tolerance.r_au)
      << "range at JDE " << row.jde << ", elongation " << row.elongation_deg;
  }
}

TEST(Planet, HorizonsConjunctionDataset) {
  for (const auto& row : HORIZONS_CONJUNCTION_ROWS) {
    const auto coordinate = geocentric_coord::apparent(row.planet, row.jde);
    const auto& tolerance = HORIZONS_TOLERANCES.at(static_cast<std::size_t>(row.planet));

    ASSERT_LE(wrapped_diff_deg(coordinate.λ.deg(), row.lon_deg), tolerance.lon_deg)
      << "longitude at JDE " << row.jde << ", elongation " << row.elongation_deg;
    ASSERT_NEAR(coordinate.β.deg(), row.lat_deg, HORIZONS_CONJUNCTION_LAT_TOLERANCE_DEG)
      << "latitude at JDE " << row.jde << ", elongation " << row.elongation_deg;
    ASSERT_NEAR(coordinate.r.au(), row.r_au, tolerance.r_au)
      << "range at JDE " << row.jde << ", elongation " << row.elongation_deg;
  }
}

TEST(Planet, LightTimeFixedPoint) {
  constexpr double jde_tt = 2451545.0;
  const auto earth = astro::earth::heliocentric_coord::vsop87d(jde_tt);
  const auto coordinate = detail::light_time_corrected(Planet::JUPITER, jde_tt, earth);
  const double retarded_jde_tt = jde_tt
                               - (astro::earth::aberration::LIGHT_TIME_DAYS_PER_AU * coordinate.r.au());
  const auto target_rect = detail::rectangular(detail::heliocentric(Planet::JUPITER, retarded_jde_tt));
  const auto earth_rect = detail::rectangular(earth);
  const detail::RectangularCoordinate geocentric {
    .x = target_rect.x - earth_rect.x,
    .y = target_rect.y - earth_rect.y,
    .z = target_rect.z - earth_rect.z,
  };
  const double expected_range = std::hypot(geocentric.x, geocentric.y, geocentric.z);
  const auto expected_lon = astro::toolbox::AngleDeg {
    astro::toolbox::rad_to_deg(std::atan2(geocentric.y, geocentric.x))
  }.normalize();
  const auto expected_lat = astro::toolbox::AngleDeg {
    astro::toolbox::rad_to_deg(std::atan2(geocentric.z, std::hypot(geocentric.x, geocentric.y)))
  };

  // One JDE ULP can move the recomputed target by about 1e-10 degrees and 1e-11 AU here.
  ASSERT_LE(wrapped_diff_deg(coordinate.λ.deg(), expected_lon.deg()), 1e-9);
  ASSERT_NEAR(coordinate.β.deg(), expected_lat.deg(), 1e-9);
  ASSERT_NEAR(coordinate.r.au(), expected_range, 1e-10);
}

TEST(Planet, ObservationEpochCorrectionChain) {
  constexpr double jde_tt = 2448976.5;
  const auto earth = astro::earth::heliocentric_coord::vsop87d(jde_tt);
  const auto geometric = detail::light_time_corrected(Planet::VENUS, jde_tt, earth);
  const auto aberration = detail::aberration(jde_tt, geometric, earth);
  const astro::toolbox::SphericalCoordinate aberrated {
    .λ = geometric.λ + aberration.Δλ,
    .β = geometric.β + aberration.Δβ,
    .r = geometric.r,
  };
  const auto fk5 = astro::sun::geocentric_coord::fk5_correction(jde_tt, aberrated);
  const auto nutation = astro::earth::nutation::longitude(jde_tt);
  const astro::toolbox::SphericalCoordinate expected {
    .λ = (aberrated.λ + fk5.Δλ + nutation).normalize(),
    .β = aberrated.β + fk5.Δβ,
    .r = aberrated.r,
  };
  const auto coordinate = geocentric_coord::apparent(Planet::VENUS, jde_tt);

  ASSERT_DOUBLE_EQ(coordinate.λ.deg(), expected.λ.deg());
  ASSERT_DOUBLE_EQ(coordinate.β.deg(), expected.β.deg());
  ASSERT_DOUBLE_EQ(coordinate.r.au(), expected.r.au());
}

TEST(Planet, RejectsInvalidInput) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) - Exercises the invalid-enumerator contract.
  const auto invalid_planet = static_cast<Planet>(255);
  EXPECT_THROW(
    static_cast<void>(geocentric_coord::apparent(Planet::MARS, std::numeric_limits<double>::quiet_NaN())),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(geocentric_coord::apparent(Planet::MARS, std::numeric_limits<double>::infinity())),
    std::invalid_argument
  );
  try {
    static_cast<void>(geocentric_coord::apparent(Planet::MERCURY, std::numeric_limits<double>::max()));
    FAIL() << "finite evaluation failure did not throw";
  } catch (const std::runtime_error& error) {
    const std::string_view message { error.what() };
    EXPECT_NE(message.find("Mercury"), std::string_view::npos);
    EXPECT_NE(message.find("JDE(TT)"), std::string_view::npos);
  }
  EXPECT_THROW(
    static_cast<void>(geocentric_coord::apparent(invalid_planet, 2451545.0)),
    std::invalid_argument
  );
}

} // namespace astro::planet::test
