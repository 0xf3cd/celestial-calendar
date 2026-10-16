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
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>

#include "coord_transform.hpp"
#include "planet.hpp"

// Retained material boundaries: V06 identifies the JPL Horizons rows, V15 the PyMeeus output rows,
// V32 the Meeus worked-example values, and V45/#298 the official PLUTO95 vectors. They remain under
// their source terms and outside the project MIT grant.

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

struct RetrogradeRow {
  Planet planet;
  double jde;
  bool retrograde;
};

struct Pluto95Row {
  double jde_tt;
  double x_au;
  double y_au;
  double z_au;
};

struct FrameRow {
  Planet planet;
  double observation_jde_tt;
  double lon_deg;
  double lat_deg;
  double r_au;
};

[[nodiscard]] auto wrapped_diff_deg(const double lhs, const double rhs) -> double {
  return std::fabs(std::remainder(lhs - rhs, 360.0));
}

[[nodiscard]] auto raw_heliocentric(const Planet planet, const double jde_tt) -> astro::toolbox::SphericalCoordinate {
  using enum astro::vsop87d::Planet;
  switch (planet) {
    case Planet::MERCURY: return detail::heliocentric<MER>(jde_tt);
    case Planet::VENUS:   return detail::heliocentric<VEN>(jde_tt);
    case Planet::MARS:    return detail::heliocentric<MAR>(jde_tt);
    case Planet::JUPITER: return detail::heliocentric<JUP>(jde_tt);
    case Planet::SATURN:  return detail::heliocentric<SAT>(jde_tt);
    case Planet::URANUS:  return detail::heliocentric<URA>(jde_tt);
    case Planet::NEPTUNE: return detail::heliocentric<NEP>(jde_tt);
    default: throw std::logic_error { "Unexpected frame-test planet" };
  }
}

// JPL Horizons API v1.2 apparent geocentric positions, collected 2026-09-22 and 2026-09-28 by
// `statistics/planet_horizons_crawler.py` and `statistics/pluto_horizons_crawler.py`. Target sources
// are DE441 (Mercury/Venus), mar099,
// jup365_merged, sat441l, ura184_merged, nep098_merged, and Pluto's plu060_merged; the Earth center
// uses DE441 except for Neptune's nep098_merged response. Inputs are echoed JD(TT), center is
// 500@399, and quantity 31 is IAU76/80 apparent true-ecliptic-of-date longitude/latitude.
// Quantity 20 supplies apparent range. Quantity 23 supplies elongation, retained here to expose
// the conjunction rows where Horizons' relativistic light deflection is intentionally absent from
// this library. The Mercury-Neptune block uses six fixed epochs spanning 1901-2094. Pluto keeps 11
// audit witnesses spanning its bounded model: domain, book, J2000, and wrap anchors; the 2025
// conjunction; and the seed-298 longitude, latitude, and range residual maxima.
// For each Mercury-Neptune planet, a 2026 scan selects the earliest minimum-elongation row.
//
// Measured worst fixed-epoch residuals (longitude arcsec, latitude arcsec, range km), by planet:
// Mercury 0.124 / 0.018 / 18; Venus 0.074 / 0.136 / 7; Mars 0.132 / 0.301 / 48;
// Jupiter 0.438 / 0.088 / 190; Saturn 0.285 / 0.036 / 364; Uranus 1.179 / 0.063 / 8627;
// Neptune 2.380 / 0.106 / 10013. Pluto's seed-298 267-point maxima are 3.378 / 0.388 / 51219.
// The tolerances below are about 3x those measured model gaps, rounded outward. The conjunction set's
// largest latitude residual is 0.546 arcsec; its 1.8-arcsec latitude tolerance isolates the omitted
// light-deflection envelope without widening this table.
constexpr std::array HORIZONS_TOLERANCES {
  Tolerance { .lon_deg = 0.00012, .lat_deg = 0.00002, .r_au = 0.0000004 },
  Tolerance { .lon_deg = 0.00008, .lat_deg = 0.00012, .r_au = 0.0000002 },
  Tolerance { .lon_deg = 0.00012, .lat_deg = 0.00030, .r_au = 0.0000010 },
  Tolerance { .lon_deg = 0.00040, .lat_deg = 0.00008, .r_au = 0.0000040 },
  Tolerance { .lon_deg = 0.00030, .lat_deg = 0.00003, .r_au = 0.0000080 },
  Tolerance { .lon_deg = 0.00100, .lat_deg = 0.00006, .r_au = 0.0002000 },
  Tolerance { .lon_deg = 0.00200, .lat_deg = 0.00010, .r_au = 0.0003000 },
  Tolerance { .lon_deg = 0.00300, .lat_deg = 0.00040, .r_au = 0.0011000 },
};
inline constexpr double HORIZONS_CONJUNCTION_LAT_TOLERANCE_DEG = 0.00050;

// NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
// Retained data: v06-planets.
constexpr std::array HORIZONS_ROWS {
  // Planet           JDE             Longitude     Latitude     Range (AU)       Elongation
  HorizonsRow { Planet::MERCURY, 2415385.500000, 267.6792943, -0.5633804,  1.373869763054,  12.2426 },
  HorizonsRow { Planet::MERCURY, 2433651.500000, 276.5324109,  3.1400738,  0.681695297755,   7.9421 },
  HorizonsRow { Planet::MERCURY, 2448976.500000, 249.9701648,  1.0647727,  1.215797841064,  18.4154 },
  HorizonsRow { Planet::MERCURY, 2451545.000000, 271.8881138, -0.9947466,  1.415466038869,   8.5378 },
  HorizonsRow { Planet::MERCURY, 2461050.500000, 282.6257364, -1.4356425,  1.423626050263,   7.2545 },
  HorizonsRow { Planet::MERCURY, 2486166.250000, 184.9738311,  1.7885395,  0.998364743709,  18.0240 },
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
  HorizonsRow { Planet::MARS,    2486166.250000, 164.4305611,  1.3220035,  2.320043393461,  38.5004 },
  HorizonsRow { Planet::JUPITER, 2415385.500000, 265.9582610,  0.3070135,  6.223068653381,  13.9542 },
  HorizonsRow { Planet::JUPITER, 2433651.500000, 335.4497273, -1.0612613,  5.528430331707,  51.6265 },
  HorizonsRow { Planet::JUPITER, 2448976.500000, 192.2857614,  1.2561848,  5.599110169546,  76.0735 },
  HorizonsRow { Planet::JUPITER, 2451545.000000,  25.2530382, -1.2621907,  4.621163711791, 104.8812 },
  HorizonsRow { Planet::JUPITER, 2461050.500000, 110.1568749,  0.2604282,  4.231754677875, 179.5064 },
  HorizonsRow { Planet::JUPITER, 2486166.250000,  36.6771566, -1.4930685,  3.989841443065, 166.1554 },
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
  HorizonsRow { Planet::URANUS,  2486166.250000, 358.8705107, -0.7806962, 19.174308071051, 155.9468 },
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

constexpr std::array PLUTO_HORIZONS_ROWS {
  // Planet         JDE             Longitude     Latitude      Range (AU)       Elongation
  HorizonsRow { Planet::PLUTO, 2409543.500000,  60.4926095, -13.0103766, 47.845265553431, 137.0240 },
  HorizonsRow { Planet::PLUTO, 2448908.500000, 231.5936103,  14.1904754, 30.528724955163,  34.4155 },
  HorizonsRow { Planet::PLUTO, 2451545.000000, 251.4547382,  10.8552594, 31.064358652861,  30.7184 },
  HorizonsRow { Planet::PLUTO, 2460697.020714, 301.7161521,  -3.2964854, 36.167510444428,   3.2964 },
  HorizonsRow { Planet::PLUTO, 2470066.864649, 339.2464309, -13.3376582, 40.653589839312, 159.7101 },
  HorizonsRow { Planet::PLUTO, 2475818.500000, 359.9970053, -15.8752852, 44.931022774971,  86.3173 },
  HorizonsRow { Planet::PLUTO, 2475819.500000,   0.0016165, -15.8814997, 44.915318746319,  87.2315 },
  HorizonsRow { Planet::PLUTO, 2475842.500000,   0.0040505, -16.0236324, 44.559941461112, 108.2904 },
  HorizonsRow { Planet::PLUTO, 2475843.500000, 359.9996438, -16.0296280, 44.545191586626, 109.2060 },
  HorizonsRow { Planet::PLUTO, 2487903.349095,  34.3095740, -16.7726454, 49.028134176318,  82.5919 },
  HorizonsRow { Planet::PLUTO, 2488068.500000,  32.4086290, -16.9269016, 48.559173648138, 111.7834 },
};

// The same Horizons query was replayed over every 0h TT day in 2025-2027. Each expected direction
// is the sign of the wrap-aware source-longitude difference across the surrounding two days. The
// station cases sit three days outside the source's daily sign-change brackets, avoiding an exact-
// station boolean; the clear cases select each planet's largest positive and negative daily change.
// Mercury's first pair brackets its March 15-April 7, 2025 retrograde interval at day resolution.
constexpr std::array HORIZONS_RETROGRADE_ROWS {
  // Planet           JDE             Retrograde
  RetrogradeRow { Planet::MERCURY, 2460746.500000, false },  // station-before
  RetrogradeRow { Planet::MERCURY, 2460753.500000, true  },  // station-after
  RetrogradeRow { Planet::MERCURY, 2460769.500000, true  },  // station-before
  RetrogradeRow { Planet::MERCURY, 2460776.500000, false },  // station-after
  RetrogradeRow { Planet::MERCURY, 2460826.500000, false },  // clear-direct
  RetrogradeRow { Planet::MERCURY, 2460999.500000, true  },  // clear-retrograde
  RetrogradeRow { Planet::VENUS,   2460733.500000, false },  // station-before
  RetrogradeRow { Planet::VENUS,   2460740.500000, true  },  // station-after
  RetrogradeRow { Planet::VENUS,   2460775.500000, true  },  // station-before
  RetrogradeRow { Planet::VENUS,   2460782.500000, false },  // station-after
  RetrogradeRow { Planet::VENUS,   2461030.500000, false },  // clear-direct
  RetrogradeRow { Planet::VENUS,   2460757.500000, true  },  // clear-retrograde
  RetrogradeRow { Planet::MARS,    2461412.500000, false },  // station-before
  RetrogradeRow { Planet::MARS,    2461419.500000, true  },  // station-after
  RetrogradeRow { Planet::MARS,    2461493.500000, true  },  // station-before
  RetrogradeRow { Planet::MARS,    2461500.500000, false },  // station-after
  RetrogradeRow { Planet::MARS,    2461098.500000, false },  // clear-direct
  RetrogradeRow { Planet::MARS,    2460690.500000, true  },  // clear-retrograde
  RetrogradeRow { Planet::JUPITER, 2460987.500000, false },  // station-before
  RetrogradeRow { Planet::JUPITER, 2460994.500000, true  },  // station-after
  RetrogradeRow { Planet::JUPITER, 2461107.500000, true  },  // station-before
  RetrogradeRow { Planet::JUPITER, 2461114.500000, false },  // station-after
  RetrogradeRow { Planet::JUPITER, 2460849.500000, false },  // clear-direct
  RetrogradeRow { Planet::JUPITER, 2461050.500000, true  },  // clear-retrograde
  RetrogradeRow { Planet::SATURN,  2460866.500000, false },  // station-before
  RetrogradeRow { Planet::SATURN,  2460873.500000, true  },  // station-after
  RetrogradeRow { Planet::SATURN,  2461004.500000, true  },  // station-before
  RetrogradeRow { Planet::SATURN,  2461011.500000, false },  // station-after
  RetrogradeRow { Planet::SATURN,  2461504.500000, false },  // clear-direct
  RetrogradeRow { Planet::SATURN,  2461696.500000, true  },  // clear-retrograde
  RetrogradeRow { Planet::URANUS,  2460921.500000, false },  // station-before
  RetrogradeRow { Planet::URANUS,  2460928.500000, true  },  // station-after
  RetrogradeRow { Planet::URANUS,  2461072.500000, true  },  // station-before
  RetrogradeRow { Planet::URANUS,  2461079.500000, false },  // station-after
  RetrogradeRow { Planet::URANUS,  2461554.500000, false },  // clear-direct
  RetrogradeRow { Planet::URANUS,  2461740.500000, true  },  // clear-retrograde
  RetrogradeRow { Planet::NEPTUNE, 2460857.500000, false },  // station-before
  RetrogradeRow { Planet::NEPTUNE, 2460864.500000, true  },  // station-after
  RetrogradeRow { Planet::NEPTUNE, 2461016.500000, true  },  // station-before
  RetrogradeRow { Planet::NEPTUNE, 2461023.500000, false },  // station-after
  RetrogradeRow { Planet::NEPTUNE, 2460756.500000, false },  // clear-direct
  RetrogradeRow { Planet::NEPTUNE, 2461677.500000, true  },  // clear-retrograde
  RetrogradeRow { Planet::PLUTO,   2475827.500000, false },  // station-before
  RetrogradeRow { Planet::PLUTO,   2475834.500000, true  },  // station-after
  RetrogradeRow { Planet::PLUTO,   2475914.500000, true  },  // clear-retrograde
  RetrogradeRow { Planet::PLUTO,   2475992.500000, true  },  // station-before
  RetrogradeRow { Planet::PLUTO,   2475999.500000, false },  // station-after
  RetrogradeRow { Planet::PLUTO,   2476095.500000, false },  // clear-direct
};

// These source-selected Mercury and Pluto rows cross 0 degrees in opposite directions across their
// surrounding Horizons epochs, pinning the signed wrap independently of this implementation.
constexpr std::array HORIZONS_RETROGRADE_WRAP_ROWS {
  RetrogradeRow { Planet::MERCURY, 2460737.500000, false },
  RetrogradeRow { Planet::MERCURY, 2460764.500000, true  },
  RetrogradeRow { Planet::PLUTO,   2475819.000000, false },
  RetrogradeRow { Planet::PLUTO,   2475843.000000, true  },
};

// Retained data: v32-planets.
inline constexpr double MEEUS_EXAMPLE_33A_RIGHT_ASCENSION_DEG =
  (21.0 + (4.0 / 60.0) + (41.454 / 3600.0)) * 15.0;
inline constexpr double MEEUS_EXAMPLE_33A_DECLINATION_DEG = -(18.0 + (53.0 / 60.0) + (16.84 / 3600.0));
inline constexpr double MEEUS_EXAMPLE_37A_LONGITUDE_DEG = 232.74071;
inline constexpr double MEEUS_EXAMPLE_37A_LATITUDE_DEG = 14.58782;
inline constexpr double MEEUS_EXAMPLE_37A_RADIUS_AU = 29.711111;

// IMCCE PLUTO95 `pluto.doc` official Cartesian test vectors (SHA-256
// 98189416f7febbeecc2a1dd9bb61562338bc9fca3eda4dfe964fe32df5c0ba1b), re-derived through the
// `pluto.sub` evaluator (SHA-256 14d2e84e99c3fd873310ed5381a79d11f9140683830307e2b9ef051916244d24).
// One arcminute is a gross units/frame tripwire, not a model-error tolerance; the separately pinned
// retained maximum is 3.566762 arcsec.
// Retained data: v45-pluto95.
constexpr std::array PLUTO95_ROWS {
  Pluto95Row { 2415023.00, 10.29158303131287,  44.52906466047693, 10.79081191605171 },
  Pluto95Row { 2451548.25, -9.86615874601937, -27.98285304568784, -5.75779357947923 },
};

// PyMeeus 0.5.12 output collected 2026-09-22 by the same script and epochs. The pinned call is
// `<Planet>.geocentric_position(Epoch(jde))`, returning apparent equatorial coordinates. PyMeeus
// independently transcribes the complete VSOP87D/Meeus formulas, but evaluates corrections at the
// retarded epoch and uses the target's heliocentric latitude in the FK5 longitude term. This layer
// therefore diagnoses coarse formula/coefficient wiring rather than observation-epoch semantics or
// absolute physical accuracy.
// Retained data: v15-planets.
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

// Generated 2026-10-12 by statistics/planet_frame_reference.py: complete AA+ v2.55 D tables,
// independently parsed, with Cartesian Meeus (21.5)/(21.7) transport before Earth subtraction.
// Fixed-J2000 reconstruction checks the same model, not an independent physical ephemeris.
// Inputs: January 1 noon TT in 1885/2000/2099, first 2025 Horizons stations ±0.1 day,
// and directed Mercury/Neptune wraps. No random draws. Source hashes and regeneration:
// src/test/provenance/planet-frame/SOURCE_PINS.json and ATTRIBUTION.md.
// Tolerances pin the generator's discrete emission selection, not all epochs within its ULP stop criterion.
// They cover evaluator arithmetic, not model error: measured longitude/latitude/range gaps
// are 2.30e-10 deg / 3.10e-12 deg / 1.02e-12 AU; thresholds leave at least a 4x margin.
// These coefficient-derived reference values remain under their source terms, outside the project MIT grant.
constexpr Tolerance FRAME_TOLERANCE { .lon_deg = 1e-9, .lat_deg = 2e-11, .r_au = 5e-12 };
// NOLINTBEGIN(modernize-use-designated-initializers) - Dense golden rows read by column.
constexpr std::array FRAME_ROWS {
  // Planet           Observation JDE(TT) Longitude (deg) Latitude (deg)   Range (AU)
  FrameRow { Planet::MERCURY, 2409543.0000000000, 286.955716888680,  2.099756498773,  0.68659085874107 },
  FrameRow { Planet::MERCURY, 2451545.0000000000, 271.897748784375, -0.994771415570,  1.41546611511363 },
  FrameRow { Planet::MERCURY, 2487705.0000000000, 277.076294544108, -1.411235046273,  1.43897329257462 },
  FrameRow { Planet::MERCURY, 2460749.6828680001,   9.593475575409,  3.194679469858,  0.74132266990249 },
  FrameRow { Planet::MERCURY, 2460749.8828680003,   9.593488522732,  3.220740207583,  0.73696545423393 },
  FrameRow { Planet::VENUS,   2409543.0000000000, 251.362279956734,  1.301467327141,  1.38108383620354 },
  FrameRow { Planet::VENUS,   2451545.0000000000, 241.573298632070,  2.066502828315,  1.13757447626924 },
  FrameRow { Planet::VENUS,   2487705.0000000000, 235.620835589211,  2.905705739582,  0.82491893029578 },
  FrameRow { Planet::VENUS,   2460736.4258499998,  10.840202712413,  6.863712849184,  0.33769482562650 },
  FrameRow { Planet::VENUS,   2460736.6258500000,  10.840220529391,  6.897128082234,  0.33670295622526 },
  FrameRow { Planet::MARS,    2409543.0000000000, 290.762283266672, -1.024007458606,  2.36530774834023 },
  FrameRow { Planet::MARS,    2451545.0000000000, 327.970527156504, -1.067714631912,  1.84968388376187 },
  FrameRow { Planet::MARS,    2487705.0000000000, 240.352224395821,  0.322764229863,  2.19307427241787 },
  FrameRow { Planet::MARS,    2460730.4840589999, 107.010998224697,  3.695497738160,  0.82293428638369 },
  FrameRow { Planet::MARS,    2460730.6840590001, 107.011008174703,  3.689514863060,  0.82445203509061 },
  FrameRow { Planet::JUPITER, 2409543.0000000000, 155.636572655049,  1.100238240247,  4.74044271993939 },
  FrameRow { Planet::JUPITER, 2451545.0000000000,  25.255510235986, -1.262050295701,  4.62116318887566 },
  FrameRow { Planet::JUPITER, 2487705.0000000000, 173.195706539704,  1.232724750493,  5.02123190222427 },
  FrameRow { Planet::JUPITER, 2460710.8024189998,  71.275567734380, -0.480161377339,  4.59052232076517 },
  FrameRow { Planet::JUPITER, 2460711.0024190000,  71.275586232856, -0.479447650344,  4.59345769064073 },
  FrameRow { Planet::SATURN,  2409543.0000000000,  79.142907455066, -1.435770338982,  8.11652821777649 },
  FrameRow { Planet::SATURN,  2451545.0000000000,  40.396668988570, -2.444649223276,  8.65278650187802 },
  FrameRow { Planet::SATURN,  2487705.0000000000, 193.850207661249,  2.381541994510,  9.54765509115704 },
  FrameRow { Planet::SATURN,  2460869.5708479998,   1.931652624055, -2.291400040951,  9.19226492868109 },
  FrameRow { Planet::SATURN,  2460869.7708480000,   1.931633730116, -2.292308075125,  9.18912350328812 },
  FrameRow { Planet::URANUS,  2409543.0000000000, 182.920733619594,  0.748139129760, 18.13223734668615 },
  FrameRow { Planet::URANUS,  2451545.0000000000, 314.817876912522, -0.658287508703, 20.72718681440723 },
  FrameRow { Planet::URANUS,  2487705.0000000000,  13.782377744711, -0.656830107630, 19.93597404112344 },
  FrameRow { Planet::URANUS,  2460706.0827859999,  53.259498367732, -0.242515857171, 19.31433246336679 },
  FrameRow { Planet::URANUS,  2460706.2827860001,  53.259519012289, -0.242443763218, 19.31766889704254 },
  FrameRow { Planet::NEPTUNE, 2409543.0000000000,  50.758780156765, -1.781467215446, 29.18444354943705 },
  FrameRow { Planet::NEPTUNE, 2451545.0000000000, 303.202387180926,  0.234987111041, 31.02444616631808 },
  FrameRow { Planet::NEPTUNE, 2487705.0000000000, 165.067236475996,  0.907952213485, 29.75118605220414 },
  FrameRow { Planet::NEPTUNE, 2460861.2996100001,   2.174134724427, -1.327061660039, 29.68074087614835 },
  FrameRow { Planet::NEPTUNE, 2460861.4996100003,   2.174115928829, -1.327234450025, 29.67743672426809 },
  FrameRow { Planet::MERCURY, 2460737.0000000000, 358.678179698851,  0.555622226304,  1.07627758794121 },
  FrameRow { Planet::MERCURY, 2460738.0000000000,   0.184290040067,  0.780952211534,  1.04978393994182 },
  FrameRow { Planet::MERCURY, 2460764.0000000000,   0.450288335727,  2.248388122759,  0.59799227683132 },
  FrameRow { Planet::MERCURY, 2460765.0000000000, 359.717897759175,  2.001590225193,  0.60052672382706 },
  FrameRow { Planet::NEPTUNE, 2460764.0000000000, 359.968430531977, -1.263931511892, 30.87727120286948 },
  FrameRow { Planet::NEPTUNE, 2460765.0000000000,   0.005960786517, -1.264157096769, 30.87468364946210 },
};
// NOLINTEND(modernize-use-designated-initializers) - Dense golden rows read by column.

inline auto assert_horizons_rows(const std::span<const HorizonsRow> rows) -> void {
  for (const auto& row : rows) {
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
  ASSERT_NEAR(equatorial.α.deg(), MEEUS_EXAMPLE_33A_RIGHT_ASCENSION_DEG, 0.0075 / 3600.0);
  ASSERT_NEAR(equatorial.δ.deg(), MEEUS_EXAMPLE_33A_DECLINATION_DEG, 0.005 / 3600.0);
}

TEST(Planet, MeeusExample37a) {
  // Meeus Example 37.a, Pluto at JDE 2448908.5. Tolerances are half a printed unit.
  const auto coordinate = detail::pluto::heliocentric_j2000(2448908.5);
  ASSERT_NEAR(coordinate.λ.deg(), MEEUS_EXAMPLE_37A_LONGITUDE_DEG, 0.5e-5);
  ASSERT_NEAR(coordinate.β.deg(), MEEUS_EXAMPLE_37A_LATITUDE_DEG, 0.5e-5);
  ASSERT_NEAR(coordinate.r.au(), MEEUS_EXAMPLE_37A_RADIUS_AU, 0.5e-6);
}

TEST(Planet, Pluto95IndependentDirections) {
  constexpr double sine_obliquity = 0.397777156;
  constexpr double cosine_obliquity = 0.917482062;
  double maximum_separation_arcsec = 0.0;

  for (const auto& row : PLUTO95_ROWS) {
    const auto coordinate = detail::pluto::heliocentric_j2000(row.jde_tt);
    const double longitude = coordinate.λ.rad();
    const double latitude = coordinate.β.rad();
    const std::array chapter37 {
      coordinate.r.au() * std::cos(longitude) * std::cos(latitude),
      coordinate.r.au()
        * ((std::sin(longitude) * std::cos(latitude) * cosine_obliquity)
           - (std::sin(latitude) * sine_obliquity)),
      coordinate.r.au()
        * ((std::sin(longitude) * std::cos(latitude) * sine_obliquity)
           + (std::sin(latitude) * cosine_obliquity)),
    };
    const std::array official { row.x_au, row.y_au, row.z_au };
    const double chapter37_norm = std::hypot(chapter37[0], chapter37[1], chapter37[2]);
    const double official_norm = std::hypot(official[0], official[1], official[2]);
    const double dot = ((chapter37[0] * official[0]) + (chapter37[1] * official[1])
                        + (chapter37[2] * official[2]))
                     / (chapter37_norm * official_norm);
    const std::array cross {
      (chapter37[1] * official[2]) - (chapter37[2] * official[1]),
      (chapter37[2] * official[0]) - (chapter37[0] * official[2]),
      (chapter37[0] * official[1]) - (chapter37[1] * official[0]),
    };
    const double cross_norm = std::hypot(cross[0], cross[1], cross[2]) / (chapter37_norm * official_norm);
    const double separation_arcsec = astro::toolbox::rad_to_deg(std::atan2(cross_norm, dot)) * 3600.0;
    maximum_separation_arcsec = std::max(separation_arcsec, maximum_separation_arcsec);
    ASSERT_LT(separation_arcsec, 60.0) << "JDE " << row.jde_tt;
  }

  ASSERT_NEAR(maximum_separation_arcsec, 3.566761983698627, 5e-8);
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
  assert_horizons_rows(HORIZONS_ROWS);
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

TEST(Planet, PlutoHorizonsGoldenDataset) {
  assert_horizons_rows(PLUTO_HORIZONS_ROWS);
}

TEST(Planet, HorizonsRetrogradeDataset) {
  for (const auto& row : HORIZONS_RETROGRADE_ROWS) {
    ASSERT_EQ(geocentric_coord::is_retrograde(row.planet, row.jde), row.retrograde)
      << "JDE " << row.jde;
  }
}

TEST(Planet, RetrogradeAcrossLongitudeWrap) {
  for (const auto& row : HORIZONS_RETROGRADE_WRAP_ROWS) {
    ASSERT_EQ(geocentric_coord::is_retrograde(row.planet, row.jde), row.retrograde)
      << "JDE " << row.jde;
  }
}

TEST(Planet, LightTimeFixedPoint) {
  constexpr double jde_tt = 2451545.0;
  const auto earth = astro::earth::heliocentric_coord::vsop87d(jde_tt);
  const auto coordinate = detail::light_time_corrected(Planet::JUPITER, jde_tt, earth);
  const double retarded_jde_tt = jde_tt
                               - (astro::earth::aberration::LIGHT_TIME_DAYS_PER_AU * coordinate.r.au());
  const auto target_rect = detail::rectangular(
    detail::heliocentric(Planet::JUPITER, retarded_jde_tt, jde_tt)
  );
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
  const auto fk5 = astro::coords::fk5_correction(jde_tt, aberrated);
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

TEST(Planet, PlutoFrameJoinInvertsFk5) {
  constexpr double retarded_jde_tt = 2451544.8;
  constexpr double observation_jde_tt = 2451545.0;
  const auto j2000 = detail::pluto::heliocentric_j2000(retarded_jde_tt);
  const auto precessed = astro::earth::precession::ecliptic(
    j2000.λ,
    j2000.β,
    astro::julian_day::J2000,
    observation_jde_tt
  );
  const auto joined = detail::pluto::heliocentric_for_subtraction(retarded_jde_tt, observation_jde_tt);
  const auto correction = astro::coords::fk5_correction(observation_jde_tt, joined);

  ASSERT_LE(
    wrapped_diff_deg((joined.λ + correction.Δλ).normalize().deg(), precessed.λ.deg()),
    1e-13
  );
  ASSERT_NEAR((joined.β + correction.Δβ).deg(), precessed.β.deg(), 1e-13);
  ASSERT_DOUBLE_EQ(joined.r.au(), j2000.r.au());
}

TEST(Planet, PlutoModelAndPublicDomains) {
  const double raw_before = std::nextafter(
    detail::pluto::RAW_START_JDE_TT,
    -std::numeric_limits<double>::infinity()
  );
  const double raw_last = std::nextafter(
    detail::pluto::RAW_END_JDE_TT,
    detail::pluto::RAW_START_JDE_TT
  );
  EXPECT_THROW(static_cast<void>(detail::pluto::heliocentric_j2000(raw_before)), std::invalid_argument);
  EXPECT_NO_THROW(static_cast<void>(detail::pluto::heliocentric_j2000(detail::pluto::RAW_START_JDE_TT)));
  EXPECT_NO_THROW(static_cast<void>(detail::pluto::heliocentric_j2000(raw_last)));
  EXPECT_THROW(
    static_cast<void>(detail::pluto::heliocentric_j2000(detail::pluto::RAW_END_JDE_TT)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(detail::pluto::heliocentric_j2000(std::numeric_limits<double>::quiet_NaN())),
    std::invalid_argument
  );

  const double apparent_before = std::nextafter(
    detail::pluto::APPARENT_START_JDE_TT,
    -std::numeric_limits<double>::infinity()
  );
  const double apparent_last = std::nextafter(
    detail::pluto::APPARENT_END_JDE_TT,
    detail::pluto::APPARENT_START_JDE_TT
  );
  EXPECT_THROW(
    static_cast<void>(geocentric_coord::apparent(Planet::PLUTO, apparent_before)),
    std::invalid_argument
  );
  EXPECT_NO_THROW(
    static_cast<void>(geocentric_coord::apparent(Planet::PLUTO, detail::pluto::APPARENT_START_JDE_TT))
  );
  EXPECT_NO_THROW(static_cast<void>(geocentric_coord::apparent(Planet::PLUTO, apparent_last)));
  EXPECT_THROW(
    static_cast<void>(geocentric_coord::apparent(Planet::PLUTO, detail::pluto::APPARENT_END_JDE_TT)),
    std::invalid_argument
  );
}

TEST(Planet, PlutoCenteredRetrogradeDomain) {
  constexpr double start_jde_tt = 2409544.0;
  constexpr double end_jde_tt = 2488069.0;
  const double before = std::nextafter(
    start_jde_tt,
    -std::numeric_limits<double>::infinity()
  );
  const double last = std::nextafter(
    end_jde_tt,
    start_jde_tt
  );
  EXPECT_THROW(
    static_cast<void>(geocentric_coord::is_retrograde(Planet::PLUTO, before)),
    std::invalid_argument
  );
  EXPECT_NO_THROW(
    static_cast<void>(
      geocentric_coord::is_retrograde(Planet::PLUTO, start_jde_tt)
    )
  );
  EXPECT_NO_THROW(static_cast<void>(geocentric_coord::is_retrograde(Planet::PLUTO, last)));
  EXPECT_THROW(
    static_cast<void>(
      geocentric_coord::is_retrograde(Planet::PLUTO, end_jde_tt)
    ),
    std::invalid_argument
  );
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
  EXPECT_THROW(
    static_cast<void>(
      geocentric_coord::is_retrograde(Planet::MARS, std::numeric_limits<double>::quiet_NaN())
    ),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(geocentric_coord::is_retrograde(invalid_planet, 2451545.0)),
    std::invalid_argument
  );
}

TEST(Planet, IndependentObservationFrameGeometricDataset) {
  for (const auto& row : FRAME_ROWS) {
    const auto earth = astro::earth::heliocentric_coord::vsop87d(row.observation_jde_tt);
    const auto coordinate = detail::light_time_corrected(row.planet, row.observation_jde_tt, earth);

    ASSERT_LE(wrapped_diff_deg(coordinate.λ.deg(), row.lon_deg), FRAME_TOLERANCE.lon_deg)
      << name(row.planet) << " longitude at JDE(TT) " << row.observation_jde_tt;
    ASSERT_NEAR(coordinate.β.deg(), row.lat_deg, FRAME_TOLERANCE.lat_deg)
      << name(row.planet) << " latitude at JDE(TT) " << row.observation_jde_tt;
    ASSERT_NEAR(coordinate.r.au(), row.r_au, FRAME_TOLERANCE.r_au)
      << name(row.planet) << " range at JDE(TT) " << row.observation_jde_tt;
  }
}

TEST(Planet, ObservationFrameProducerPreservesRadiusAndEqualEpoch) {
  constexpr std::array planets {
    Planet::MERCURY, Planet::VENUS, Planet::MARS, Planet::JUPITER,
    Planet::SATURN, Planet::URANUS, Planet::NEPTUNE,
  };

  for (const Planet planet : planets) {
    for (const double jde_tt : std::array { 2409543.0, 2451545.0, 2487705.0 }) {
      const auto equal = detail::heliocentric(planet, jde_tt, jde_tt);
      const auto transported = detail::heliocentric(planet, jde_tt, jde_tt + 0.2);
      const auto raw = raw_heliocentric(planet, jde_tt);

      ASSERT_LE(wrapped_diff_deg(equal.λ.deg(), raw.λ.deg()), 1e-13)
        << name(planet) << " at JDE(TT) " << jde_tt;
      ASSERT_NEAR(equal.β.deg(), raw.β.deg(), 1e-13)
        << name(planet) << " at JDE(TT) " << jde_tt;
      ASSERT_DOUBLE_EQ(equal.r.au(), raw.r.au()) << name(planet) << " at JDE(TT) " << jde_tt;
      ASSERT_DOUBLE_EQ(transported.r.au(), raw.r.au()) << name(planet) << " at JDE(TT) " << jde_tt;
    }
  }
}

TEST(Planet, ObservationFrameProducerKeepsPlutoJoin) {
  for (const double observation_jde_tt : std::array { 2409543.5, 2451545.0, 2488069.0 }) {
    const double retarded_jde_tt = observation_jde_tt - 0.2;
    const auto expected = detail::pluto::heliocentric_for_subtraction(retarded_jde_tt, observation_jde_tt);
    const auto actual = detail::heliocentric(Planet::PLUTO, retarded_jde_tt, observation_jde_tt);

    ASSERT_DOUBLE_EQ(actual.λ.deg(), expected.λ.deg()) << "longitude at JDE(TT) " << observation_jde_tt;
    ASSERT_DOUBLE_EQ(actual.β.deg(), expected.β.deg()) << "latitude at JDE(TT) " << observation_jde_tt;
    ASSERT_DOUBLE_EQ(actual.r.au(), expected.r.au()) << "range at JDE(TT) " << observation_jde_tt;
  }
}

} // namespace astro::planet::test
