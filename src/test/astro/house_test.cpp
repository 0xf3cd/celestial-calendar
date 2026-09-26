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
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

#include "coord_transform.hpp"
#include "earth.hpp"
#include "geolocation.hpp"
#include "house.hpp"
#include "rise_set.hpp"
#include "toolbox.hpp"

namespace astro::house::test {

using astro::toolbox::AngleDeg;

namespace {

constexpr double FORMULA_TOLERANCE_DEG = 3e-6;
constexpr double DATE_TOLERANCE_DEG = 1e-4;

struct FormulaCase {
  std::string_view name;
  System system;
  double armc_deg;
  double latitude_deg;
  double obliquity_deg;
  double ascendant_deg;
  double midheaven_deg;
  std::array<double, 12> cusps_deg;
};

struct DateCase {
  std::string_view name;
  System system;
  double jd_ut1;
  double jde_tt;
  double latitude_deg;
  double longitude_deg;
  double ascendant_deg;
  double midheaven_deg;
  std::array<double, 12> cusps_deg;
};

[[nodiscard]] auto angular_difference(const double lhs, const double rhs) -> double {
  return std::fabs(std::remainder(lhs - rhs, 360.0));
}

} // anonymous namespace

static_assert(std::is_same_v<astro::rise_set::GeoLocation, astro::GeoLocation>);

TEST(House, AscendantMeeus14a) {
  // Meeus Example 14.a prints the eastern ecliptic/horizon intersection as 169°21' and the
  // antipodal setting point as 349°21'; the printed precision allows half an arcminute.
  const auto ascendant = detail::ascendant(AngleDeg { 75.0 }, AngleDeg { 51.0 }, AngleDeg { 23.44 });
  ASSERT_NEAR(ascendant.deg(), 169.0 + (21.0 / 60.0), 0.5 / 60.0);
}

TEST(House, SwissFormulaLevel) {
  // Generated on 2026-09-25 by statistics/house_swiss_crawler.py from Swiss Ephemeris
  // v2.10.3bfinal `swe_houses_armc_ex2`. The separate 3e-6° tolerance covers the reference
  // solver's loss of precision immediately inside the Placidus polar limit.
  // NOLINTBEGIN(modernize-use-designated-initializers): positional columns mirror the crawler output.
  const std::array dataset {
    FormulaCase { "meeus-14",            System::EQUAL,        75.0000000000,  51.0000000000, 23.4400000000, 169.358304802347,  76.188453109242, { 169.358304802347, 199.358304802347, 229.358304802347, 259.358304802347, 289.358304802347, 319.358304802347, 349.358304802347,  19.358304802347,  49.358304802347,  79.358304802347, 109.358304802347, 139.358304802347 } },
    FormulaCase { "meeus-14",            System::WHOLE_SIGN,   75.0000000000,  51.0000000000, 23.4400000000, 169.358304802347,  76.188453109242, { 150.000000000000, 180.000000000000, 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000,   0.000000000000,  30.000000000000,  60.000000000000,  90.000000000000, 120.000000000000 } },
    FormulaCase { "meeus-14",            System::PLACIDUS,     75.0000000000,  51.0000000000, 23.4400000000, 169.358304802347,  76.188453109242, { 169.358304802347, 192.034797538214, 220.933748396566, 256.188453109242, 292.881090430665, 324.160283058891, 349.358304802347,  12.034797538214,  40.933748396566,  76.188453109242, 112.881090430665, 144.160283058891 } },
    FormulaCase { "southern",            System::EQUAL,       123.0000000000, -33.8688000000, 23.4392911000, 227.305346515834, 120.787323771477, { 227.305346515834, 257.305346515834, 287.305346515834, 317.305346515834, 347.305346515834,  17.305346515834,  47.305346515834,  77.305346515834, 107.305346515834, 137.305346515834, 167.305346515834, 197.305346515834 } },
    FormulaCase { "southern",            System::WHOLE_SIGN,  123.0000000000, -33.8688000000, 23.4392911000, 227.305346515834, 120.787323771477, { 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000,   0.000000000000,  30.000000000000,  60.000000000000,  90.000000000000, 120.000000000000, 150.000000000000, 180.000000000000 } },
    FormulaCase { "southern",            System::PLACIDUS,    123.0000000000, -33.8688000000, 23.4392911000, 227.305346515834, 120.787323771477, { 227.305346515834, 255.092181735632, 277.875988585158, 300.787323771477, 328.059243246997,   4.055124459259,  47.305346515834,  75.092181735632,  97.875988585158, 120.787323771477, 148.059243246997, 184.055124459259 } },
    FormulaCase { "armc-wrap",           System::EQUAL,       359.9000000000,   0.0000000000, 23.4392911000,  89.908251779045, 359.891006064271, {  89.908251779045, 119.908251779045, 149.908251779045, 179.908251779045, 209.908251779045, 239.908251779045, 269.908251779045, 299.908251779045, 329.908251779045, 359.908251779045,  29.908251779045,  59.908251779045 } },
    FormulaCase { "armc-wrap",           System::WHOLE_SIGN,  359.9000000000,   0.0000000000, 23.4392911000,  89.908251779045, 359.891006064271, {  60.000000000000,  90.000000000000, 120.000000000000, 150.000000000000, 180.000000000000, 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000,   0.000000000000,  30.000000000000 } },
    FormulaCase { "armc-wrap",           System::PLACIDUS,    359.9000000000,   0.0000000000, 23.4392911000,  89.908251779045, 359.891006064271, {  89.908251779045, 117.815034738398, 147.714652976104, 179.891006064271, 212.077143059379, 241.993911375181, 269.908251779045, 297.815034738398, 327.714652976104, 359.891006064271,  32.077143059379,  61.993911375181 } },
    FormulaCase { "near-polar-north",    System::PLACIDUS,    276.9400000000,  66.5000000000, 23.4400000000,  91.941491363794, 276.372211073616, {  91.941491363794,  93.033796867674,  94.394426330861,  96.372211073616,  99.948786226864, 110.000757535689, 271.941491363794, 273.033796867674, 274.394426330861, 276.372211073616, 279.948786226864, 290.000757535689 } },
    FormulaCase { "near-polar-south",    System::PLACIDUS,     96.9400000000, -66.5000000000, 23.4400000000, 271.941491363794,  96.372211073616, { 271.941491363794, 273.033796867674, 274.394426330861, 276.372211073616, 279.948786226864, 290.000757535689,  91.941491363794,  93.033796867674,  94.394426330861,  96.372211073616,  99.948786226864, 110.000757535689 } },
    FormulaCase { "high-latitude-north", System::EQUAL,       183.0000000000,  89.0000000000, 23.4400000000,   2.514394277895, 183.269275002642, {   2.514394277895,  32.514394277895,  62.514394277895,  92.514394277895, 122.514394277895, 152.514394277895, 182.514394277895, 212.514394277895, 242.514394277895, 272.514394277895, 302.514394277895, 332.514394277895 } },
    FormulaCase { "high-latitude-north", System::WHOLE_SIGN,  183.0000000000,  89.0000000000, 23.4400000000,   2.514394277895, 183.269275002642, {   0.000000000000,  30.000000000000,  60.000000000000,  90.000000000000, 120.000000000000, 150.000000000000, 180.000000000000, 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000 } },
    FormulaCase { "high-latitude-south", System::EQUAL,         3.0000000000, -89.0000000000, 23.4400000000, 182.514394277895,   3.269275002642, { 182.514394277895, 212.514394277895, 242.514394277895, 272.514394277895, 302.514394277895, 332.514394277895,   2.514394277895,  32.514394277895,  62.514394277895,  92.514394277895, 122.514394277895, 152.514394277895 } },
    FormulaCase { "high-latitude-south", System::WHOLE_SIGN,    3.0000000000, -89.0000000000, 23.4400000000, 182.514394277895,   3.269275002642, { 180.000000000000, 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000,   0.000000000000,  30.000000000000,  60.000000000000,  90.000000000000, 120.000000000000, 150.000000000000 } },
    FormulaCase { "whole-zero-boundary", System::WHOLE_SIGN,  270.0000000000,  60.0000000000, 23.4400000000,   0.000000000100, 270.000000000000, {   0.000000000000,  30.000000000000,  60.000000000000,  90.000000000000, 120.000000000000, 150.000000000000, 180.000000000000, 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000 } },
  };
  // NOLINTEND(modernize-use-designated-initializers)

  for (const auto& row : dataset) {
    SCOPED_TRACE(row.name);
    const auto actual = detail::calculate(
      AngleDeg { row.armc_deg },
      AngleDeg { row.latitude_deg },
      AngleDeg { row.obliquity_deg },
      row.system
    );
    ASSERT_NEAR(angular_difference(actual.ascendant.deg(), row.ascendant_deg), 0.0, FORMULA_TOLERANCE_DEG);
    ASSERT_NEAR(angular_difference(actual.midheaven.deg(), row.midheaven_deg), 0.0, FORMULA_TOLERANCE_DEG);
    for (std::size_t index = 0; index < actual.cusps.size(); ++index) {
      ASSERT_NEAR(
        angular_difference(actual.cusps.at(index).deg(), row.cusps_deg.at(index)),
        0.0,
        FORMULA_TOLERANCE_DEG
      );
    }
  }
}

TEST(House, SwissDateLevel) {
  // Generated on 2026-09-25 by statistics/house_swiss_crawler.py from Swiss Ephemeris
  // v2.10.3bfinal `swe_houses_ex2`. The 1e-4° tolerance covers the different apparent sidereal-time
  // and nutation models while remaining far below the errors from a UT1/TT or longitude-sign swap.
  // NOLINTBEGIN(modernize-use-designated-initializers): positional columns mirror the crawler output.
  const std::array dataset {
    DateCase { "j2000-greenwich",      System::EQUAL,       2451545.0000000000, 2451545.0007387605,  51.5000000000,    0.0000000000,  24.287305086198, 279.611087800285, {  24.287305086198,  54.287305086198,  84.287305086198, 114.287305086198, 144.287305086198, 174.287305086198, 204.287305086198, 234.287305086198, 264.287305086198, 294.287305086198, 324.287305086198, 354.287305086198 } },
    DateCase { "j2000-greenwich",      System::WHOLE_SIGN,  2451545.0000000000, 2451545.0007387605,  51.5000000000,    0.0000000000,  24.287305086198, 279.611087800285, {   0.000000000000,  30.000000000000,  60.000000000000,  90.000000000000, 120.000000000000, 150.000000000000, 180.000000000000, 210.000000000000, 240.000000000000, 270.000000000000, 300.000000000000, 330.000000000000 } },
    DateCase { "j2000-greenwich",      System::PLACIDUS,    2451545.0000000000, 2451545.0007387605,  51.5000000000,    0.0000000000,  24.287305086198, 279.611087800285, {  24.287305086198,  61.161799412691,  82.030012128933,  99.611087800285, 119.053371520496, 147.687375204715, 204.287305086198, 241.161799412691, 262.030012128933, 279.611087800285, 299.053371520496, 327.687375204715 } },
    DateCase { "meeus-13b-washington", System::PLACIDUS,    2446896.3062499999, 2446896.3068918549,  38.9213888889,  -77.0655555556, 149.215242085214,  54.046417382821, { 149.215242085214, 172.643681339771, 201.041979113621, 234.046417382821, 268.625166293827, 300.834861135264, 329.215242085214, 352.643681339771,  21.041979113621,  54.046417382821,  88.625166293827, 120.834861135264 } },
    DateCase { "j2000-sydney",         System::PLACIDUS,    2451545.0000000000, 2451545.0007387605, -33.8688000000,  151.2093000000, 152.488909139281,  73.089186823627, { 152.488909139281, 195.677567269628, 228.131549399578, 253.089186823627, 275.563408529259, 299.979438157141, 332.488909139281,  15.677567269628,  48.131549399578,  73.089186823627,  95.563408529259, 119.979438157141 } },
    DateCase { "east-date-line",       System::PLACIDUS,    2461041.5000000000, 2461041.5007974519,  35.0000000000,  179.9000000000,  16.384568398230, 279.708040990102, {  16.384568398230,  51.871318063133,  77.373611104101,  99.708040990102, 123.504731500787, 153.898599937039, 196.384568398230, 231.871318063133, 257.373611104101, 279.708040990102, 303.504731500787, 333.898599937039 } },
    DateCase { "west-date-line",       System::PLACIDUS,    2461041.5000000000, 2461041.5007974519,  35.0000000000, -179.9000000000,  16.689296583904, 279.892538251926, {  16.689296583904,  52.098550056418,  77.563466671451,  99.892538251926, 123.711455212670, 154.158332645255, 196.689296583904, 232.098550056418, 257.563466671451, 279.892538251926, 303.711455212670, 334.158332645255 } },
  };
  // NOLINTEND(modernize-use-designated-initializers)

  for (const auto& row : dataset) {
    SCOPED_TRACE(row.name);
    const auto actual = calculate(
      row.jd_ut1,
      row.jde_tt,
      astro::GeoLocation {
        .latitude = AngleDeg { row.latitude_deg },
        .longitude = AngleDeg { row.longitude_deg },
      },
      row.system
    );
    ASSERT_NEAR(angular_difference(actual.ascendant.deg(), row.ascendant_deg), 0.0, DATE_TOLERANCE_DEG);
    ASSERT_NEAR(angular_difference(actual.midheaven.deg(), row.midheaven_deg), 0.0, DATE_TOLERANCE_DEG);
    for (std::size_t index = 0; index < actual.cusps.size(); ++index) {
      ASSERT_NEAR(
        angular_difference(actual.cusps.at(index).deg(), row.cusps_deg.at(index)),
        0.0,
        DATE_TOLERANCE_DEG
      );
    }
  }
}

TEST(House, PrincipalAnglesMatchCoordinateGeometry) {
  struct GeometryCase {
    double armc_deg;
    double latitude_deg;
  };
  const std::array cases {
    GeometryCase { .armc_deg = 75.0,  .latitude_deg = 51.0 },
    GeometryCase { .armc_deg = 183.0, .latitude_deg = 89.0 },
    GeometryCase { .armc_deg = 3.0,   .latitude_deg = -89.0 },
  };
  const AngleDeg obliquity { 23.44 };

  for (const auto& row : cases) {
    const AngleDeg armc { row.armc_deg };
    const AngleDeg latitude { row.latitude_deg };
    const auto result = detail::calculate(armc, latitude, obliquity, System::EQUAL);
    const auto mc_equatorial = astro::coords::ecliptic_to_equatorial(
      result.midheaven,
      AngleDeg { 0.0 },
      obliquity
    );
    ASSERT_NEAR(angular_difference(mc_equatorial.α.deg(), armc.deg()), 0.0, 1e-12);

    const auto asc_equatorial = astro::coords::ecliptic_to_equatorial(
      result.ascendant,
      AngleDeg { 0.0 },
      obliquity
    );
    const auto asc_hour_angle = (armc - asc_equatorial.α).normalize();
    const auto asc_horizontal = astro::coords::equatorial_to_horizontal(
      asc_hour_angle,
      asc_equatorial.δ,
      latitude
    );
    ASSERT_NEAR(asc_horizontal.h.deg(), 0.0, 1e-12);
    ASSERT_GT(asc_hour_angle.deg(), 180.0); // The eastern, rising intersection.
  }
}

TEST(House, CuspsHonorSystemStructure) {
  const auto equal = detail::calculate(AngleDeg { 75.0 }, AngleDeg { 51.0 }, AngleDeg { 23.44 }, System::EQUAL);
  const auto whole = detail::calculate(AngleDeg { 75.0 }, AngleDeg { 51.0 }, AngleDeg { 23.44 }, System::WHOLE_SIGN);
  const auto placidus = detail::calculate(
    AngleDeg { 75.0 },
    AngleDeg { 51.0 },
    AngleDeg { 23.44 },
    System::PLACIDUS
  );

  for (std::size_t index = 0; index < 12; ++index) {
    ASSERT_NEAR(
      angular_difference(equal.cusps.at(index).deg(), equal.ascendant.deg() + (30.0 * index)),
      0.0,
      1e-12
    );
    ASSERT_NEAR(std::remainder(whole.cusps.at(index).deg(), 30.0), 0.0, 1e-12);
  }
  for (const Result* result : { &equal, &whole, &placidus }) {
    for (std::size_t index = 0; index < 6; ++index) {
      ASSERT_NEAR(
        angular_difference(result->cusps.at(index + 6).deg(), result->cusps.at(index).deg() + 180.0),
        0.0,
        1e-11
      );
    }
  }
}

TEST(House, WholeSignBoundaryBelongsToFollowingSign) {
  for (std::size_t index = 0; index < 12; ++index) {
    const double boundary_deg = 30.0 * static_cast<double>(index);
    SCOPED_TRACE(boundary_deg);
    const double preceding_deg = astro::toolbox::normalize_deg(boundary_deg - 30.0);
    ASSERT_NEAR(detail::whole_sign_start(AngleDeg { boundary_deg - 0.000001 }).deg(), preceding_deg, 1e-12);
    ASSERT_NEAR(detail::whole_sign_start(AngleDeg { boundary_deg }).deg(), boundary_deg, 1e-12);
    ASSERT_NEAR(detail::whole_sign_start(AngleDeg { boundary_deg + 0.000001 }).deg(), boundary_deg, 1e-12);
  }
  ASSERT_NEAR(detail::whole_sign_start(AngleDeg { 360.0 }).deg(), 0.0, 1e-12);

  const auto exact_zero = detail::calculate(
    AngleDeg { 270.0 },
    AngleDeg { 60.0 },
    AngleDeg { 23.44 },
    System::WHOLE_SIGN
  );
  ASSERT_EQ(exact_zero.ascendant.deg(), 0.0);
  ASSERT_EQ(exact_zero.cusps.front().deg(), 0.0);
}

TEST(House, PlacidusRootsHandleLongitudeSeam) {
  struct SeamCase {
    detail::PlacidusCusp cusp;
    double armc_deg;
  };
  const std::array cases {
    SeamCase { .cusp = detail::PlacidusCusp::TWO,     .armc_deg = 240.0 },
    SeamCase { .cusp = detail::PlacidusCusp::THREE,   .armc_deg = 210.0 },
    SeamCase { .cusp = detail::PlacidusCusp::ELEVEN,  .armc_deg = 330.0 },
    SeamCase { .cusp = detail::PlacidusCusp::TWELVE,  .armc_deg = 300.0 },
  };
  for (const auto& row : cases) {
    const auto cusp = detail::placidus_cusp(
      AngleDeg { row.armc_deg },
      AngleDeg { 0.0 },
      AngleDeg { 23.44 },
      row.cusp
    );
    ASSERT_NEAR(cusp.deg(), 0.0, 1e-12);
  }
}

TEST(House, RejectsInvalidInputs) {
  constexpr double jd_ut1 = 2451545.0;
  constexpr double jde_tt = 2451545.0007387605;
  const auto location = [](const double latitude_deg, const double longitude_deg) {
    return astro::GeoLocation {
      .latitude = AngleDeg { latitude_deg },
      .longitude = AngleDeg { longitude_deg },
    };
  };

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): exercises the invalid-enumerator contract.
  const auto invalid_system = static_cast<System>(255);
  EXPECT_THROW(
    static_cast<void>(calculate(nan, jde_tt, location(0.0, 0.0), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, infinity, location(0.0, 0.0), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(nan, 0.0), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(0.0, infinity), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(0.0, 180.000001), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(0.0, -180.000001), System::EQUAL)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, location(0.0, 0.0), invalid_system)),
    std::invalid_argument
  );

  for (const System system : { System::EQUAL, System::WHOLE_SIGN, System::PLACIDUS }) {
    EXPECT_THROW(
      static_cast<void>(calculate(jd_ut1, jde_tt, location(90.0, 0.0), system)),
      std::invalid_argument
    );
    EXPECT_THROW(
      static_cast<void>(calculate(jd_ut1, jde_tt, location(-90.0, 0.0), system)),
      std::invalid_argument
    );
  }
}

TEST(House, PlacidusUsesTrueObliquityDomain) {
  constexpr double jd_ut1 = 2451545.0;
  constexpr double jde_tt = 2451545.0007387605;
  const auto true_obliquity = astro::earth::obliquity::true_obliquity(jde_tt);
  const auto mean_obliquity = astro::earth::obliquity::mean(jde_tt);
  const double true_boundary_deg = 90.0 - true_obliquity.deg();
  const double between_boundaries_deg = 90.0 - ((true_obliquity.deg() + mean_obliquity.deg()) / 2.0);

  const auto at_boundary = astro::GeoLocation {
    .latitude = AngleDeg { true_boundary_deg },
    .longitude = AngleDeg { 0.0 },
  };
  const auto between_boundaries = astro::GeoLocation {
    .latitude = AngleDeg { between_boundaries_deg },
    .longitude = AngleDeg { 0.0 },
  };

  EXPECT_THROW(
    static_cast<void>(calculate(jd_ut1, jde_tt, at_boundary, System::PLACIDUS)),
    std::invalid_argument
  );
  EXPECT_THROW(
    static_cast<void>(calculate(
      jd_ut1,
      jde_tt,
      astro::GeoLocation {
        .latitude = AngleDeg { true_boundary_deg + 0.01 },
        .longitude = AngleDeg { 0.0 },
      },
      System::PLACIDUS
    )),
    std::invalid_argument
  );
  ASSERT_NO_THROW(static_cast<void>(calculate(jd_ut1, jde_tt, between_boundaries, System::PLACIDUS)));
}

TEST(House, PlacidusReportsNonConvergence) {
  EXPECT_THROW(
    static_cast<void>(
      detail::placidus_cusp(
        AngleDeg { 75.0 },
        AngleDeg { 51.0 },
        AngleDeg { 23.44 },
        detail::PlacidusCusp::TWO,
        0
      )
    ),
    std::runtime_error
  );
}

} // namespace astro::house::test
